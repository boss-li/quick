#pragma once

#ifdef _WIN32
#include <windows.h>
//#include <winsock2.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

namespace dd {
#ifdef _WIN32
	typedef DWORD PlatformThreadId;
	typedef DWORD PlatformThreadRef;
#else
	typedef pid_t PlatformThreadId;
	typedef pthread_t PlatformThreadRef;
#endif

	PlatformThreadId CurrentThreadId();
	PlatformThreadRef CurrentThreadRef();

	bool IsThreadRefEqual(const PlatformThreadRef& a, const PlatformThreadRef& b);
	void SetCurrentThreadName(const char* name);
}// namespace dd
