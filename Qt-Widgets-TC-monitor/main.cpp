#include "stdafx.h"  // 放在最前面
#include <QtWidgets/QApplication>
#include "QtWidgetsTCmonitor.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QtWidgetsTCmonitor w;
    w.show();
    return a.exec();
}
