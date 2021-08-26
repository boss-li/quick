#include "platform_thread.h"
#include <assert.h>

namespace dd {

#ifndef _WIN32
	struct ThreadAttributes {
		ThreadAttributes() { pthread_attr_init(&attr); }
		~ThreadAttributes() { pthread_attr_destroy(&attr); }
		pthread_attr_t* operator&() { return &attr; }
		pthread_attr_t attr;
	};
#endif

	PlatformThread::PlatformThread(ThreadRunFunction func, void *obj, const std::string &thread_name, ThreadPriority priority /*= kNormalPriority*/)
		: _run_function(func), _priority(priority), _obj(obj), _name(thread_name)
	{
		assert(func);
		assert(!_name.empty());
		assert(_name.length() < 64);
		_spawned_thread_checker.Detach();
	}

	PlatformThread::~PlatformThread()
	{
		assert(_thread_checker.IsCurrent());
#ifdef  _WIN32
		assert(!_thread);
		assert(!_thread_id);
#endif
	}

	void PlatformThread::Start()
	{
		assert(_thread_checker.IsCurrent());
		assert(!_thread);
#ifdef _WIN32
		_thread = ::CreateThread(nullptr, 1024 * 1024, &StartThread, this,
			STACK_SIZE_PARAM_IS_A_RESERVATION, &_thread_id);
		assert(_thread);
		assert(_thread_id);
#else
		ThreadAttributes attr;
		pthread_attr_setstacksize(&attr, 1024 * 1024);
		int ret = pthread_create(&_thread, &attr, &StartThread, this);
		assert(ret == 0);
#endif
	}

	bool PlatformThread::IsRunning() const
	{
		assert(_thread_checker.IsCurrent());
#ifdef _WIN32
		return _thread != nullptr;
#else
		return _thread != 0;
#endif
	}

	PlatformThreadRef PlatformThread::GetThreadRef() const
	{
#ifdef _WIN32
		return _thread_id;
#else
		return _thread;
#endif
	}

	void PlatformThread::Stop()
	{
		assert(_thread_checker.IsCurrent());
		if (!IsRunning())
			return;
#ifdef _WIN32
		WaitForSingleObject(_thread, INFINITE);
		CloseHandle(_thread);
		_thread = nullptr;
		_thread_id = 0;
#else
		int ret = pthread_join(_thread, nullptr);
		assert(ret == 0);
#endif
		_spawned_thread_checker.Detach();
	}

#ifdef _WIN32
	bool PlatformThread::QueueAPC(PAPCFUNC apc_function, ULONG_PTR data)
	{
		assert(_thread_checker.IsCurrent());
		assert(IsRunning());
		return QueueUserAPC(apc_function, _thread, data) != FALSE;
	}
#endif

	void PlatformThread::Run()
	{
		assert(_spawned_thread_checker.IsCurrent());
		SetCurrentThreadName(_name.c_str());
		SetPriority(_priority);
		_run_function(_obj);
	}

	bool PlatformThread::SetPriority(ThreadPriority priority)
	{
		assert(_spawned_thread_checker.IsCurrent());
#ifdef _WIN32
		return SetThreadPriority(_thread, priority) != FALSE;
#else
		return true;
#endif
	}

#ifdef _WIN32
	DWORD WINAPI PlatformThread::StartThread(void *param)
	{
		::SetLastError(ERROR_SUCCESS);
		static_cast<PlatformThread*>(param)->Run();
		return 0;
	}
#else
	void *PlatformThread::StartThread(void* param)
	{
		static_cast<PlatformThread*>(param)->Run();
		return 0;
	}
#endif

}// namespace dd
