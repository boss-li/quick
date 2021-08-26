#include "process_thread_impl.h"
#include <string.h>
#include <assert.h>
#include "module.h"
namespace dd {
	const int64_t kCallProcessImmediately = -1;

	int64_t GetNextCallbackTime(Module* module, int64_t time_now) {
		int64_t interval = module->TimeUntilNextProcess();
		if (interval < 0)
			return time_now;
		return time_now + interval;
	}

	ProcessThread::~ProcessThread() {}

	// static
	std::unique_ptr<ProcessThread> ProcessThread::Create(const char* thread_name) {
		return std::unique_ptr<ProcessThread>(new ProcessThreadImpl(thread_name));
	}

	ProcessThreadImpl::ProcessThreadImpl(const char* thread_name)
		: _stop(false), _thread_name(thread_name) {}

	ProcessThreadImpl::~ProcessThreadImpl() {
		assert(_thread_checker.IsCurrent());
		assert(!_thread.get());
		assert(!_stop);
		while (!_delayed_tasks.empty()) {
			delete _delayed_tasks.top().task;
			_delayed_tasks.pop();
		}

		while (!_queue.empty()) {
			delete _queue.front();
			_queue.pop();
		}
	}

	void ProcessThreadImpl::Delete() {
		Stop();
		delete this;
	}

	void ProcessThreadImpl::Start() {
		assert(_thread_checker.IsCurrent());
		assert(!_thread.get());
		if (_thread.get())
			return;
		assert(!_stop);

		for (ModuleCallback& m : _modules)
			m.module->ProcessThreadAttached(this);

		_thread.reset(new PlatformThread(&ProcessThreadImpl::Run, this, _thread_name));
		_thread->Start();
	}

	void ProcessThreadImpl::Stop() {
		assert(_thread_checker.IsCurrent());
		if (!_thread.get())
			return;
		{
			AutoLock l(_lock);
			_stop = true;
		}
		_wake_up.Set();
		_thread->Stop();
		_stop = false;
		_thread.reset();
		for (ModuleCallback& m : _modules)
			m.module->ProcessThreadAttached(nullptr);
	}

	void ProcessThreadImpl::WakeUp(Module* module) {
		{
			AutoLock l(_lock);
			for (ModuleCallback& m : _modules){
				if (m.module == module)
					m.next_callback = kCallProcessImmediately;
			}
		}
		_wake_up.Set();
	}

	void ProcessThreadImpl::PostTask(std::unique_ptr<QueuedTask> task) {
		{
			AutoLock l(_lock);
			_queue.push(task.release());
		}
		_wake_up.Set();
	}

	void ProcessThreadImpl::PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds) {
		int64_t run_at_ms = GetCurrentTick() + milliseconds;
		bool recalulate_wakeup_time = false;
		{
			AutoLock l(_lock);
			recalulate_wakeup_time = _delayed_tasks.empty() || run_at_ms < _delayed_tasks.top().run_at_ms;
			_delayed_tasks.emplace(run_at_ms, std::move(task));
		}
		if (recalulate_wakeup_time) {
			_wake_up.Set();
		}
	}

	void ProcessThreadImpl::RegisterModule(Module* module, const Location& from) {
		assert(_thread_checker.IsCurrent());
		if (_thread.get())
			module->ProcessThreadAttached(this);
		{
			AutoLock l(_lock);
			_modules.push_back(ModuleCallback(module, from));
		}
		_wake_up.Set();
	}

	void ProcessThreadImpl::DeRegisterModule(Module* module) {
		assert(_thread_checker.IsCurrent());
		assert(module);
		{
			AutoLock l(_lock);
			_modules.remove_if([&module](const ModuleCallback& m){ return m.module == module; });
		}

		module->ProcessThreadAttached(nullptr);
	}

	// static
	void ProcessThreadImpl::Run(void* obj) {
		ProcessThreadImpl* impl = static_cast<ProcessThreadImpl*>(obj);
		CurrentTaskQueueSetter set_current(impl);
		while (impl->Process())
		{}
	}

	bool ProcessThreadImpl::Process() {
		int64_t now = GetCurrentTick();
		int64_t next_checkpoint = now + (1000 * 60);

		{
			AutoLock l(_lock);
			if (_stop)
				return false;
			for (ModuleCallback& m : _modules) {
				if (m.next_callback == 0)
					m.next_callback = GetNextCallbackTime(m.module, now);
				if (m.next_callback <= now || m.next_callback == kCallProcessImmediately) {
					{
						m.module->Process();
					}
					int64_t new_now = GetCurrentTick();
					m.next_callback = GetNextCallbackTime(m.module, new_now);
				}

				if (m.next_callback < next_checkpoint)
					next_checkpoint = m.next_callback;
			}

			while (!_delayed_tasks.empty() && _delayed_tasks.top().run_at_ms <= now) {
				_queue.push(_delayed_tasks.top().task);
				_delayed_tasks.pop();
			}

			if (!_delayed_tasks.empty())
			{
				if (_delayed_tasks.top().run_at_ms < next_checkpoint)
					next_checkpoint = _delayed_tasks.top().run_at_ms;
			}

			while (!_queue.empty()) {
				QueuedTask* task = _queue.front();
				_queue.pop();
				_lock.LockEnd();
				if (task->Run())
					delete task;
				_lock.LockBegin();
			}
		}
		
		int64_t time_to_wait = next_checkpoint - GetCurrentTick();
		if (time_to_wait > 0)
			_wake_up.Wait(static_cast<int>(time_to_wait));
		return true;
	}
} // namespace dd