#include "task_queue_win.h"
#include "../platform_thread.h"
#include <chrono>
#include <assert.h>
#include <queue>
#include <algorithm>
#include "../event.h"

#pragma comment(lib, "Winmm.lib")
namespace dd {

#define  WM_RUN_TASK WM_USER + 1
#define WM_QUEUE_DELAYED_TASK WM_USER + 2

	void CALLBACK InitializeQueueThread(ULONG_PTR param)
	{
		MSG msg;
		::PeekMessage(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
		Event *p = reinterpret_cast<Event*>(param);
		p->Set();
	}

	ThreadPriority TaskQueuePriorityToThreadPriority(TaskQueueFactory::Priority priority)
	{
		switch (priority)
		{
		case TaskQueueFactory::Priority::HIGH:
			return kRealtimePriority;
		case TaskQueueFactory::Priority::LOW:
			return kLowPriority;
		case TaskQueueFactory::Priority::NORMAL:
			return kNormalPriority;
		}
		return kNormalPriority;
	}

	int64_t GetTick()
	{
		static const UINT kPeriod = 1;
		// 设置精度为1毫秒
		bool high_res = (timeBeginPeriod(kPeriod) == TIMERR_NOERROR);
		int64_t ret = GetCurrentTick();
		if (high_res)
			timeEndPeriod(kPeriod);// timeBeginPeriod/timeEndPeroid需要成对出现
		return ret;
	}

	class DelayedTaskInfo
	{
	public:
		DelayedTaskInfo() {}
		DelayedTaskInfo(uint32_t millseconds, std::unique_ptr<QueuedTask> task)
			:_due_time(GetTick() + millseconds), _task(std::move(task)) {}
		DelayedTaskInfo(DelayedTaskInfo&&) = default;

		bool operator>(const DelayedTaskInfo& other) const
		{
			return _due_time > other._due_time;
		}
		DelayedTaskInfo& operator= (DelayedTaskInfo&& other) = default;

		void Run() const
		{
			assert(_due_time);
			_task->Run() ? _task.reset() : static_cast<void>(_task.release());
		}

		int64_t due_time() const { return _due_time; }
	private:
		int64_t _due_time = 0;
		mutable std::unique_ptr<QueuedTask> _task;
	};

	class MultimediaTimer
	{
	public:
		MultimediaTimer() : _event(::CreateEvent(nullptr, true, false, nullptr)) {}
		~MultimediaTimer()
		{
			Cancel();
			::CloseHandle(_event);
		}

		bool StartOneShotTimer(UINT delay_ms)
		{
			assert(_timer_id == 0);
			assert(_event != nullptr);
			_timer_id = ::timeSetEvent(delay_ms, 0, reinterpret_cast<LPTIMECALLBACK>(_event), 0,
				TIME_ONESHOT | TIME_CALLBACK_EVENT_SET);
			return _timer_id != 0;
		}

		void Cancel()
		{
			if (_timer_id)
			{
				::timeKillEvent(_timer_id);
				_timer_id = 0;
			}
			::ResetEvent(_event);
		}

		HANDLE* event_for_wait() { return &_event; }

	private:
		HANDLE _event = nullptr;
		MMRESULT _timer_id = 0;
	};

	class TaskQueueWin : public TaskQueueBase
	{
	public:
		TaskQueueWin(const std::string queue_name, ThreadPriority priority);
		~TaskQueueWin() override = default;

		void Delete() override;
		void PostTask(std::unique_ptr<QueuedTask> task) override;
		void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds) override;

		void RunPendingTasks();

	private:
		static void ThreadMain(void* context);

		class WorkThread : public PlatformThread
		{
		public:
			WorkThread(ThreadRunFunction func,
				void *obj,
				const std::string &thread_name,
				ThreadPriority priorite)
				:PlatformThread(func, obj, thread_name, priorite) {}

			bool QueueAPC(PAPCFUNC apc_function, ULONG_PTR data)
			{
				return PlatformThread::QueueAPC(apc_function, data);
			}
		};

		void RunThreadMain();
		bool ProcessQueuedMessages();
		void RunDueTasks();
		void ScheduleNextTimer();
		void CancelTimers();
		void DrainTasks();

		template <typename T>
		struct greater
		{
			bool operator()(const T& l, const T& r) { return l > r; }
		};

		MultimediaTimer _timer;
		std::priority_queue<DelayedTaskInfo,
			std::vector<DelayedTaskInfo>,
			greater<DelayedTaskInfo>> _timer_tasks;
		UINT_PTR _timer_id = 0;
		WorkThread _thread;
		Lock _pending_lock;
		std::queue<std::unique_ptr<QueuedTask>> _pending;
		HANDLE _in_queue;

	};

	TaskQueueWin::TaskQueueWin(const std::string queue_name, ThreadPriority priority)
		: _thread(&TaskQueueWin::ThreadMain, this, queue_name, priority),
		_in_queue(::CreateEvent(nullptr, true, false, nullptr))
	{
		assert(_in_queue);
		_thread.Start();
		Event e;
		_thread.QueueAPC(&InitializeQueueThread, reinterpret_cast<ULONG_PTR>(&e));
		e.Wait(-1);
	}

	void TaskQueueWin::Delete()
	{
		assert(!IsCurrent());
		while (!::PostThreadMessage(_thread.GetThreadRef(), WM_QUIT, 0, 0))
		{
			assert(ERROR_NOT_ENOUGH_QUOTA == ::GetLastError());
			Sleep(1);
		}
		_thread.Stop();
		::CloseHandle(_in_queue);
		delete this;
	}

