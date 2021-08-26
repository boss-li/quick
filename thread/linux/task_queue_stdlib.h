#pragma once
#include <memory>
#include "../task_queue_factory.h"

namespace dd {
	std::unique_ptr<TaskQueueFactory> CreateTaskQueueStdlibFactory();
}// namepsace dd
