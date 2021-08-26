#pragma once
#include <memory>
#include <string>
#ifndef _WIN32
#define MyGetError() errno
#define TSOCKET int
#define INVALID_SOCKET -1
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#else
#define TSOCKET uint64_t
struct fd_set;
#endif

struct timeval;
struct sockaddr_in;
class UdpSocket
{
public:
	UdpSocket(uint16_t port);
	~UdpSocket();
	bool Send(const std::string &ip, uint16_t port, const uint8_t* data, uint16_t data_len);
	int16_t Recv(std::string& ip, uint16_t& port, uint8_t* buf, uint16_t buf_len, uint64_t &received_time_microseconds);
private:
	void InitSocket();
	void SetSocketNoblock(TSOCKET s);

	uint16_t _port;
	TSOCKET _socket;
	std::unique_ptr<timeval> _read_time;
	std::unique_ptr<fd_set> _readset;
	std::unique_ptr<sockaddr_in> _read_addr;

	uint64_t _recv_speed_bytes = 0;
	uint64_t _print_time_ms = 0;

	uint64_t _prior_recv_time = 0;
};

