#pragma once
#include <memory>
#include <string>

#ifdef _WIN32 
#define QUICK_EXPORT __declspec(dllexport)
#else
#define QUICK_EXPORT
#endif

/*
	quick模块是一个基于udp协议的可靠快速传输模块，模块为上层提供了服务器、客户端的两种使用方式。
	一个服务器可接收多个客户端的连接，一个客户端也可以往多个服务器连接，连接建立后双方就可以进行数据传输。

	quick模块内部使用了bbr拥塞控制算法，具体的算法实现代码来自：
	\chromium\src\net\third_party\quiche\src\quic\core\congestion_control下的bbr2_xxx相关文件

	bbr算法文档：https://tools.ietf.org/pdf/draft-cardwell-iccrg-bbr-congestion-control-00.pdf
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
