#ifdef _WIN32
#include <winsock2.h>
#include <io.h>
#pragma comment(lib, "Ws2_32.lib")
#define MyGetError() WSAGetLastError()
#endif
#include "udp_socket.h"
#include "../log/LogWrapper.h"
#include "../thread/utility.h"

extern LogWrapper g_quic_log;
using namespace dd;
UdpSocket::UdpSocket(uint16_t port)
	: _port(port)
{
	InitSocket();
	_read_time.reset(new timeval);
	_read_time->tv_sec = 1;
	_read_time->tv_usec = 0;
	_readset.reset(new fd_set);
	_read_addr.reset(new sockaddr_in);
}

UdpSocket::~UdpSocket()
{
	if (_socket != INVALID_SOCKET) {
#ifdef _WIN32
		closesocket(_socket);
#else
		close(_socket);
#endif
	}
}

bool UdpSocket::Send(const std::string &ip, uint16_t port, const uint8_t* data, uint16_t data_len)
{
	if (_socket == INVALID_SOCKET)
		return false;
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip.c_str());
	int send_len = sendto(_socket, (char*)data, data_len, 0, (sockaddr*)&addr, sizeof(addr));
	return send_len == data_len;
}

int16_t UdpSocket::Recv(std::string& ip, uint16_t& port, uint8_t* buf, uint16_t buf_len, uint64_t &received_time_microseconds)
{
	if (_socket == INVALID_SOCKET)
		return 0;
	FD_ZERO(_readset.get());
	FD_SET(_socket, _readset.get());
	auto ret = ::select(_socket + 1, _readset.get(), nullptr, nullptr, _read_time.get());
	auto code = MyGetError();
	if (ret <= 0)
		return 0;
	int addr_size = sizeof(sockaddr_in);
#ifdef _WIN32
#define socklen_t int
#endif
	// 经测试，当收到可读事件时，只读一次，和循环读取直到没有数据而退出，这两种方式效率上是一样的。
	// 收发都在本机上进行测试时，极限接收速度在 120Mbyte/s左右
	int read_len = recvfrom(_socket,
		(char*)buf,
		buf_len,
		0,
		(sockaddr*)_read_addr.get(),
		(socklen_t*)&addr_size);

	port = htons(_read_addr->sin_port);
	ip = inet_ntoa(_read_addr->sin_addr);
	code = MyGetError();
	if (read_len > 0) {
		_recv_speed_bytes += read_len;
		auto now = dd::GetCurrentTick();
		if (now - _print_time_ms >= 1000) {
			_print_time_ms = now;
#ifdef OUTPUT_VERBOSE_INFO
			float speed = (float)_recv_speed_bytes / 1024.0 / 1024.0;
			dd::OutputDebugInfo("recv speed: %f Mbyte/s\r\n", speed);
#endif
			_recv_speed_bytes = 0;
		}
#ifdef OUTPUT_VERBOSE_INFO
		dd::OutputDebugInfo("udp socket recv time  delta = %llu\r\n", received_time_microseconds - _prior_recv_time);
		_prior_recv_time = received_time_microseconds;
#endif
		received_time_microseconds = dd::GetCurrentTickMicroSecond();
		return read_len;
	}
	else if (read_len < 0 && code == 0) {
		return read_len; // -1 网络不可达
	}
	return 0;
}

void UdpSocket::InitSocket()
{
#ifdef _WIN32
	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (result != NO_ERROR) {
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_ERROR, "WSAStartup failed : %d", result);
		return;
	}
#endif
	_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (_socket == INVALID_SOCKET) {
#ifdef _WIN32
		WSACleanup();
#endif
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_ERROR, "create socket failed, code = %d", MyGetError());
		return;
	}

	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(_port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	int ret = bind(_socket, (sockaddr*)&addr, sizeof(addr));
	if (ret == INVALID_SOCKET) {
		g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_ERROR, "bind socket failed, code = %d", MyGetError());
		return;
	}

	SetSocketNoblock(_socket);
	g_quic_log.Write(RTC_FROM_HERE, LOG_LEVEL_INFO, "init udp socket<%u> successed, port = %d", _socket, _port);
}

void UdpSocket::SetSocketNoblock(TSOCKET s)
{
#ifdef _WIN32
	unsigned long mode = 1;
	ioctlsocket(s, FIONBIO, &mode);
#else
	int flags = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flags | O_NONBLOCK);
//	int o = 1;
//	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&o, sizeof(o));
#endif
}
