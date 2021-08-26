#pragma once
#include <memory>
#include <string>
#include "task_queue_base.h"

namespace dd
{
	class TaskQueueFactory
	{
	public:
		enum class Priority { NORMAL = 0, HIGH, LOW };
		virtual ~TaskQueueFactory() = default;
		virtual std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
			const std::string &name, Priority priority) const = 0;
	};

	std::unique_ptr<TaskQueueFactory> CreateTaskQueueFactory();
} // namespace dd
