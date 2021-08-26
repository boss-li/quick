#include "task_queue_base.h"

namespace dd
{
	thread_local TaskQueueBase* current = nullptr;
	TaskQueueBase* TaskQueueBase::Current()
	{
		return current;
	}

	TaskQueueBase::CurrentTaskQueueSetter::CurrentTaskQueueSetter(TaskQueueBase* task_queue)
		: _previous(current)
	{
		current = task_queue;
	}

	TaskQueueBase::CurrentTaskQueueSetter::~CurrentTaskQueueSetter()
	{
		current = _previous;
	}

} // namespace dd
