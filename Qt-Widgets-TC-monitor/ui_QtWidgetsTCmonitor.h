/********************************************************************************
** Form generated from reading UI file 'QtWidgetsTCmonitor.ui'
**
** Created by: Qt User Interface Compiler version 6.10.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_QTWIDGETSTCMONITOR_H
#define UI_QTWIDGETSTCMONITOR_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_QtWidgetsTCmonitorClass
{
public:
    QWidget *centralWidget;
    QVBoxLayout *verticalLayout;
    QPushButton *pushButton;
    QMenuBar *menuBar;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *QtWidgetsTCmonitorClass)
    {
        if (QtWidgetsTCmonitorClass->objectName().isEmpty())
            QtWidgetsTCmonitorClass->setObjectName("QtWidgetsTCmonitorClass");
        QtWidgetsTCmonitorClass->resize(800, 600);
        centralWidget = new QWidget(QtWidgetsTCmonitorClass);
        centralWidget->setObjectName("centralWidget");
        verticalLayout = new QVBoxLayout(centralWidget);
        verticalLayout->setSpacing(6);
        verticalLayout->setContentsMargins(11, 11, 11, 11);
        verticalLayout->setObjectName("verticalLayout");
        pushButton = new QPushButton(centralWidget);
        pushButton->setObjectName("pushButton");
        pushButton->setMinimumSize(QSize(100, 30));

        verticalLayout->addWidget(pushButton);

        QtWidgetsTCmonitorClass->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(QtWidgetsTCmonitorClass);
        menuBar->setObjectName("menuBar");
        menuBar->setGeometry(QRect(0, 0, 800, 22));
        QtWidgetsTCmonitorClass->setMenuBar(menuBar);
        statusBar = new QStatusBar(QtWidgetsTCmonitorClass);
        statusBar->setObjectName("statusBar");
        QtWidgetsTCmonitorClass->setStatusBar(statusBar);

        retranslateUi(QtWidgetsTCmonitorClass);

        QMetaObject::connectSlotsByName(QtWidgetsTCmonitorClass);
    } // setupUi

    void retranslateUi(QMainWindow *QtWidgetsTCmonitorClass)
    {
        QtWidgetsTCmonitorClass->setWindowTitle(QCoreApplication::translate("QtWidgetsTCmonitorClass", "System Monitor", nullptr));
        pushButton->setText(QCoreApplication::translate("QtWidgetsTCmonitorClass", "Test", nullptr));
    } // retranslateUi

};

namespace Ui {
    class QtWidgetsTCmonitorClass: public Ui_QtWidgetsTCmonitorClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_QTWIDGETSTCMONITOR_H
