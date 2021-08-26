#include "event.h"
#ifdef _WIN32
#include <windows.h>
#include <assert.h>
#else
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>
#endif

namespace dd {
	Event::Event() : Event(false, false)
	{
	}

#ifdef _WIN32
	Event::Event(bool manual_reset, bool initially_signaled)
	{
		_event_handle = ::CreateEvent(nullptr, manual_reset, initially_signaled, nullptr);
		assert(_event_handle);
		::timeBeginPeriod(1);
	}

	Event::~Event()
	{
		::CloseHandle(_event_handle);
		::timeEndPeriod(1);
	}

	void Event::Set()
	{
		::SetEvent(_event_handle);
	}

	void Event::Reset()
	{
		::ResetEvent(_event_handle);
	}

	bool Event::Wait(int give_up_after_ms, int warn_after_ms)
	{
		const DWORD ms = (give_up_after_ms == kForever ? INFINITE : give_up_after_ms);
		return ::WaitForSingleObject(_event_handle, ms) == WAIT_OBJECT_0;
		Sleep(give_up_after_ms == -1 ? 0 : give_up_after_ms);
		return 0;
	}

#else

	Event::Event(bool manual_reset, bool initially_signaled)
		: _is_manual_reset(manual_reset), _event_status(initially_signaled)
	{
		assert(pthread_mutex_init(&_event_mutex, nullptr) == 0);
		pthread_condattr_t cond_attr;
		assert(pthread_condattr_init(&cond_attr) == 0);
		assert(pthread_cond_init(&_event_cond, &cond_attr) == 0);
		pthread_condattr_destroy(&cond_attr);
	}

	Event::~Event()
	{
		pthread_mutex_destroy(&_event_mutex);
		pthread_cond_destroy(&_event_cond);
	}

	void Event::Set()
	{
		pthread_mutex_lock(&_event_mutex);
		_event_status = true;
		pthread_cond_broadcast(&_event_cond);
		pthread_mutex_unlock(&_event_mutex);
	}

	void Event::Reset()
	{
		pthread_mutex_lock(&_event_mutex);
		_event_status = false;
		pthread_mutex_unlock(&_event_mutex);
	}

	timespec GetTimespec(int millisecond_from_now) {
		timespec ts;
		timeval tv;
		gettimeofday(&tv, nullptr);
		ts.tv_sec = tv.tv_sec;
		ts.tv_nsec = tv.tv_usec * 1000;

		ts.tv_sec += (millisecond_from_now / 1000);
		ts.tv_nsec += (millisecond_from_now % 1000) * 1000000;

		if (ts.tv_nsec >= 1000000000) {
			ts.tv_sec++;
			ts.tv_nsec -= 1000000000;
		}
		return ts;
	}

	bool Event::Wait(int give_up_after_ms, int warn_after_ms)
	{
		pthread_mutex_lock(&_event_mutex);
		auto ts = GetTimespec(give_up_after_ms);
		auto ret = pthread_cond_timedwait(&_event_cond, &_event_mutex, &ts);
		pthread_mutex_unlock(&_event_mutex);
		return ret;
	}
#endif

}// namespace dd
