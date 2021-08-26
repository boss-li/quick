#pragma once
#include "udp_socket.h"
#include "../thread/task_queue.h"
#include "../thread/task_queue_factory.h"
#include "../thread/repeating_task.h"
using namespace dd;

class QuickImpl;
class PacketReader
{
public:
	PacketReader(QuickImpl* quick, TaskQueueFactory* factory, UdpSocket* socket);
	void Stop();

private:
	void Recv();
	UdpSocket* _socket;
	TaskQueue _task_queue;
	static const int kInterval = 1000;
#define BUF_LEN 2048
	std::unique_ptr<uint8_t> _read_buf;
	bool _stop = false;
	QuickImpl* _quick;

	
};

