#pragma once
#include "simple_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctime> 
#include<iomanip> 
#include <sstream>
#include <assert.h>
#include <fstream>
#include <chrono>
#include <thread>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <dirent.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../thread/utility.h"

void OutputDebugInfo(const char* format, ...)
{
	char buf[256];
	va_list args;
	va_start(args, format);
	memset(buf, 0, 256);
	vsnprintf(buf, 256, format, args);
	va_end(args);


	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::time_t now_time = std::chrono::system_clock::to_time_t(now);
	auto v = std::put_time(std::localtime(&now_time), "%Y-%m-%d %X");
	std::stringstream ss;
	ss << v;
	printf("%s %s\r\n", ss.str().c_str(), buf);
}

SimpleServer::SimpleServer(uint16_t port)
	: _local_port(port)
{
	_quick = Quick::Create(_local_port, true, this);
	OutputDebugInfo("quick server start, udp port %d", _local_port);
}

SimpleServer::~SimpleServer()
{

}

bool SimpleServer::Process()
{
	ProcessPackets();
	auto now = dd::GetCurrentTickMicroSecond();
	if (now - _print_info >= 1000000) {
		_print_info = now;
		float speed = (float)_recv_speed / 1024.0 / 1024.0;
		OutputDebugInfo("total recv: %llu bytes, recv speed = %f Mbyte/s", _total_recv_bytes, speed);
		_recv_speed = 0;
	}
	return true;
}

void SimpleServer::OnConnected(uint32_t session_id)
{

}

void SimpleServer::OnDisconnected(uint32_t session_id)
{
	OutputDebugInfo("session %u disconnected", session_id);
	_session_id = 0;
}

void SimpleServer::OnNewClientConnected(uint32_t session_id, const std::string &ip, uint16_t port)
{
	if (_session_id) {
		OutputDebugInfo("only allow 1 client connect, the connected client is %u", _session_id);
		return;
	}
	OutputDebugInfo("new client connected, session id = %u, ip = %s, port = %d", session_id, ip.c_str(), port);
	_session_id = session_id;
	_total_recv_bytes = 0;
}

void SimpleServer::OnMessage(uint32_t session_id, const uint8_t *data, uint16_t data_len)
{
	dd::AutoLock lock(_lock);
	_wait_process_packets.push_back(Packet(session_id, data_len, data));
}

void SimpleServer::ProcessPackets()
{
	dd::AutoLock lock(_lock);
	for (auto iter = _wait_process_packets.begin(); iter != _wait_process_packets.end();) {
		
		_total_recv_bytes += iter->data_len;
		_recv_speed += iter->data_len;
		uint8_t msg_id = iter->data.get()[0];
		switch (msg_id)
		{
		case Q_MSG_FILE_BEGIN:
			ProcFileBegin(iter->data.get(), iter->data_len);
			break;
		case Q_MSG_FILE_DATA:
			ProcFileData(iter->data.get(), iter->data_len);
			break;
		case Q_MSG_FILE_END:
			ProcFileEnd(iter->data.get(), iter->data_len);
			break;
		case Q_MSG_VERBOSE:
			ProcVerbose(iter->data.get(), iter->data_len);
			break;
		default:
			break;
		}
		iter = _wait_process_packets.erase(iter);
	}
}

void SimpleServer::ProcFileBegin(uint8_t *data, uint16_t data_len)
{
	FileBegin *msg = (FileBegin*)data;
	char name[256];
	sprintf(name, "recv_%s", msg->file_name);
	std::string file_path = dd::GetCurrentDir();
	file_path += name;
	OutputDebugInfo("begin recv file: %s", name);
#ifdef _WIN32
	auto ret = DeleteFileA(file_path.c_str());
	auto code = GetLastError();
	_fd = _open(file_path.c_str(),  _O_CREAT | _O_RDWR |  _O_BINARY  |  _O_TRUNC, _S_IREAD | _S_IWRITE);
#else
	remove(file_path.c_str());
	_fd = open(file_path.c_str(), O_APPEND | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
#endif
	assert(_fd != -1);
	_begin_recv_file_time = dd::GetCurrentTickMicroSecond();
}

void SimpleServer::ProcFileData(uint8_t *data, uint16_t data_len)
{
	FileData *msg = (FileData*)data;
	int  write_len = 0;
#ifdef _WIN32
	write_len = _write(_fd, msg->data, data_len - 1);
#else
	write(_fd, msg->data, data_len - 1);
#endif
}

void SimpleServer::ProcFileEnd(uint8_t *data, uint16_t data_len)
{
	auto now = dd::GetCurrentTickMicroSecond();
	OutputDebugInfo("recv file finished, use time(ms) : %llu", (now - _begin_recv_file_time) / 1000);
#ifdef _WIN32
	_close(_fd);
#else
	close(_fd);
#endif
	_fd = 0;
}

void SimpleServer::ProcVerbose(uint8_t *data, uint16_t data_len)
{
	// no nothing
}
