#pragma once
#include <memory>
#include "queued_task.h"

namespace dd {
	template <typename Closure>
	class ClosureTask : public QueuedTask {
	public:
		explicit ClosureTask(Closure&& closure) :_closure(std::forward<Closure>(closure)) {}

	private:
		bool Run()override {
			_closure();
			return true;
		}
	private:
		typename std::decay<Closure>::type _closure;
	};

	template<typename Closure, typename Cleanup>
	class ClosureTaskWithCleanup : public ClosureTask<Closure> {
	public:
		ClosureTaskWithCleanup(Closure &&closure, Cleanup&& cleanup)
			: ClosureTask<Closure>(std::forward<Closure>(closure)),
			_cleanup(std::forward<Cleanup>(cleanup)){}

		~ClosureTaskWithCleanup()override { _cleanup(); }
	private:
		typename std::decay<Cleanup>::type _cleanup;
	};

	template<typename Closure>
	std::unique_ptr<QueuedTask> ToQueuedTask(Closure&& closure) {
		return std::make_unique<ClosureTask<Closure>>(std::forward<Closure>(closure));
	}

	template< typename Closure, typename Cleanup>
	std::unique_ptr<QueuedTask> ToQueuedTask(Closure&& closure, Cleanup&& cleanup) {
		return std::make_unique<ClosureTaskWithCleanup<Closure, Cleanup>>(
			std::forward<Closure>(closure), std::forward<Cleanup>(cleanup));
	}
}// namespace dd


