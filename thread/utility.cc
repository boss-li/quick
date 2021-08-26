#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/times.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif
namespace dd {
	std::string GetCurrentDir()
	{
		int bufLen = 1024;
		char buf[1024];
		GetCurrentDir(buf, bufLen);
		std::string str = buf;
		return str;
	}

	void GetCurrentDir(char *dir, int dirLen)
	{
		memset(dir, 0, dirLen);
#ifdef _WIN32
		::GetModuleFileNameA(NULL, dir, dirLen);
		char* lpszTemp = strrchr(dir, '\\');
		lpszTemp++;
		*lpszTemp = 0;
#else
		int ret = readlink("/proc/self/exe", dir, dirLen);;
		int index = 0;
		for (index = ret; index > 0; index--)
		{
			if (dir[index] == '/')
				break;
			else
				dir[index] = '\0';
		}
#endif
	}

	void OutputDebugInfo(const char* format, ...)
	{
		char buf[256];
		va_list args;
		va_start(args, format);
		memset(buf, 0, 256);
		vsnprintf(buf, 256, format, args);
		va_end(args);
#ifdef _WIN32
		OutputDebugStringA(buf);
	//	OutputDebugStringA("\r\n");
#else
		printf("%s\r\n", buf);
#endif
	}

}// namespace dd
