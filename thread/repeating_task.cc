#include "repeating_task.h"
#include "to_queued_task.h"
#include "utility.h"
#include <limits>
#include <algorithm>
#include <iostream>

namespace dd {

	inline bool IsPlusInfinity(int64_t  v) {
		// windows下定义了max min的宏，为了避免冲突所以这样写
		return v == (std::numeric_limits<int64_t>::max)();
	}
	RepeatingTaskBase::RepeatingTaskBase(TaskQueueBase* task_queue,
		int64_t first_delay)
		: _task_queue(task_queue),
		_next_run_time(GetCurrentTick() + first_delay) {}
	RepeatingTaskBase::~RepeatingTaskBase() = default;

	bool RepeatingTaskBase::Run() {
		if (IsPlusInfinity(_next_run_time))
			return true;
		auto delay = RunClosure();
		if (IsPlusInfinity(_next_run_time))
			return true;

		uint64_t lost_time = GetCurrentTick() - _next_run_time;
		_next_run_time += delay;
		delay -= lost_time;
		delay = delay > 0 ? delay : 0;// (std::max)(delay, 0);
		_task_queue->PostDelayedTask(std::unique_ptr<RepeatingTaskBase>(this), delay);
		return false;
	}

	void RepeatingTaskBase::Stop() {
		_next_run_time = (std::numeric_limits<int64_t>::max)();
	}

	RepeatingTaskHandle::RepeatingTaskHandle(RepeatingTaskHandle&& other) 
		: _repeating_task(other._repeating_task) {
			other._repeating_task = nullptr;
		}
	
	RepeatingTaskHandle& RepeatingTaskHandle::operator=(RepeatingTaskHandle&& other) {
		_repeating_task = other._repeating_task;
		other._repeating_task = nullptr;
		return *this;
	}

	RepeatingTaskHandle::RepeatingTaskHandle(
		RepeatingTaskBase* repeating_task)
		: _repeating_task(repeating_task) {}

	void RepeatingTaskHandle::Stop() {
		if (_repeating_task) {
			_repeating_task->Stop();
			_repeating_task = nullptr;
		}
	}

	bool RepeatingTaskHandle::Running()const {
		return _repeating_task != nullptr;
	}
}// namespace dd
