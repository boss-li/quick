#include "platform_thread_types.h"
#ifndef _WIN32
#include <sys/syscall.h>
#include <sys/prctl.h>
#endif
namespace dd {

	PlatformThreadId CurrentThreadId()
	{
#ifdef _WIN32
		return GetCurrentThreadId();
#else
		//return reinterpret_cast<PlatformThreadId>(pthread_self());
		 return syscall(__NR_gettid);
#endif
	}



	PlatformThreadRef CurrentThreadRef()
	{
#ifdef _WIN32
		return GetCurrentThreadId();
#else
		return pthread_self();
#endif
	}

	bool IsThreadRefEqual(const PlatformThreadRef& a, const PlatformThreadRef& b)
	{
#ifdef _WIN32
		return a == b;
#else
		pthread_equal(a, b);
#endif
	}

	void SetCurrentThreadName(const char* name)
	{
#ifdef _WIN32
		// For details see:
// https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code
#pragma pack(push, 8)
		struct {
			DWORD dwType;
			LPCSTR szName;
			DWORD dwThreadID;
			DWORD dwFlags;
		} threadname_info = { 0x1000, name, static_cast<DWORD>(-1), 0 };
#pragma pack(pop)

#pragma warning(push)
#pragma warning(disable : 6320 6322)
		__try {
			::RaiseException(0x406D1388, 0, sizeof(threadname_info) / sizeof(ULONG_PTR),
				reinterpret_cast<ULONG_PTR*>(&threadname_info));
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {  // NOLINT
		}
#else
		prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(name));
#endif
	}

}// namespace dd
