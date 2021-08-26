#pragma once
#include <memory>
#pragma  pack(push, 1)
enum QuickMsg {
	QUICK_MSG_UNKNOWN = 0,
	QUICK_MSG_HELLO,	// �ͻ�������������
	QUICK_MSG_HELLO_OK,	// ������ͬ�⽨������
	QUICK_MSG_REJECT,	// �������ܾ�����
	QUICK_MSG_SESSION_ID_REAPEAT,	// ������֪ͨ�ͻ���id�ظ����ͻ�����Ҫ����id������
	QUICK_MSG_BYE,// �Ͽ�����
	QUICK_MSG_PAYLOAD,	// ��������
	QUICK_MSG_ACK,	// ȷ����Ϣ
	QUICK_MSG_HEARTBEAT,// ������Ϣ
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
	uint8_t payload[1];	// payload�ĳ���������udp���ĳ��ȼ�ȥ sizeof(QuickPayload)����õ�
};

struct QuickAck {
	uint8_t msg_id = QUICK_MSG_ACK;
	uint32_t session_id;
	uint16_t max_pkt_number_delay_us;	//��ack����������ŵİ��ӽ��յ�������ack��Ϣ��������΢���������ڼ���rtt
	uint16_t base_pkt_number = 0;  // ��ack���еĻ�׼���š�һ��ack��Ϣ������ȷ��17���������к�16��������ſ�ͨ��base_pkt_number��pkt_number_mask�õ�
	uint16_t pkt_number_mask = 0;	// ��������룬��ӦλΪ1��ʾ����Ϊ base_pkt_number + N�İ����յ��� Ϊ0��ʾδ�յ�
	uint16_t previous_ack_base_pkt_number = 0;	// ǰһ��ack��Ϣ��ȷ�ϵİ����
	uint16_t previous_ack_pkt_number_mask = 0;	// ǰһ��ack��Ϣ��ȷ�ϵİ�����������������ֶ��ǿ���ack��ϢҲ�п��ܻᶪʧ��ͨ������ȷ�Ͽ����÷��Ͷ�
	// ������Ϊack��Ϣ��ʧ�����µĴ����ش�
};

struct QuickHeartbeat {
	uint8_t msg_id = QUICK_MSG_HEARTBEAT;
	uint32_t session_id;
};

#pragma pack(pop)
