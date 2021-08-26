#include "quick_client.h"
#include <QtWidgets/QApplication>
#include "../dump/MiniDump.h"

int main(int argc, char *argv[])
{
	inject_core_handler();

    QApplication a(argc, argv);
    quick_client w;
    w.show();
    return a.exec();
}
