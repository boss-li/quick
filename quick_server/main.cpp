#include "quick_server.h"
#include <QtWidgets/QApplication>
#ifdef _WIN32
#include "../dump/MiniDump.h"
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
	inject_core_handler();
#endif
    QApplication a(argc, argv);
    quick_server w;
    w.show();
    return a.exec();
}
