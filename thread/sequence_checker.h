#pragma once
#include "utility.h"
#include "platform_thread_types.h"
#include "task_queue_base.h"

namespace dd
{
	class SequenceCheckerImpl
	{
	public:
		SequenceCheckerImpl();
		~SequenceCheckerImpl();
		bool IsCurrent() const;
		void Detach();

	private:
		mutable Lock _lock;
		mutable bool _attached;
		mutable PlatformThreadRef _valid_thread;
		mutable const TaskQueueBase* _valid_queue;
		mutable const void* _valid_system_queue;
	};
	class SequenceChecker : public SequenceCheckerImpl
	{
	};
}

