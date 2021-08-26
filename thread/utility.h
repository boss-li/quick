#pragma once
#include <string.h>
#include <string>
#include <memory>
#ifdef _WIN32
#include <Windows.h>
#else
#include <pthread.h>
#endif
#include <chrono>

//#define OUTPUT_DEBUG_INFO
//#define OUTPUT_VERBOSE_INFO

namespace dd {
	class Location {
	public:
		Location(const char* function_name, const char* file_name, int line_number)
			: _function_name(function_name), _file_name(file_name), _line_number(line_number) {}

		const char* function_name() const { return _function_name; }
		const char* file_name() const { return _file_name; }
		int line_number()const { return line_number(); }
		std::string ToString()const {
			std::string file_name = _file_name;
			for (int i = file_name.length() - 1; i >= 0; i--)
			{
#ifdef WIN32
				if (file_name.substr(i, 1) == "\\")
#else
				if (file_name.substr(i, 1) == "/")
#endif
				{
					file_name = file_name.substr(i + 1);
					break;
				}
			}

			char buf[256] = { 0 };
			snprintf(buf, sizeof(buf), "%s@%s:%d", _function_name, file_name.c_str(), _line_number);
			return buf;
		}
	private:
		const char* _function_name = "Unknown";
		const char* _file_name = "Unknown";
		int _line_number = -1;
	};

#define RTC_FROM_HERE RTC_FROM_HERE_WITH_FUNCTION(__FUNCTION__)
#define RTC_FROM_HERE_WITH_FUNCTION(function_name) \
Location(function_name, __FILE__, __LINE__)

	inline auto GetCurrentTick()
	{
		auto now = std::chrono::system_clock::now().time_since_epoch();
		return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
	}

	inline auto GetCurrentTickMicroSecond()
	{
		auto now = std::chrono::system_clock::now().time_since_epoch();
		return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
	}

	std::string GetCurrentDir();
	void GetCurrentDir(char *dir, int dirLen);

	// 使用Lock要比std::mutex的效率要高，原因在于CriticalSection效率比mutex要高
	class Lock
	{
	public:
		Lock()
		{
#ifdef WIN32
			::InitializeCriticalSection(&_csLock);
#else
			pthread_mutex_init(&_mutex, NULL);
#endif
		}
		~Lock()
		{
#ifdef WIN32
			::DeleteCriticalSection(&_csLock);
#else
			pthread_mutex_destroy(&_mutex);
#endif
		}

		void LockBegin()
		{
#ifdef WIN32
			::EnterCriticalSection(&_csLock);
#else
			pthread_mutex_lock(&_mutex);
#endif
		}

		void LockEnd()
		{
#ifdef WIN32
			::LeaveCriticalSection(&_csLock);
#else
			pthread_mutex_unlock(&_mutex);
#endif
		}
	private:
#ifdef WIN32
		CRITICAL_SECTION _csLock;
#else
		pthread_mutex_t _mutex;
#endif
	};

	class AutoLock
	{
	private:
		Lock &_lock;
	public:
		explicit AutoLock(Lock &lock)
			:_lock(lock)
		{
			_lock.LockBegin();
		}
		~AutoLock(void)
		{
			_lock.LockEnd();
		}

		AutoLock(const AutoLock&) = delete;
		AutoLock& operator =(const AutoLock&) = delete;
	};

	void OutputDebugInfo(const char* format, ...);
}// namespace dd
