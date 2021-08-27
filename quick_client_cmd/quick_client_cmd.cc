// quick_client_cmd.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <memory>
#include <string>
#include "../quick/quick.h"
#include <chrono>
#include <thread>
#include "simple_client.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <signal.h>
#endif


void PrintUsage() {
	std::cout << "quick_client_cmd -local_port=xxx -server_ip=xxx.xxx.xxx.xxx -server_port=xxx -file=/home/xxx.xxx[optional]\r\n";
	std::cout << "if no -file parameter, quick_client_cmd will loop send verbose msg to server\r\n";
	std::cout << "press key p and enter to start/stop send\r\n";
}

void ParseParams(int argc, char* argv[], uint16_t &local_port, std::string& server_ip, 
	uint16_t &server_port, std::string& send_file) {
	std::string k_local_port = "-local_port=";
	std::string k_server_ip = "-server_ip=";
	std::string k_server_port = "-server_port=";
	std::string k_file = "-file=";
	for (int i = 0; i < argc; i++) {
		std::string str = argv[i];
		int index = -1;
		index = str.find(k_local_port);
		if (index != -1) {
			std::string v = str.substr(index + k_local_port.length());
			local_port = atoi(v.c_str());
			continue;
		}

		index = str.find(k_server_ip);
		if (index != -1) {
			server_ip = str.substr(index + k_server_ip.length());
			continue;
		}

		index = str.find(k_server_port);
		if (index != -1) {
			std::string v = str.substr(index + k_server_port.length());
			server_port = atoi(v.c_str());
			continue;
		}

		index = str.find(k_file);
		if (index != -1) {
			send_file = str.substr(index + k_file.length());
			continue;
		}
	}
}

bool g_running = true;
bool g_sending = true;

#ifdef _WIN32
bool KeyEventHandler(DWORD ctrl_type) {
	switch (ctrl_type) {
	case CTRL_C_EVENT:
		g_sending = !g_sending;
		return true;
		break;
	case CTRL_BREAK_EVENT:
		g_running = false;
		return true;
		break;
	default:
		return false;
	}
	return false;
}
#endif

void StopSend(int signo) {
	g_sending = !g_sending;
}

void WaitInputThread() {
	std::string input;
	while (true) {
		std::cin >> input;
		if (input == "p")
			g_sending = !g_sending;
		std::cout << ">";
	}
}

int main(int argc, char* argv[])
{
	if (argc < 4) {
		PrintUsage();
		return -1;
	}

	std::thread  input_thread(WaitInputThread);

	uint16_t local_port = 0;
	std::string server_ip;
	uint16_t server_port = 0;
	std::string send_file;
	ParseParams(argc, argv, local_port, server_ip, server_port, send_file);
	std::unique_ptr<SimpleClient> client(new SimpleClient(local_port, server_ip, server_port, send_file));

#ifdef _WIN32
//	SetConsoleCtrlHandler((PHANDLER_ROUTINE)KeyEventHandler, true);
#else
//	signal(SIGINT, StopSend);
#endif

	while (g_running) {
		if (g_sending && !client->Process())
			break;
		else {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			client->PrintInfo();
		}
	}
	client->PrintInfo();
	input_thread.join();
	std::this_thread::sleep_for(std::chrono::milliseconds(1500));
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
