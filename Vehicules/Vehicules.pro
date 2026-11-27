QT       += core gui widgets sql network

DEFINES += STANDALONE_VEHICULES

TARGET = Vehicules
TEMPLATE = app

SOURCES += main.cpp gsmartfirestation.cpp maintenancemail.cpp
HEADERS += gsmartfirestation.h maintenancemail.h
FORMS   += gsmartfirestation.ui
RESOURCES += resources.qrc
