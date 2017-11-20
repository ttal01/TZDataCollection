
QT += core
QT += network
QT += script
QT -= gui

TARGET = TZDataCollection
CONFIG += console
CONFIG -= app_bundle
CONFIG += serialport

TEMPLATE = app

SOURCES += main.cpp \
           PublicFun.cpp \
           ConfigFile.cpp \
           TCPClent.cpp \
           S7PLC200Protocol.cpp \
           S7PLC300Protocol.cpp \
           S7PLC400Protocol.cpp \
           zip.cpp \
           MyFTP.cpp \
            ModBusProtocol.cpp \
            serialport.cpp \
            socketcan.cpp \
            gpsProtocol.cpp \
    FtpCLient.cpp \
    AIInter.cpp \
    DIInter.cpp \
    datacomlayer.cpp

HEADERS +=  DAType.h  \
            EDAType.h \
            PublicFun.h \
            ConfigFile.h \
            TCPClent.h \
            S7Protocol.h \
            zip.h \
            miniz.h \
            MyFTP.h \
            serialport.h \
            ./zeromq-4.2.1/include/zmq.h \
            ./zeromq-4.2.1/include/zmq_utils.h \
            socketcan.h \
    FtpCLient.h

include(./log4qt/src/log4qt/log4qt.pri)

LIBS += -L./zeromq-4.2.1/lib -lzmq
DEFINES += __TIMESTAMP_ISO__=$(shell date +'\'"\\\"%Y-%m-%d %H:%M:%S\\\""\'')
INCLUDEPATH += ./zeromq-4.2.1/include/

