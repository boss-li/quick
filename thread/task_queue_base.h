#pragma once
#include <memory>
#include "queued_task.h"

namespace dd {
	class TaskQueueBase
	{
	public:
		virtual void Delete() = 0;
		virtual void PostTask(std::unique_ptr<QueuedTask> task) = 0;
		virtual void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds) = 0;
		static TaskQueueBase* Current();
		bool IsCurrent() const { return Current() == this; }
		virtual ~TaskQueueBase() = default;

	public:
		class CurrentTaskQueueSetter
		{
		public:
			explicit CurrentTaskQueueSetter(TaskQueueBase* task_queue);
			CurrentTaskQueueSetter(const CurrentTaskQueueSetter&) = delete;
			CurrentTaskQueueSetter& operator =(const CurrentTaskQueueSetter&) = delete;
			~CurrentTaskQueueSetter();
		private:
			TaskQueueBase* _previous;
		};
	};

	struct TaskQueueDeleter
	{
		void operator()(TaskQueueBase* task_queue) const { task_queue->Delete(); }
	};

} // namespace dd
