TEMPLATE = lib
PROJECT = protocol
TARGET = $$PROJECT

CONFIG += console
CONFIG -= app_bundle

QT += core \
      xml
QT -= gui

CONFIG += warn_on

QMAKE_CXXFLAGS += -Wall -Werror -Wextra -pedantic-errors
QMAKE_CXXFLAGS += -std=c++14

DESTDIR = $$PWD/build/bin
OBJECTS_DIR = $$PWD/build/obj

SOURCES += \
    src/protocol.cpp

PUB_HEADERS += \
    src/protocol.h

HEADERS += \
    $$PUB_HEADERS

# installs
target.path = $$PREFIX/lib

includes.files = $$PUB_HEADERS
includes.path = $$PREFIX/include

INSTALLS += \
    target \
    includes
