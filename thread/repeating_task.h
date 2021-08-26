#pragma once
#include <memory>
#include <type_traits>
#include <utility>

#include "queued_task.h"
#include "task_queue_base.h"
#include "sequence_checker.h"
#include "thread_checker.h"

namespace dd {
	class RepeatingTaskHandle;

	class RepeatingTaskBase : public QueuedTask {
	public:
		RepeatingTaskBase(TaskQueueBase* task_queue, int64_t first_delay);
		~RepeatingTaskBase() override;
		virtual int64_t RunClosure() = 0;

	private:
		friend class RepeatingTaskHandle;

		bool Run() final;
		void Stop();

		TaskQueueBase* const _task_queue;
		int64_t _next_run_time;
	};

	template<class Closure>
	class RepeatingTaskImpl final : public RepeatingTaskBase {
	public:
		RepeatingTaskImpl(TaskQueueBase* task_queue,
			int64_t first_delay,
			Closure&& closure)
			: RepeatingTaskBase(task_queue, first_delay),
			_closure(std::forward<Closure>(closure)) {}

		int64_t RunClosure() override { return _closure(); }
	private:
		typename std::remove_const<
			typename std::remove_reference<Closure>::type>::type _closure;
	};

	class RepeatingTaskHandle {
	public:
		RepeatingTaskHandle() = default;
		~RepeatingTaskHandle() = default;
		RepeatingTaskHandle(RepeatingTaskHandle&& other);
		RepeatingTaskHandle& operator=(RepeatingTaskHandle&& other);
		RepeatingTaskHandle(const RepeatingTaskHandle&) = delete;
		RepeatingTaskHandle& operator=(const RepeatingTaskHandle&) = delete;

		template <class Closure>
		static RepeatingTaskHandle Start(TaskQueueBase* task_queue,
			Closure&& closure) {
			auto repeating_task = std::make_unique<
				RepeatingTaskImpl<Closure>>(task_queue, 0, std::forward<Closure>(closure));
			auto* repeating_task_ptr = repeating_task.get();
			task_queue->PostTask(std::move(repeating_task));
			return RepeatingTaskHandle(repeating_task_ptr);
		}

		template <class Closure>
		static RepeatingTaskHandle DelayedStart(TaskQueueBase* task_queue,
			int64_t first_delay,
			Closure&& closure) {
			auto repeating_task = std::make_unique<
				RepeatingTaskImpl<Closure>>(
					task_queue, first_delay, std::forward<Closure>(closure));
			auto* repeating_task_ptr = repeating_task.get();
			task_queue->PostDelayedTask(std::move(repeating_task), first_delay);
			return RepeatingTaskHandle(repeating_task_ptr);
		}

		void Stop();
		bool Running() const;

	private:
		explicit RepeatingTaskHandle(RepeatingTaskBase* repeating_task);
		RepeatingTaskBase* _repeating_task = nullptr;
	};
}// namespace dd
