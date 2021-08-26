#include <windows.h>
#include <dbghelp.h>
#include <QDir>
#include <QTime>
#include <QDebug>

#define DUMP_PATH "./dmp/"


static
long ApplicationCrashHandler(EXCEPTION_POINTERS *pException){
{
        // 在程序exe的目录中创建dmp文件夹
        QDir *dmp = new QDir;
        bool exist = dmp->exists(DUMP_PATH);
        if(exist == false)
            dmp->mkdir(DUMP_PATH);
        }

        QDateTime current_date_time = QDateTime::currentDateTime();
        QString current_date = QString::number(current_date_time.toString("dd").toInt() % 5);
        QString time =  current_date + ".dmp";
        EXCEPTION_RECORD *record = pException->ExceptionRecord;
        QString errCode(QString::number(record->ExceptionCode, 16));
        QString errAddr(QString::number((uint)record->ExceptionAddress, 16));
        QString errFlag(QString::number(record->ExceptionFlags, 16));
        QString errPara(QString::number(record->NumberParameters, 16));
        qDebug()<<"errCode: "<<errCode;
        qDebug()<<"errAddr: "<<errAddr;
        qDebug()<<"errFlag: "<<errFlag;
        qDebug()<<"errPara: "<<errPara;
        HANDLE hDumpFile = CreateFile((LPCWSTR)QString(DUMP_PATH + time).utf16(),
               GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(hDumpFile != INVALID_HANDLE_VALUE) {
          MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
          dumpInfo.ExceptionPointers = pException;
          dumpInfo.ThreadId = GetCurrentThreadId();
          dumpInfo.ClientPointers = TRUE;
          MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),hDumpFile, MiniDumpWithFullMemory, &dumpInfo, NULL, NULL);
          CloseHandle(hDumpFile);
        }
        else{
          qDebug()<<"hDumpFile == null";
        }
        return EXCEPTION_EXECUTE_HANDLER;
}


void inject_core_handler() {
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);
}
