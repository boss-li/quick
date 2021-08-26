#include "../task_queue_factory.h"
#include "task_queue_stdlib.h"

namespace dd {

	std::unique_ptr<TaskQueueFactory> CreateTaskQueueFactory()
	{
		return CreateTaskQueueStdlibFactory();
	}
}// namespace dd
