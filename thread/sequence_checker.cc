#include "sequence_checker.h"

namespace dd
{
	const void* GetSystemQueueRef()
	{
		return nullptr;
	}
	SequenceCheckerImpl::SequenceCheckerImpl()
		:_attached(true),
		_valid_thread(CurrentThreadRef()),
		_valid_queue(TaskQueueBase::Current()),
		_valid_system_queue(GetSystemQueueRef())
	{

	}

	SequenceCheckerImpl::~SequenceCheckerImpl() = default;

	bool SequenceCheckerImpl::IsCurrent() const
	{
		const TaskQueueBase* const current_queue = TaskQueueBase::Current();
		const PlatformThreadRef current_thread = CurrentThreadRef();
		const void* const current_system_queue = GetSystemQueueRef();
		AutoLock lock(_lock);
		if (!_attached)
		{
			_attached = true;
			_valid_thread = current_thread;
			_valid_queue = current_queue;
			_valid_system_queue = current_system_queue;
			return true;
		}
		if (_valid_queue || current_queue)
			return _valid_queue == current_queue;
		if (_valid_system_queue && _valid_system_queue == current_system_queue)
			return true;
		return IsThreadRefEqual(_valid_thread, current_thread);
	}

	void SequenceCheckerImpl::Detach()
	{
		AutoLock lock(_lock);
		_attached = false;
	}

}// namespace dd
