#pragma once
#include <memory>

#pragma pack(push, 1)
enum Q_MSG {
	Q_MSG_FILE_BEGIN = 0,
	Q_MSG_FILE_DATA,
	Q_MSG_FILE_END,
	Q_MSG_VERBOSE,
};

struct FileBegin {
	uint8_t msg_id;
	uint8_t file_name[128];
};
struct FileData {
	uint8_t msg_id;
	uint8_t data[1];
};
struct FileEnd {
	uint8_t msg_id;
};
struct Verbose {
	uint8_t msg_id;
	uint8_t data[1];
};

#pragma pack(pop)
