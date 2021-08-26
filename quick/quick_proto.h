#pragma once
#include <memory>
#pragma  pack(push, 1)
enum QuickMsg {
	QUICK_MSG_UNKNOWN = 0,
	QUICK_MSG_HELLO,	// 客户端请求建立连接
	QUICK_MSG_HELLO_OK,	// 服务器同意建立连接
	QUICK_MSG_REJECT,	// 服务器拒绝连接
	QUICK_MSG_SESSION_ID_REAPEAT,	// 服务器通知客户端id重复，客户端需要更换id再连接
	QUICK_MSG_BYE,// 断开连接
	QUICK_MSG_PAYLOAD,	// 负载数据
	QUICK_MSG_ACK,	// 确认消息
	QUICK_MSG_HEARTBEAT,// 心跳消息
};

struct QuickHello {
	uint8_t msg_id = QUICK_MSG_HELLO;
	uint32_t session_id;
};

struct QuickHelloOk {
	uint8_t msg_id = QUICK_MSG_HELLO_OK;
	uint32_t session_id;
};

struct QuickHelloReject {
	uint8_t msg_id = QUICK_MSG_REJECT;
	uint32_t session_id;
};

struct QuickSessionIdRepeat {
	uint8_t msg_id = QUICK_MSG_SESSION_ID_REAPEAT;
	uint32_t session_id;
};

struct QuickBye {
	uint8_t msg_id = QUICK_MSG_BYE;
	uint32_t session_id;
};

struct QuickPayload {
	uint8_t msg_id = QUICK_MSG_PAYLOAD;
	uint32_t session_id;
	uint16_t packet_number;
	uint16_t sequence_number;
	uint64_t send_timestamp;
	uint8_t payload[1];	// payload的长度由整个udp包的长度减去 sizeof(QuickPayload)计算得到
};

struct QuickAck {
	uint8_t msg_id = QUICK_MSG_ACK;
	uint32_t session_id;
	uint16_t max_pkt_number_delay_us;	//此ack包中最新序号的包从接收到到发出ack消息所经过的微秒数。用于计算rtt
	uint16_t base_pkt_number = 0;  // 此ack包中的基准包号。一个ack消息最多可以确认17个包，其中后16个包的序号可通过base_pkt_number和pkt_number_mask得到
	uint16_t pkt_number_mask = 0;	// 包序号掩码，对应位为1表示包号为 base_pkt_number + N的包已收到， 为0表示未收到
	uint16_t previous_ack_base_pkt_number = 0;	// 前一个ack消息所确认的包情况
	uint16_t previous_ack_pkt_number_mask = 0;	// 前一个ack消息所确认的包情况。增加这两个字段是考虑ack消息也有可能会丢失，通过两次确认可以让发送端
	// 减少因为ack消息丢失而导致的错误重传
};

struct QuickHeartbeat {
	uint8_t msg_id = QUICK_MSG_HEARTBEAT;
	uint32_t session_id;
};

#pragma pack(pop)
