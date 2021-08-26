#pragma once

#include <QtWidgets/QWidget>
#include "ui_quick_server.h"
#include "../../quick/quick/quick.h"
#include <QTimer>
#include <unordered_map>
#include "../../quick/thread/utility.h"
#include <list>

class quick_server : public QWidget, public QuickCallback
{
    Q_OBJECT

public:
    quick_server(QWidget *parent = Q_NULLPTR);

	void OnConnected(uint32_t session_id) override;
	void OnDisconnected(uint32_t session_id) override;
	void OnNewClientConnected(uint32_t session_id, const std::string &ip, uint16_t port) override;
	void OnMessage(uint32_t session_id, const uint8_t *data, uint16_t data_len) override;

private:
	void AppendTips(const QString& tips);
private slots:
	void on_btn_start_clicked();
	void on_btn_clear_clicked();
	void on_btn_disconnect_clicked();
	void on_btn_export_debug_info_clicked();
	void OnClientConnected(uint32_t session_id, const QString&ip, uint16_t port);
	void OnClientDisconnected(uint32_t session_id);
	void OnTimer();
	void OnTimerProcess();
	void OnStateChanged(bool state);
	void OnTimerSend();

signals:
	void SigNewClientConnected(uint32_t session_id, const QString& ip, uint16_t port);
	void SigDisconnected(uint32_t session_id);

private:

	struct Client {
		uint32_t session_id = 0;
		QString ip;
		uint16_t port;
		uint64_t total_received_bytes = 0;
		uint64_t total_send_bytes = 0;
		uint32_t receive_per_second = 0;

		Client(uint32_t session_id, const QString& ip, uint16_t port) {
			this->session_id = session_id;
			this->ip = ip;
			this->port = port;
		}
		Client() {}
	};
	struct Packet {
		uint32_t session_id;
		uint16_t data_len;
		std::unique_ptr<uint8_t> data;

		Packet() {}
		Packet(uint32_t session_id, uint16_t data_len, const uint8_t *data) {
			this->session_id = session_id;
			this->data_len = data_len;
			this->data.reset(new uint8_t[data_len]);
			memcpy(this->data.get(), data, data_len);
		}
	};
    Ui::quick_serverClass ui;
	std::unique_ptr<Quick> _quick;
	QTimer _timer;
	uint32_t _clean_tick = 0;
	std::unordered_map<uint32_t, Client> _clients;

	dd::Lock _lock;
	std::list<Packet> _wait_process_packets;
	QTimer _timer_process;
	QTimer _timer_send;
};
