#ifdef _WIN32
//#include "StdAfx.h"
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include "LogWrapper.h"
#include <stdio.h>
#include <stdarg.h>
#include <memory.h>
#include "../thread/utility.h"

typedef void (*FN_Init)();
typedef void (*FN_UnInit)();
typedef HANDLE (*FN_Open)(const char *log);
typedef bool (*FN_Close)(HANDLE h);
typedef bool (*FN_MLogWrite)(int type, HANDLE h, const char *log, unsigned short logLen);
typedef bool (*FN_MLogWriteHex)(int type, HANDLE hj, const char *prefix, const char *data, unsigned short dataLen);

static HMODULE s_hModule = NULL;
static FN_Init s_init = NULL;
static FN_UnInit s_uninit = NULL;
static FN_Open s_open = NULL;
static FN_Close s_close = NULL;
static FN_MLogWrite s_write = NULL;
static FN_MLogWriteHex s_writeHex = NULL;

LogWrapper::LogWrapper(void)
{
	m_buf = new char[MAX_BUF_SIZE];
	m_hLog = NULL;
}

LogWrapper::~LogWrapper(void)
{
	delete[] m_buf;
}

void LogWrapper::Open( const char *name )
{
	if(s_open != NULL)
		m_hLog = s_open(name);
}

void LogWrapper::Close()
{
	if(s_close != NULL)
		s_close(m_hLog);
}

bool LogWrapper::Write(const dd::Location& location, int type,  const char *format, ... )
{
	va_list args;
	va_start(args, format);
	memset(m_buf, 0, MAX_BUF_SIZE);
	const std::string str = location.ToString();
	if (str.length() >= MAX_BUF_SIZE)
		return false;
	sprintf(m_buf, "%s ", str.c_str());
	vsnprintf(m_buf + str.length() + 1, MAX_BUF_SIZE - str.length() - 1, format, args);
	m_buf[MAX_BUF_SIZE - 1] = '\0';
	va_end(args);

	if(s_write != NULL)
		return	s_write(type, m_hLog, m_buf, strlen(m_buf));
	return false;
}

bool LogWrapper::WriteHex(dd::Location& location, int type,  const char *data, unsigned short dataLen, const char *format, ... )
{
	va_list args;
	memset(m_buf, 0, MAX_BUF_SIZE);
	va_start(args, format);
	std::string str = location.ToString();
	if (str.length() >= MAX_BUF_SIZE)
		return false;
	sprintf(m_buf, "%s ", str.c_str());
	vsnprintf(m_buf + str.length() + 1, MAX_BUF_SIZE - str.length() - 1, format, args);
	m_buf[MAX_BUF_SIZE - 1] = '\0';
	va_end(args);

	if(s_writeHex != NULL)
		return s_writeHex(type, m_hLog, m_buf, data, dataLen);
	return false;
}
void LogWrapper::Load()
{
	if(s_hModule != NULL)
		return;
#ifdef _WIN32
	char lpszPath[1024];
	dd::GetCurrentDir(lpszPath, 1024);
	strcat(lpszPath, "mlog.dll");
	s_hModule = ::LoadLibraryA(lpszPath);
	if(!s_hModule)
		return;
	s_init =(FN_Init)::GetProcAddress(s_hModule, "MLogInit");
	s_uninit =(FN_UnInit)::GetProcAddress(s_hModule, "MLogUnInit");
	s_open =(FN_Open)::GetProcAddress(s_hModule, "MLogOpen");
	s_close = (FN_Close)::GetProcAddress(s_hModule, "MLogClose");
	s_write = (FN_MLogWrite)::GetProcAddress(s_hModule, "MLogWrite");
	s_writeHex = (FN_MLogWriteHex)::GetProcAddress(s_hModule, "MLogWriteHex");
#else
	s_hModule = dlopen("libmlog.so", RTLD_NOW);
	if (s_hModule)
	{
		printf("dlopen libmlog.so successed\n");
		s_init =(FN_Init)dlsym(s_hModule, "MLogInit");
		s_uninit =(FN_UnInit)dlsym(s_hModule, "MLogUnInit");
		s_open =(FN_Open)dlsym(s_hModule, "MLogOpen");
		s_close = (FN_Close)dlsym(s_hModule, "MLogClose");
		s_write = (FN_MLogWrite)dlsym(s_hModule, "MLogWrite");
		s_writeHex = (FN_MLogWriteHex)dlsym(s_hModule, "MLogWriteHex");
	}
	else
		printf("dlopen libmlog.so failed\n");
#endif
	if(s_init != NULL)
		s_init();
}

void LogWrapper::Free()
{
	if(s_uninit != NULL)
	{
		s_uninit();
		s_uninit = NULL;
	}
	if(s_hModule == NULL)
		return;
#ifdef WIN32
	::FreeLibrary(s_hModule);
#else
	dlclose(s_hModule);
#endif
	s_hModule = NULL;
	s_init = NULL;
	s_uninit = NULL;
	s_open = NULL;
	s_close = NULL;
	s_write = NULL;
	s_writeHex = NULL;
}
