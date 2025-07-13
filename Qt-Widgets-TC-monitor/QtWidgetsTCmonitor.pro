QT += core gui widgets charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = QtWidgetsTCmonitor
TEMPLATE = app

CONFIG += c++20

# Generate ui_*.h files
CONFIG += uic

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
INCLUDEPATH += ../src
INCLUDEPATH += ../src/core

# Add explicit paths for QtCharts
INCLUDEPATH += $$[QT_INSTALL_HEADERS]/QtCharts
DEPENDPATH += $$[QT_INSTALL_HEADERS]/QtCharts

# Add Windows libraries for shared memory and system info
win32 {
    LIBS += -lkernel32 -luser32 -ladvapi32 -lole32 -loleaut32
}

# Link to the core library objects (if needed)
OBJECTS += \
    ../src/core/DataStruct/SharedMemoryManager.o \
    ../src/core/utils/Logger.o

# Make sure all files are saved as UTF-8
CODECFORSRC = UTF-8

# Enable console for debugging (optional)
CONFIG += console
