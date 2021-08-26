#pragma once
#include <memory>
#include "queued_task.h"
#include "task_queue_base.h"

namespace dd {
	class Module;
	class Location;
	class ProcessThread : public TaskQueueBase {
	public:
		~ProcessThread() override;

		static std::unique_ptr<ProcessThread> Create(const char* thread_name);
		
		virtual void Start() = 0;
		virtual void Stop() = 0;
		virtual void WakeUp(Module* module) = 0;
		virtual void RegisterModule(Module* module, const Location& from) = 0;
		virtual void DeRegisterModule(Module* module) = 0;
	};
}// namespace dd
