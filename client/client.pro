TEMPLATE = app
PROJECT = client
TARGET = $$PROJECT

CONFIG += console
CONFIG -= app_bundle

QT += core \
      network 
QT += gui \
      widgets

CONFIG += warn_on

QMAKE_CXXFLAGS += -Wall -Werror -Wextra -pedantic-errors
QMAKE_CXXFLAGS += -std=c++14

DESTDIR = $$PWD/build/bin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
UI_DIR = $$PWD/build/ui

SOURCES += \
    src/client.cpp \
    src/main.cpp

HEADERS += \
    src/client.h

FORMS += \
    src/clientwidget.ui

# installs
target.path = $$PREFIX/bin

INSTALLS += \
    target

INCLUDEPATH += $$PREFIX/include

LIBS += -L$$PREFIX/lib -lprotocol
