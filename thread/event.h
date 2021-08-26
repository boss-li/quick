#pragma once
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
namespace dd {
	class Event
	{
	public:
		static const int kForever = -1;
		Event();
		Event(bool manual_reset, bool initially_signaled);
		Event(const Event&) = delete;
		Event& operator=(const Event&) = delete;
		~Event();

		void Set();
		void Reset();
		bool Wait(int give_up_after_ms, int warn_after_ms);
		bool Wait(int give_up_after_ms) {
			return Wait(give_up_after_ms, give_up_after_ms == kForever ? 3000 : kForever);
		}

	private:
#ifdef _WIN32
		HANDLE _event_handle;
#else
		pthread_mutex_t _event_mutex;
		pthread_cond_t _event_cond;
		const bool _is_manual_reset;
		bool _event_status;
#endif
	};
} // namepace dd
