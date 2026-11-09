QT       += core gui widgets sql

DEFINES += STANDALONE_VEHICULES

TARGET = Vehicules
TEMPLATE = app

SOURCES += main.cpp gsmartfirestation.cpp
HEADERS += gsmartfirestation.h
FORMS   += gsmartfirestation.ui
RESOURCES += resources.qrc
