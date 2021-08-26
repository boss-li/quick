#pragma once
#include <memory>
#include <string>

#ifdef _WIN32 
#define QUICK_EXPORT __declspec(dllexport)
#else
#define QUICK_EXPORT
#endif

/*
	quickģ����һ������udpЭ��Ŀɿ����ٴ���ģ�飬ģ��Ϊ�ϲ��ṩ�˷��������ͻ��˵�����ʹ�÷�ʽ��
	һ���������ɽ��ն���ͻ��˵����ӣ�һ���ͻ���Ҳ������������������ӣ����ӽ�����˫���Ϳ��Խ������ݴ��䡣

	quickģ���ڲ�ʹ����bbrӵ�������㷨��������㷨ʵ�ִ������ԣ�
	\chromium\src\net\third_party\quiche\src\quic\core\congestion_control�µ�bbr2_xxx����ļ�

	bbr�㷨�ĵ���https://tools.ietf.org/pdf/draft-cardwell-iccrg-bbr-congestion-control-00.pdf
*/

#define MAX_QUICK_PACKET_SIZE 1350

class QuickCallback {
public:
	virtual ~QuickCallback() {}
	virtual void OnConnected(uint32_t session_id) = 0;
	virtual void OnDisconnected(uint32_t session_id) = 0;
	virtual void OnNewClientConnected(uint32_t session_id, const std::string &ip, uint16_t port) = 0;
	virtual void OnMessage(uint32_t session_id, const uint8_t *data, uint16_t data_len) = 0;
};

class  QUICK_EXPORT Quick {
public:
	static std::unique_ptr<Quick> Create(uint16_t port, bool as_server, QuickCallback* cb);

	virtual uint32_t ConnectToServer(const std::string& ip, uint16_t port) = 0;
	virtual bool Disconnect(uint32_t session_id) = 0;
	virtual bool Send(uint32_t session_id, const uint8_t* data, uint16_t data_len) = 0;

	// for debug
	virtual void ExportDebugInfo(uint32_t session_id) = 0;
};
