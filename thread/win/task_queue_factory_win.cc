#include "../task_queue_factory.h"
#include "task_queue_win.h"

namespace dd
{
	std::unique_ptr<TaskQueueFactory> CreateTaskQueueFactory()
	{
		return CreateTaskQueueWinFactory();
	}

}// namespace
