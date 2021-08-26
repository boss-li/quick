#pragma once
#ifdef _WIN32
#include <Windows.h>
#else
typedef void *HANDLE;
#define HMODULE void*
#define NULL 0
#endif
#include <string>
#include "../thread/utility.h"


enum
{
	LOG_LEVEL_FATAL = 0,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_INFO,
	LOG_LEVEL_DEBUG,
};
class LogWrapper
{
public:
	LogWrapper(void);
	~LogWrapper(void);

public:
	void Open(const char *name);
	void Close();
	bool Write(const dd::Location& location, int type, const char *format, ...);
	bool WriteHex(dd::Location& location, int type, const char *data, unsigned short dataLen, const char *format, ...);

public:
	static void Load();
	static void Free();

private:
	HANDLE m_hLog;
	static const int MAX_BUF_SIZE = 1024 * 150;
	char *m_buf;
};
