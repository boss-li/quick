#pragma once
#include <stdint.h>

namespace dd {
	class ProcessThread;

	class Module {
	public:
		virtual int64_t TimeUntilNextProcess() = 0;
		virtual void Process() = 0;
		virtual void ProcessThreadAttached(ProcessThread* process_thread) {}

	protected:
		virtual ~Module(){}
	};
}// namespace dd
