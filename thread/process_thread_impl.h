#pragma once
#include <stdint.h>
#include <memory>
#include <queue>
#include <list>

#include "queued_task.h"
#include "module.h"
#include "process_thread.h"
#include "event.h"
#include "platform_thread.h"
#include "thread_checker.h"
#include "utility.h"

namespace dd {
	class ProcessThreadImpl : public ProcessThread {
	public:
		explicit ProcessThreadImpl(const char* thread_name);
		~ProcessThreadImpl() override;

		void Start() override;
		void Stop() override;
		void WakeUp(Module* module) override;
		void PostTask(std::unique_ptr<QueuedTask> task) override;
		void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds) override;

		void RegisterModule(Module* module, const Location& from) override;
		void DeRegisterModule(Module* module) override;

	protected:
		static void Run(void *obj);
		bool Process();

	private:
		struct ModuleCallback {
			ModuleCallback() = delete;
			ModuleCallback(ModuleCallback&& cb) = default;
			ModuleCallback(const ModuleCallback& cb) = default;
			ModuleCallback(Module* m, const Location& l)
				: module(m), location(l){}
			bool operator== (const ModuleCallback& cb) const {
				return cb.module == module;
			}

			Module* const module;
			int64_t next_callback = 0;
			const Location location;
		private:
			ModuleCallback& operator=(ModuleCallback&);
		};

		struct DelayedTask {
			DelayedTask(int64_t run_at_ms, std::unique_ptr<QueuedTask> task)
			: run_at_ms(run_at_ms), task(task.release()){}
			friend bool operator<(const DelayedTask& lhs, const DelayedTask& rhs) {
				return lhs.run_at_ms > rhs.run_at_ms;
			}

			int64_t run_at_ms;
			QueuedTask* task;
		};
		typedef std::list<ModuleCallback> ModuleList;
		void Delete() override;
		Lock _lock;
		ThreadChecker _thread_checker;
		Event _wake_up;
		std::unique_ptr<PlatformThread> _thread;
		
		ModuleList _modules;
		std::queue<QueuedTask*> _queue;
		std::priority_queue<DelayedTask> _delayed_tasks;
		bool _stop;
		const char* _thread_name;

	};
}
