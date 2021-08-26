#pragma once
#include <assert.h>
#include <memory>
#include <atomic>
#define MAX_PACKET_COUNT 0xFFFF
template <typename T>
class RingBuffer final{
public:
	explicit RingBuffer(uint16_t capacity) :
	_capacity(capacity){
		assert(capacity > 2 && capacity < MAX_PACKET_COUNT);
		_buffer = new T[_capacity];
		_packet_count = 0;
	}
	~RingBuffer() {
		if (_buffer)
			delete[] _buffer;
	}
	RingBuffer& operator =(const RingBuffer&) = delete;
	RingBuffer& operator =(const RingBuffer&&) = delete;
	RingBuffer(const RingBuffer&) = delete;
	RingBuffer(const RingBuffer&&) = delete;

	T* GetWriteablePacket() {
		if (_capacity == _packet_count)
			return nullptr;
		return &_buffer[_write_index];
	}
	void WritePacket() {
		_write_index = (_write_index + 1 >= _capacity) ? 0 : _write_index + 1;
		_packet_count++;
	}

	T* GetReadablepacket() {
		if(_packet_count == 0)
			return nullptr;
		return &_buffer[_read_index];
	}

	void RemovePacket() {
		_read_index = (_read_index + 1 >= _capacity) ? 0 : _read_index + 1;
		_packet_count--;
	}

	uint16_t GetPacketCount() {
		return _packet_count;
	}

	uint16_t GetCapacity() {
		return _capacity;
	}
private:
	int _capacity;
	uint16_t _read_index = 0;
	uint16_t _write_index = 0;
	std::atomic<uint16_t> _packet_count;
	T* _buffer = nullptr;
};
