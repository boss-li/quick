// quick_server_cmd.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "simple_server.h"
#include <thread>

void PrintUsage() {
	std::cout << "quick_server_cmd -local_port=xxx\r\n";
}

void ParseParams(int argc, char* argv[], uint16_t &local_port) {
	std::string key = "-local_port=";
	for (int i = 0; i < argc; i++) {
		std::string str = argv[i];
		int index = str.find(key);
		if (index != -1) {
			int index2 = str.find(" ", index);
			std::string v = str.substr(index + key.length());
			local_port = atoi(v.c_str());
			break;
		}
	}
}

bool g_running = true;
int main(int argc, char* argv[])
{
	if (argc < 1) {
		PrintUsage();
		return -1;
	}
	uint16_t local_port = 0;
	ParseParams(argc, argv, local_port);
	if (local_port == 0) {
		PrintUsage();
		return -1;
	}
	std::unique_ptr<SimpleServer> server(new SimpleServer(local_port));
	while (g_running) {
		server->Process();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
    std::cout << "Hello World!\n";
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
