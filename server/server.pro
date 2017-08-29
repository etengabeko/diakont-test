TEMPLATE = app
PROJECT = server
TARGET = $$PROJECT

CONFIG += console
CONFIG -= app_bundle

QT += core \
      network
QT -= gui

CONFIG += warn_on

QMAKE_CXXFLAGS += -Wall -Werror -Wextra -pedantic-errors
QMAKE_CXXFLAGS += -std=c++14

DESTDIR = $$PWD/build/bin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc

SOURCES += \
    src/server.cpp \
    src/main.cpp

HEADERS += \
    src/server.h

# installs
target.path = $$PREFIX/bin

INSTALLS += \
    target

INCLUDEPATH += $$PREFIX/include

LIBS += -L$$PREFIX/lib -lprotocol
