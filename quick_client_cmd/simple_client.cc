#include "simple_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctime> 
#include<iomanip> 
#include <sstream>
#include <assert.h>
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
#include <random>
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

SimpleClient::SimpleClient(uint16_t local_port, const std::string& server_ip, uint16_t server_port, const std::string& send_file)
	: _local_port(local_port), _server_ip(server_ip), _server_port(server_port), _send_file(send_file)
{
	_quick = Quick::Create(_local_port, false, this);
	OutputDebugInfo("init quick, local udp port = %d", _local_port);
	_session_id = _quick->ConnectToServer(_server_ip, _server_port);
	OutputDebugInfo("connect to server %s:%d", _server_ip.c_str(), _server_port);
}

SimpleClient::~SimpleClient()
{

}

bool SimpleClient::Process()
{
	if (!_server_state)
		return true;
	if (_send_file != "") {
		SendFile();
		return false;
	}
	else {
		if (!_server_state)
			return true;

#define PACKET_SIZE 1350
		static std::mt19937 rng;
		rng.seed(std::random_device()());
		static std::uniform_int_distribution<std::mt19937::result_type> dist6(1000, PACKET_SIZE);
		static int data_len = 0;

		for (int i = 0; i < 50; i++) {
			static uint8_t buf[PACKET_SIZE];
			Verbose *msg = (Verbose*)buf;
			msg->msg_id = Q_MSG_VERBOSE;
			data_len = dist6(rng);
			if (_quick->Send(_session_id, buf, data_len)) {
				_total_send_bytes += data_len;
				_send_speed += data_len;
			}
			else
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	
		PrintInfo();
	}
	return true;
}

void SimpleClient::OnConnected(uint32_t session_id)
{
	OutputDebugInfo("server connected, session id = %u", session_id);
	_server_state = true;
}

void SimpleClient::OnDisconnected(uint32_t session_id)
{
	OutputDebugInfo("server disconnected, session id = %u", session_id);
	_server_state = false;
}

void SimpleClient::OnNewClientConnected(uint32_t session_id, const std::string &ip, uint16_t port)
{

}

void SimpleClient::OnMessage(uint32_t session_id, const uint8_t *data, uint16_t data_len)
{

}

void SimpleClient::PrintInfo()
{
		auto now = dd::GetCurrentTickMicroSecond();
		if (now - _print_info >= 1000000) {
			_print_info = now;
			float speed = (float)_send_speed / 1024.0 / 1024.0;
			OutputDebugInfo("total send: %llu bytes, send speed: %f Mbyte/s", _total_send_bytes, speed);
			_send_speed = 0;
		}
}

void SimpleClient::SendFile()
{
	SendFileBegin();

	uint8_t buf[1350];
	int read_len = 0;
	FileData *msg = (FileData*)buf;
	msg->msg_id = Q_MSG_FILE_DATA;
	bool ret = false;
	int fd = 0;

#ifdef _WIN32
	fd = _open(_send_file.c_str(), _O_RDONLY | _O_BINARY);
#else
	fd = open(_send_file.c_str(), O_RDONLY | O_SYNC, S_IRUSR | S_IWUSR);
#endif
	printf("file = %s\r\n", _send_file.c_str());
	assert(fd != -1);

	while (true) {
#ifdef _WIN32
		read_len = _read(fd, msg->data, 1349);
#else
		read_len = read(fd, msg->data, 1349);
#endif
		if(read_len <= 0)
			break;

		while (true) {
			ret = _quick->Send(_session_id, buf, read_len + 1);
			if (ret) {
				_total_send_bytes += read_len + 1;
				break;
			}
			else
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

#ifdef _WIN32
	_close(fd);
#else
	close(fd);
#endif

	SendFileEnd();
}

void SimpleClient::SendFileBegin()
{
#ifdef _WIN32
	int index = _send_file.find_last_of("\\");
#else
	int index = _send_file.find_last_of("/");
#endif
	std::string name = _send_file.substr(index + 1);
	FileBegin msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_id = Q_MSG_FILE_BEGIN;
	assert(name.length() < sizeof(msg.file_name));
	memcpy(msg.file_name, name.c_str(), name.length());
	_quick->Send(_session_id, (uint8_t*)&msg, sizeof(msg));

	_total_send_bytes += sizeof(msg);
	OutputDebugInfo("start send file, file name = %s", name.c_str());
}

void SimpleClient::SendFileEnd()
{
	FileEnd msg;
	msg.msg_id = Q_MSG_FILE_END;
	_quick->Send(_session_id, (uint8_t*)&msg, sizeof(msg));
	_total_send_bytes != sizeof(msg);
	OutputDebugInfo("send file finish");
}
