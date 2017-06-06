#-------------------------------------------------
#
# Project created by QtCreator 2017-03-31T11:31:55
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ../../jiratools/bin/JiraTools
TEMPLATE = app


SOURCES += main.cpp\
        coadialog.cpp \
    ccurl.cpp

HEADERS  += coadialog.h \
    ccurl.h

FORMS    += coadialog.ui

INCLUDEPATH += ./include

LIBS += -L../jiratools/lib -lcurl

RESOURCES += \
    icon.qrc

RC_FILE = icon/logo.rc
