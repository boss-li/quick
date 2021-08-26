#include "task_queue_stdlib.h"
#include <string.h>
#include <algorithm>
#include <map>
#include <memory>
#include <queue>
#include <utility>
#include "../queued_task.h"
#include "../task_queue_base.h"
#include <assert.h>
#include "../event.h"
#include "../utility.h"
#include "../platform_thread.h"
#include <chrono>

namespace dd {

	ThreadPriority TaskQueuePriorityToThreadPriority(TaskQueueFactory::Priority priority)
	{
		switch (priority) {
		case TaskQueueFactory::Priority::HIGH:
			return kRealtimePriority;
		case TaskQueueFactory::Priority::LOW:
			return kLowPriority;
		case TaskQueueFactory::Priority::NORMAL:
			return kNormalPriority;
		default:
			return kNormalPriority;
		}
	}

	class TaskQueueStdlib final : public TaskQueueBase {
	public:
		TaskQueueStdlib(const std::string &queue_name, ThreadPriority priority);
		~TaskQueueStdlib() override = default;

		void Delete() override;
		void PostTask(std::unique_ptr<QueuedTask> task) override;
		void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds) override;

	private:
		using OrderId = uint64_t;
		struct DelayedEntryTimeout {
			int64_t _next_fire_at_ms{};
			OrderId _order{};
			bool operator<(const DelayedEntryTimeout& o)const {
				return std::tie(_next_fire_at_ms, _order) < std::tie(o._next_fire_at_ms, o._order);
			}
		};

		struct NextTask {
			bool _final_task{ false };
			std::unique_ptr<QueuedTask> _run_task;
			int64_t _sleep_time_ms{};
		};

		NextTask GetNextTask();
		static void ThreadMain(void* context);
		void ProcessTasks();
		void NotifyWake();
		void DrainTasks();

		Event _started;
		Event _stopped;
		Event _flag_notify;
		PlatformThread _thread;
		Lock _pending_lock;
		bool _thread_should_quit{ false };
		OrderId _thread_posting_order{};
		std::queue<std::pair<OrderId, std::unique_ptr<QueuedTask>>> _pending_queue;
		std::map<DelayedEntryTimeout, std::unique_ptr<QueuedTask>> _delayed_queue;
	};

	TaskQueueStdlib::TaskQueueStdlib(const std::string &queue_name, ThreadPriority priority)
		:_started(false, false),
		_stopped(false, false),
		_flag_notify(false, false),
		_thread(&TaskQueueStdlib::ThreadMain, this, queue_name, priority)
	{
		_thread.Start();
		_started.Wait(-1);
	}

	void TaskQueueStdlib::Delete()
	{
		assert(!IsCurrent());
		{
			AutoLock lock_(_pending_lock);
			_thread_should_quit = true;
		}
		NotifyWake();
		_stopped.Wait(-1);
		_thread.Stop();
		delete this;
	}

	void TaskQueueStdlib::PostTask(std::unique_ptr<QueuedTask> task)
	{
		{
			AutoLock lock(_pending_lock);
			OrderId order = _thread_posting_order++;
			_pending_queue.push(std::pair<OrderId, std::unique_ptr<QueuedTask>>(order, std::move(task)));
		}
		NotifyWake();
	}

	void TaskQueueStdlib::PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds)
	{
		auto fire_at = GetCurrentTick() + milliseconds;
		DelayedEntryTimeout delay;
		delay._next_fire_at_ms = fire_at;
		{
			AutoLock lock(_pending_lock);
			delay._order = ++_thread_posting_order;
			_delayed_queue[delay] = std::move(task);
		}
		NotifyWake();
	}

	dd::TaskQueueStdlib::NextTask TaskQueueStdlib::GetNextTask()
	{
		NextTask result{};
		auto tick = GetCurrentTick();
		AutoLock lock(_pending_lock);
		if (_thread_should_quit) {
			result._final_task = true;
			return result;
		}

		if (_delayed_queue.size() > 0) {
			auto delayed_entry = _delayed_queue.begin();
			const auto& delay_info = delayed_entry->first;
			auto& delay_run = delayed_entry->second;
			if (tick >= delay_info._next_fire_at_ms) {
				if (_pending_queue.size() > 0) {
					auto& entry = _pending_queue.front();
					auto& entry_order = entry.first;
					auto& entry_run = entry.second;
					if (entry_order < delay_info._order){
						result._run_task = std::move(entry_run);
						_pending_queue.pop();
						return result;
					}
				}

				result._run_task = std::move(delay_run);
				_delayed_queue.erase(delayed_entry);
				return result;
			}
			result._sleep_time_ms = delay_info._next_fire_at_ms - tick;
		}

		if (_pending_queue.size() > 0) {
			auto& entry = _pending_queue.front();
			result._run_task = std::move(entry.second);
			_pending_queue.pop();
		}
		return result;
	}

	void TaskQueueStdlib::ThreadMain(void* context)
	{
		TaskQueueStdlib* me = static_cast<TaskQueueStdlib*>(context);
		CurrentTaskQueueSetter set_current(me);
		me->ProcessTasks();
	}

	void TaskQueueStdlib::NotifyWake()
	{
		_flag_notify.Set();
	}

	void TaskQueueStdlib::ProcessTasks()
	{
		_started.Set();
		while (true) {
			auto task = GetNextTask();
			if (task._final_task)
				break;
			if (task._run_task) {
				QueuedTask* release_ptr = task._run_task.release();
				if (release_ptr->Run())
					delete release_ptr;
				continue;
			}

			if (0 == task._sleep_time_ms)
				_flag_notify.Wait(Event::kForever);
			else
				_flag_notify.Wait(task._sleep_time_ms);
		}
		DrainTasks();
		_stopped.Set();
	}

	void TaskQueueStdlib::DrainTasks()
	{
		AutoLock lock(_pending_lock);
		while (_delayed_queue.size() > 0) {
			auto delayed_entry = _delayed_queue.begin();
			auto& delay_run = delayed_entry->second;
			std::unique_ptr<QueuedTask> task = std::move(delay_run);
			QueuedTask* release_ptr = task.release();
			if (release_ptr->Run())
				delete release_ptr;
			_delayed_queue.erase(delayed_entry);
		}

		while(_pending_queue.size() > 0){
			auto& entry = _pending_queue.front();
			std::unique_ptr<QueuedTask> task = std::move(entry.second);
			QueuedTask* release_ptr = task.release();
			if (release_ptr->Run())
				delete release_ptr;
			_pending_queue.pop();
		}
	}

	class TaskQueueStdlibFactory final : public TaskQueueFactory {
	public:
		std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
			const std::string &name,
			Priority priority)const override {
			return std::unique_ptr<TaskQueueBase, TaskQueueDeleter>(
				new TaskQueueStdlib(name, TaskQueuePriorityToThreadPriority(priority)));
		}
	};

	std::unique_ptr<TaskQueueFactory> CreateTaskQueueStdlibFactory() {
		return std::make_unique<TaskQueueStdlibFactory>();
	}
}// namespace dd
