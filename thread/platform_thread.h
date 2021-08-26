#pragma once
#include <string>
#include "thread_checker.h"

namespace dd {
	typedef void(*ThreadRunFunction)(void*);
	enum ThreadPriority {
#ifdef _WIN32
		kLowPriority = THREAD_PRIORITY_BELOW_NORMAL,
		kNormalPriority = THREAD_PRIORITY_NORMAL,
		kHighPriority = THREAD_PRIORITY_ABOVE_NORMAL,
		kHighestPriority = THREAD_PRIORITY_HIGHEST,
		kRealtimePriority = THREAD_PRIORITY_TIME_CRITICAL
#else
		kLowPriority = 1,
		kNormalPriority = 2,
		kHighPriority = 3,
		kHighestPriority = 4,
		kRealtimePriority = 5
#endif
	};
	class PlatformThread
	{
	public:
		PlatformThread(ThreadRunFunction func,
			void *obj,
			const std::string &thread_name,
			ThreadPriority priority = kNormalPriority);
		virtual ~PlatformThread();
		const std::string& name() const { return _name; }
		void Start();
		bool IsRunning() const;
		PlatformThreadRef GetThreadRef() const;
		void Stop();

	protected:
#ifdef _WIN32
		bool QueueAPC(PAPCFUNC apc_function, ULONG_PTR data);
#endif

	private:
		void Run();
		bool SetPriority(ThreadPriority priority);

		ThreadRunFunction const _run_function = nullptr;
		const ThreadPriority _priority = kNormalPriority;
		void* const _obj;
		const std::string _name;
		ThreadChecker _thread_checker;
		ThreadChecker _spawned_thread_checker;
#ifdef _WIN32
		static DWORD WINAPI StartThread(void *param);
		HANDLE _thread = nullptr;
		DWORD _thread_id = 0;
#else
		static void* StartThread(void* param);
		pthread_t _thread = 0;
#endif
	};
}// namespace dd
