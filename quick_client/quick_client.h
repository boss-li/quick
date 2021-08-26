#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_quick_client.h"
#include "../quick/quick.h"
#include <QTimer>
#include "../thread/utility.h"

class quick_client : public QMainWindow, public QuickCallback
{
    Q_OBJECT

public:
    quick_client(QWidget *parent = Q_NULLPTR);
	~quick_client();

	void OnConnected(uint32_t session_id) override;
	void OnDisconnected(uint32_t session_id) override;
	void OnNewClientConnected(uint32_t session_id, const std::string &ip, uint16_t port) override;
	void OnMessage(uint32_t session_id, const uint8_t *data, uint16_t data_len) override;

private:
	void AppendTips(const QString& tips);
private slots:
	void on_btn_start_clicked();
	void on_btn_connect_clicked();
	void on_btn_disconnect_clicked();
	void on_btn_send_clicked();
	void on_btn_send_loop_clicked();
	void on_btn_connect_2_clicked();
	void on_btn_disconnect_2_clicked();
	void on_btn_send_2_clicked();
	void on_btn_send_loop_2_clicked();

	void on_btn_clear_clicked();
	void on_btn_export_debug_info_clicked();
	void OnMessage(uint32_t session_id, const QByteArray& data);
	void OnServerConnected(uint32_t session_id);
	void OnSessionDisconnected(uint32_t session_id);
	void OnTimer();
	void OnTimer2();
	void OnTimerPrint();
	void OnTimerProcess();

signals:
	void SigMessage(uint32_t session_id, const QByteArray& data);
	void SigConnected(uint32_t session_id);
	void SigDisconnected(uint32_t session_id);

private:
    Ui::quick_clientClass ui;

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

	std::unique_ptr<Quick> _quick;
	uint32_t _session_id = 0;
	uint64_t _total_sent_bytes = 0;
	uint64_t _total_recv_bytes = 0;
	uint64_t _recv_speed = 0;

	uint32_t _session_id2 = 0;
	uint64_t _total_sent_bytes2 = 0;
	uint64_t _total_recv_bytes2 = 0;
	uint64_t _recv_speed2 = 0;
	QTimer _timer;
	QTimer _timer2;
	QTimer _timer_print;
	uint32_t _clean_tick = 0;

	QTimer _timer_process;
	dd::Lock _lock;
	std::list<Packet> _wait_process_packets;
};
