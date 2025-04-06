QT += core gui widgets charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QtWidgetsTCmonitor
TEMPLATE = app

CONFIG += c++20

SOURCES += \
    main.cpp \
    QtWidgetsTCmonitor.cpp \
    stdafx.cpp

HEADERS += \
    QtWidgetsTCmonitor.h \
    stdafx.h

FORMS += \
    QtWidgetsTCmonitor.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

INCLUDEPATH += $$PWD

# Add explicit path for QtCharts
INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtCharts

# Ensure necessary Qt modules are linked
QT += core gui widgets
