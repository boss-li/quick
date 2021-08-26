#pragma once
#include <memory>
#include "task_queue_base.h"
#include "task_queue_factory.h"
#include "queued_task.h"
#include "to_queued_task.h"

namespace dd {
	class TaskQueue
	{
	public:
		using Priority = TaskQueueFactory::Priority;
		explicit TaskQueue(std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue);
		~TaskQueue();

		bool IsCurrent() const;

		TaskQueueBase* Get() { return _impl; }

		void PostTask(std::unique_ptr<QueuedTask> task);
		void PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds);

		template<class Closure,
			typename std::enable_if<!std::is_convertible<
			Closure,
			std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
			void PostTask(Closure && closure) {
			PostTask(ToQueuedTask(std::forward<Closure>(closure)));
		}

		template<class Closure,
			typename std::enable_if<!std::is_convertible<
			Closure,
			std::unique_ptr<QueuedTask>>::value>::type* = nullptr>
			void PostDelayedTask(Closure&& closure, uint32_t milliseconds){
			PostDelayedTask(ToQueuedTask(std::forward<Closure>(closure)), milliseconds);
		}

	private:
		TaskQueueBase* const _impl;
	};

}// namespace dd

