TEMPLATE = app
PROJECT = serialize-test
TARGET = $$PROJECT

QT += core \
      xml \
      testlib
QT -= gui

CONFIG += warn_on
QMAKE_CXXFLAGS += -Wall -Werror -Wextra -pedantic-errors
QMAKE_CXXFLAGS += -std=c++14

DESTDIR = $$PWD/build/sbin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc

SOURCES = \
    ../src/protocol.cpp \
    src/main.cpp

HEADERS = \
    ../src/protocol.h

#installs
target.path = $$PREFIX/sbin

INSTALLS += \
    target

INCLUDEPATH += ../src
