#pragma once
#include "../task_queue_base.h"
#include "../task_queue_factory.h"
namespace dd
{
	std::unique_ptr<TaskQueueFactory> CreateTaskQueueWinFactory();

}// namespace dd
