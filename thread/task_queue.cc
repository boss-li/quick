#include "task_queue.h"

namespace dd {
	TaskQueue::TaskQueue(std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue)
		: _impl(task_queue.release())
	{

	}

	TaskQueue::~TaskQueue()
	{
		_impl->Delete();
	}

	bool TaskQueue::IsCurrent() const
	{
		return _impl->IsCurrent();
	}

	void TaskQueue::PostTask(std::unique_ptr<QueuedTask> task)
	{
		_impl->PostTask(std::move(task));
	}

	void TaskQueue::PostDelayedTask(std::unique_ptr<QueuedTask> task, uint32_t milliseconds)
	{
		_impl->PostDelayedTask(std::move(task), milliseconds);
	}

}// namespace dd
