#include "quick_server.h"
#include <QDateTime>
#include <QDebug>
#include <random>

quick_server::quick_server(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
	qRegisterMetaType<uint32_t>("uint32_t");
	qRegisterMetaType<uint16_t>("uint16_t");
	connect(this, SIGNAL(SigNewClientConnected(uint32_t, const QString&, uint16_t)), this, SLOT(OnClientConnected(uint32_t, const QString&, uint16_t)));
	connect(this, SIGNAL(SigDisconnected(uint32_t)), this, SLOT(OnClientDisconnected(uint32_t)));
	connect(&_timer, &QTimer::timeout, this, &quick_server::OnTimer);
	connect(&_timer_process, &QTimer::timeout, this, &quick_server::OnTimerProcess);
	connect(&_timer_send, &QTimer::timeout, this, &quick_server::OnTimerSend);
	connect(ui.checkBox, &QCheckBox::stateChanged, this, &quick_server::OnStateChanged);

	_timer.start(1000);
	_timer_process.start(10);
	on_btn_start_clicked();
}

void quick_server::OnConnected(uint32_t session_id)
{
//	emit SigNewClientConnected(session_id);
}

void quick_server::OnDisconnected(uint32_t session_id)
{
	emit SigDisconnected(session_id);
}

void quick_server::OnNewClientConnected(uint32_t session_id, const std::string &ip, uint16_t port)
{
	emit SigNewClientConnected(session_id, ip.c_str(), port);
}

void quick_server::OnMessage(uint32_t session_id, const uint8_t *data, uint16_t data_len)
{
	// 在这里，使用锁的方式要比使用 发信号的方式效率更高，占用cpu更低。
	dd::AutoLock lock(_lock);
	_wait_process_packets.push_back(Packet(session_id, data_len, data));
}

void quick_server::OnClientConnected(uint32_t session_id, const QString&ip, uint16_t port)
{
	Client c(session_id, ip, port);
	_clients[session_id] = c;
	AppendTips(QString("new client connected<%1:%2>, session id = %3").arg(ip).arg(port).arg(session_id));

	int row = ui.tableWidget->rowCount();
	ui.tableWidget->insertRow(row);
	ui.tableWidget->setItem(row, 0, new QTableWidgetItem(QString("%1").arg(session_id)));
	ui.tableWidget->setItem(row, 1, new QTableWidgetItem(QString("%1:%2").arg(ip).arg(port)));
	ui.tableWidget->setItem(row, 2, new QTableWidgetItem(QString("%1").arg(0)));
	ui.tableWidget->setItem(row, 3, new QTableWidgetItem(QString("%1").arg(0)));
}

void quick_server::OnClientDisconnected(uint32_t session_id)
{
	auto iter = _clients.find(session_id);
	if (iter == _clients.end())
		return;
	_clients.erase(iter);
	AppendTips(QString("client disconnected, session id = %1").arg(session_id));

	for (int i = 0; i < ui.tableWidget->rowCount(); i++) {
		auto item = ui.tableWidget->item(i, 0);
		if (item->text().toUInt() == session_id) {
			ui.tableWidget->removeRow(i);
			break;
		}
	}
}

void quick_server::OnTimer()
{
	_clean_tick++;
	if (_clean_tick >= 600) {
		_clean_tick = 0;
		on_btn_clear_clicked();
	}

	int row = 0;
	for (auto &iter : _clients) {
		auto recv_item = ui.tableWidget->item(row, 2);
		auto send_item = ui.tableWidget->item(row, 3);
		recv_item->setText(QString("%1").arg(iter.second.total_received_bytes));
		send_item->setText(QString("%1").arg(iter.second.total_send_bytes));

		float speed = (float)iter.second.receive_per_second / 1024.0 / 1024.0;
		AppendTips(QString("session %1 recv speed: %2 Mbyte/s").arg(iter.second.session_id).arg(speed));
		iter.second.receive_per_second = 0;

		row++;
	}
}

void quick_server::OnTimerProcess()
{
	auto count = 0;
	dd::AutoLock lock(_lock);
	for (auto iter = _wait_process_packets.begin(); iter != _wait_process_packets.end();) {
		auto temp = _clients.find(iter->session_id);
		if (temp != _clients.end()) {
			temp->second.total_received_bytes += iter->data_len;
			temp->second.receive_per_second += iter->data_len;
		}

		iter = _wait_process_packets.erase(iter);
		count++;
	/*	if(count >= 30)
			break;*/
	}
}

void quick_server::OnStateChanged(bool state)
{
	if (ui.checkBox->isChecked()) {
		int interval = ui.le_interval->text().toUInt();
		_timer_send.start(interval);
	}
	else {
		_timer_send.stop();
	}
}

void quick_server::OnTimerSend()
{
#define DATA_SIZE 1350
	static uint8_t buf[DATA_SIZE];
	//QString msg = ui.le_msg_loop->text();
	//msg = QString("%1, %2").arg(msg).arg(_message_index++);
	static bool ret = false;
	static std::mt19937 rng;
	rng.seed(std::random_device()());
	static std::uniform_int_distribution<std::mt19937::result_type> dist6(1000, DATA_SIZE);

	uint16_t data_len = 0;
	data_len = dist6(rng);
	for (auto &iter : _clients) {
		data_len = dist6(rng);
		ret = _quick->Send(iter.first, buf, data_len);
		if (ret)
			iter.second.total_send_bytes += data_len;
	}
}

void quick_server::AppendTips(const QString& tips)
{
	auto time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss:zzz ^");
	ui.textBrowser->append(time + tips);
}

void quick_server::on_btn_start_clicked()
{
	uint16_t port = ui.le_port->text().toUInt();
	_quick = Quick::Create(port, true, this);
	AppendTips(QString("start server , local udp port : %1").arg(port));
	ui.btn_start->setEnabled(false);
}

void quick_server::on_btn_clear_clicked()
{
	ui.textBrowser->clear();
}

void quick_server::on_btn_disconnect_clicked()
{
	int row = ui.tableWidget->currentRow();
	if (row < 0)
		return;
	uint32_t session_id = ui.tableWidget->item(row, 0)->text().toUInt();
	_quick->Disconnect(session_id);
	AppendTips(QString::fromLocal8Bit("断开客户端 %1").arg(session_id));
	OnClientDisconnected(session_id);
}

void quick_server::on_btn_export_debug_info_clicked()
{
//	_quick->ExportDebugInfo(_session_id);
}
