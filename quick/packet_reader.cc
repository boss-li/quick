#include "packet_reader.h"
#include "quick_impl.h"

PacketReader::PacketReader(QuickImpl* quick, TaskQueueFactory* factory, UdpSocket* socket)
	:_quick(quick), _task_queue(factory->CreateTaskQueue("packet_reader", TaskQueueFactory::Priority::NORMAL)),
	_socket(socket)
{
	_read_buf.reset(new uint8_t[BUF_LEN]);
	_task_queue.PostTask([this]() {
		Recv();
	});
}


void PacketReader::Stop()
{
	_stop = true;
}

void PacketReader::Recv()
{
	std::string ip;
	uint16_t port = 0;
	uint64_t received_time_microseconds = 0;
	while (!_stop) {
		auto read_len = _socket->Recv(ip, port, _read_buf.get(), BUF_LEN, received_time_microseconds);
		if (read_len == 0)
			continue;
		_quick->OnRecv(ip, port, _read_buf.get(), read_len, received_time_microseconds);
	}
}
