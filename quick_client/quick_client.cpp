#include "quick_client.h"
#include <QDateTime>
#include <thread>
#include <chrono>
#include <random>

quick_client::quick_client(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
	qRegisterMetaType<uint32_t>("uint32_t");
	connect(this, SIGNAL(SigMessage(uint32_t, const QByteArray&)), this, SLOT(OnMessage(uint32_t, const QByteArray&)));
	connect(this, SIGNAL(SigConnected(uint32_t)), this, SLOT(OnServerConnected(uint32_t)));
	connect(this, SIGNAL(SigDisconnected(uint32_t)), this, SLOT(OnSessionDisconnected(uint32_t)));
	connect(&_timer, &QTimer::timeout, this, &quick_client::OnTimer);
	connect(&_timer2, &QTimer::timeout, this, &quick_client::OnTimer2);
	connect(&_timer_print, &QTimer::timeout, this, &quick_client::OnTimerPrint);
	connect(&_timer_process, &QTimer::timeout, this, &quick_client::OnTimerProcess);
	_timer_process.start(10);
	_timer_print.start(1000);
}

quick_client::~quick_client()
{
	_timer.stop();
	_timer2.stop();
	on_btn_disconnect_clicked();
	on_btn_disconnect_2_clicked();
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void quick_client::OnConnected(uint32_t session_id)
{
	emit SigConnected(session_id);
}

void quick_client::OnDisconnected(uint32_t session_id)
{
	emit SigDisconnected(session_id);
}

void quick_client::OnNewClientConnected(uint32_t session_id, const std::string &ip, uint16_t port)
{

}

void quick_client::OnMessage(uint32_t session_id, const uint8_t *data, uint16_t data_len)
{
	// 在这里，使用锁的方式要比使用 发信号的方式效率更高，占用cpu更低。
	dd::AutoLock lock(_lock);
	_wait_process_packets.push_back(Packet(session_id, data_len, data));
}

void quick_client::OnMessage(uint32_t session_id, const QByteArray& data)
{
//	AppendTips(QString("<%1> recv msg: %2").arg(_session_id).arg(data.data()));
}

void quick_client::OnServerConnected(uint32_t session_id)
{
	AppendTips(QString::fromLocal8Bit("服务器已连接, session id = %1").arg(session_id));
}

void quick_client::OnSessionDisconnected(uint32_t session_id)
{
	AppendTips(QString::fromLocal8Bit("连接断开, session id = %1").arg(session_id));
	if (_session_id == session_id) {
		ui.btn_connect->setEnabled(true);
		ui.btn_disconnect->setEnabled(false);
	}
	else if (_session_id2 == session_id) {
		ui.btn_connect_2->setEnabled(true);
		ui.btn_disconnect_2->setEnabled(false);
	}
}

void quick_client::OnTimer()
{ 
#define DATA_SIZE 1350
	static uint8_t buf[DATA_SIZE];
	//QString msg = ui.le_msg_loop->text();
	//msg = QString("%1, %2").arg(msg).arg(_message_index++);
	static bool ret = false;
	static std::mt19937 rng;
	rng.seed(std::random_device()());
	static std::uniform_int_distribution<std::mt19937::result_type> dist6(1000, DATA_SIZE);

	int loop_count = 20;
	uint16_t data_len = 0;
	while (loop_count >= 0) {
		while (true) {
			//ret = _quick->Send(_session_id, (const uint8_t*)msg.toStdString().c_str(), msg.toStdString().length());
			data_len = dist6(rng);
			ret = _quick->Send(_session_id, buf, data_len);
			if (ret) {
				//_total_sent_bytes += msg.toStdString().length();
				_total_sent_bytes += data_len;
				break;
			}
			else {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		loop_count--;
	}
}

void quick_client::OnTimer2()
{
#define DATA_SIZE 1350
	static uint8_t buf[DATA_SIZE];
	static bool ret = false;
	static std::mt19937 rng;
	rng.seed(std::random_device()());
	static std::uniform_int_distribution<std::mt19937::result_type> dist6(1000, DATA_SIZE);

	int loop_count = 20;
	uint16_t data_len = 0;
	while (loop_count >= 0) {
		while (true) {
			data_len = dist6(rng);
			ret = _quick->Send(_session_id2, buf, data_len);
			if (ret) {
				_total_sent_bytes2 += data_len;
				break;
			}
			else {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		loop_count--;
	}
}

void quick_client::OnTimerPrint()
{
	QString msg;
	if (_session_id) {
		//msg = QString("session %1 total sent %2 bytes").arg(_session_id).arg(_total_sent_bytes);
		//AppendTips(msg);
		float speed = (float)_recv_speed / 1024.0 / 1024.0;
		AppendTips(QString("session %1 recv speed: %3 Mbyte/s").arg(_session_id).arg(speed));
		_recv_speed = 0;
	}
	if (_session_id2) {
		/*msg = QString("session %1 total sent %2 bytes").arg(_session_id2).arg(_total_sent_bytes2);
		AppendTips(msg);*/
		float speed = (float)_recv_speed2 / 1024.0 / 1024.0;
		AppendTips(QString("session %1 recv speed: %3 Mbyte/s").arg(_session_id2).arg(speed));
		_recv_speed2 = 0;
	}

	ui.label_total_send->setText(QString("%1").arg(_total_sent_bytes));
	ui.label_total_send_2->setText(QString("%1").arg(_total_sent_bytes2));
	ui.label_total_recv->setText(QString("%1").arg(_total_recv_bytes));
	ui.label_total_recv_2->setText(QString("%1").arg(_total_recv_bytes2));
	_clean_tick++;
	if (_clean_tick >= 600) {
		_clean_tick = 0;
		on_btn_clear_clicked();
	}
}

void quick_client::OnTimerProcess()
{
	dd::AutoLock lock(_lock);
	for (auto iter = _wait_process_packets.begin(); iter != _wait_process_packets.end();) {
		if (iter->session_id == _session_id) {
			_total_recv_bytes += iter->data_len;
			_recv_speed += iter->data_len;
		}
		else if (iter->session_id == _session_id2) {
			_total_recv_bytes2 += iter->data_len;
			_recv_speed2 += iter->data_len;
		}

		iter = _wait_process_packets.erase(iter);
	}
}

void quick_client::AppendTips(const QString& tips)
{
	QString str = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss:zzz ^ ");
	ui.textBrowser->append(str + tips);
}

void quick_client::on_btn_start_clicked()
{
	uint16_t port = ui.le_port->text().toInt();
	_quick = Quick::Create(port, false, this);
	AppendTips(QString::fromLocal8Bit("开始监听本地udp端口：%1").arg(port));
	ui.btn_start->setEnabled(false);
}

void quick_client::on_btn_connect_clicked()
{
	if (_quick == nullptr)
		return;
	QString ip = ui.le_dst_ip->text();
	uint16_t port = ui.le_dst_port->text().toInt();
	_session_id = _quick->ConnectToServer(ip.toStdString(), port);
	ui.btn_connect->setEnabled(false);
	ui.btn_disconnect->setEnabled(true);
	_total_sent_bytes = 0;
	ui.label_total_send->setText("0"); 
}

void quick_client::on_btn_disconnect_clicked()
{
	_timer.stop();
	ui.btn_send_loop->setText(QString::fromLocal8Bit("定时发送"));
	//	_timer_print.stop();
	OnTimerPrint();

	_quick->Disconnect(_session_id);
	AppendTips(QString("disconnect session %1").arg(_session_id));
	_session_id = 0;
	ui.btn_disconnect->setEnabled(false);
	ui.btn_connect->setEnabled(true);
}

void quick_client::on_btn_send_clicked()
{
	QString msg = ui.le_msg->text();
	bool ret = _quick->Send(_session_id, (const uint8_t*)msg.toStdString().c_str(), msg.toStdString().length());
	if (ret) {
		AppendTips(QString("<%1> send msg: %2").arg(_session_id).arg(msg));
		_total_sent_bytes += msg.toStdString().length();
	}
}

void quick_client::on_btn_send_loop_clicked()
{
	if (_session_id == 0)
		return;
	if (_timer.isActive()) {
		_timer.stop();
		ui.btn_send_loop->setText(QString::fromLocal8Bit("定时发送"));
	//	_timer_print.stop();
		OnTimerPrint();
	}
	else {
		uint32_t interval = ui.le_send_interval->text().toInt();
		ui.btn_send_loop->setText(QString::fromLocal8Bit("停止发送"));
		_timer.start(interval);
	}
}

void quick_client::on_btn_connect_2_clicked()
{
	if (_quick == nullptr)
		return;
	QString ip = ui.le_dst_ip_2->text();
	uint16_t port = ui.le_dst_port_2->text().toInt();
	_session_id2 = _quick->ConnectToServer(ip.toStdString(), port);
	ui.btn_connect_2->setEnabled(false);
	ui.btn_disconnect_2->setEnabled(true);
	_total_sent_bytes2 = 0;
	ui.label_total_send_2->setText("0");
}

void quick_client::on_btn_disconnect_2_clicked()
{
	_timer2.stop();
	ui.btn_send_loop_2->setText(QString::fromLocal8Bit("定时发送"));
	//	_timer_print.stop();
	OnTimerPrint();

	_quick->Disconnect(_session_id2);
	AppendTips(QString("disconnect session %1").arg(_session_id2));
	_session_id2 = 0;
	ui.btn_disconnect_2->setEnabled(false);
	ui.btn_connect_2->setEnabled(true);
}

void quick_client::on_btn_send_2_clicked()
{
	QString msg = ui.le_msg_2->text();
	bool ret = _quick->Send(_session_id2, (const uint8_t*)msg.toStdString().c_str(), msg.toStdString().length());
	if (ret) {
		AppendTips(QString("<%1> send msg: %2").arg(_session_id2).arg(msg));
		_total_sent_bytes2 += msg.toStdString().length();
	}
}

void quick_client::on_btn_send_loop_2_clicked()
{
	if (_session_id2 == 0)
		return;
	if (_timer2.isActive()) {
		_timer2.stop();
		ui.btn_send_loop_2->setText(QString::fromLocal8Bit("定时发送"));
	//	_timer_print.stop();
		OnTimerPrint();
	}
	else {
		uint32_t interval = ui.le_send_interval_2->text().toInt();
		ui.btn_send_loop_2->setText(QString::fromLocal8Bit("停止发送"));
		_timer2.start(interval);
	}
}

void quick_client::on_btn_clear_clicked()
{
	//_total_sent_bytes = 0;
	//_message_index = 0;
	ui.textBrowser->clear();
}

void quick_client::on_btn_export_debug_info_clicked()
{
	_quick->ExportDebugInfo(_session_id);
}