	void TaskQueueWin::PostTask(std::unique_ptr<QueuedTask> task)
	{
		AutoLock lock(_pending_lock);
		_pending.push(std::move(task));
		::SetEvent(_in_queue);
	}

	void TaskQueueWin::PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds)
	{
		if (!milliseconds)
		{
			PostTask(std::move(task));
			return;
		}

		auto *task_info = new DelayedTaskInfo(milliseconds, std::move(task));
		if (!::PostThreadMessage(_thread.GetThreadRef(), WM_QUEUE_DELAYED_TASK, 0,
			reinterpret_cast<LPARAM>(task_info)))
			delete task_info;
	}

	void TaskQueueWin::RunPendingTasks()
	{
		while (true)
		{
			std::unique_ptr<QueuedTask> task;
			{
				AutoLock lock(_pending_lock);
				if(_pending.empty())
					break;
				task = std::move(_pending.front());
				_pending.pop();
			}
			if (!task->Run())
				task.release();
		}
	}

	void TaskQueueWin::ThreadMain(void* context)
	{
		static_cast<TaskQueueWin*>(context)->RunThreadMain();
	}

	void TaskQueueWin::RunThreadMain()
	{
		CurrentTaskQueueSetter set_current(this);
		HANDLE handles[2] = { *_timer.event_for_wait(), _in_queue };
		while (true)
		{
			DWORD result = ::MsgWaitForMultipleObjectsEx(ARRAYSIZE(handles), handles, INFINITE, QS_ALLEVENTS,
				MWMO_ALERTABLE);
			assert(WAIT_FAILED != result);
			if (result == (WAIT_OBJECT_0 + 2))
			{
				if (!ProcessQueuedMessages())
				{
					DrainTasks();
					break;
				}
			}

			if (result == WAIT_OBJECT_0 ||
				(!_timer_tasks.empty() &&
					::WaitForSingleObject(*_timer.event_for_wait(), 0) == WAIT_OBJECT_0))
			{
				_timer.Cancel();
				RunDueTasks();
				ScheduleNextTimer();
			}

			if (result == (WAIT_OBJECT_0 + 1)) {
				::ResetEvent(_in_queue);
				RunPendingTasks();
			}
		}
	}

	bool TaskQueueWin::ProcessQueuedMessages()
	{
		MSG msg = {};
		static const int kMaxTaskProcessingTimeMs = 500;
		auto start = GetTick();
		while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) && msg.message != WM_QUIT)
		{
			if (!msg.hwnd)
			{
				switch (msg.message)
				{
				case WM_RUN_TASK:
					// 不使用
					break;
				case WM_QUEUE_DELAYED_TASK:
				{
					std::unique_ptr<DelayedTaskInfo> info(reinterpret_cast<DelayedTaskInfo*>(msg.lParam));
					bool need_to_schedule_timers =
						_timer_tasks.empty() ||
						_timer_tasks.top().due_time() > info->due_time();
					_timer_tasks.emplace(std::move(*info.get()));
					if (need_to_schedule_timers) {
						CancelTimers();
						ScheduleNextTimer();
					}
				break;
				}
				case WM_TIMER: {
					assert(_timer_id == msg.wParam);
					::KillTimer(nullptr, msg.wParam);
					_timer_id = 0;
					RunDueTasks();
					ScheduleNextTimer();
					break;
				}

				default:
					break;
				}
			}
			else {
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}

			if(GetTick() > start + kMaxTaskProcessingTimeMs)
				break;
		}
		return msg.message != WM_QUIT;
	}

	void TaskQueueWin::RunDueTasks()
	{
		assert(!_timer_tasks.empty());
		auto now = GetTick();
		do {
			const auto& top = _timer_tasks.top();
			if(top.due_time() > now)
				break;;
			top.Run();
			_timer_tasks.pop();
		} while (!_timer_tasks.empty());
	}

	void TaskQueueWin::ScheduleNextTimer()
	{
		assert(_timer_id == 0);
		if (_timer_tasks.empty())
			return;
		const auto& next_task = _timer_tasks.top();
		int64_t delay_msg = std::max<int>(0, next_task.due_time() - GetTick());
		uint32_t milliseconds = delay_msg;
		if (!_timer.StartOneShotTimer(milliseconds))
			_timer_id = ::SetTimer(nullptr, 0, milliseconds, nullptr);
	}

	void TaskQueueWin::CancelTimers()
	{
		_timer.Cancel();
		if (_timer_id) {
			::KillTimer(nullptr, _timer_id);
			_timer_id = 0;
		}
	}

	void TaskQueueWin::DrainTasks()
	{
		//   线程退出，清空所有任务
		RunPendingTasks();
		while (!_timer_tasks.empty()) {
			const auto & task = _timer_tasks.top();
			task.Run();
			_timer_tasks.pop();
		}
	}

	class TaskQueueWinFactory : public TaskQueueFactory {
	public:
		std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
			const std::string &name,
			Priority priority) const override {
			return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
				new TaskQueueWin(name, TaskQueuePriorityToThreadPriority(priority)));
		}
	};
	std::unique_ptr<TaskQueueFactory> CreateTaskQueueWinFactory()
	{
		return std::make_unique<TaskQueueWinFactory>();
	}
}// namespace dd
