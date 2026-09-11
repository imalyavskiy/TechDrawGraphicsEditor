QT += core gui widgets gui-private
CONFIG += c++17
TEMPLATE = app
TARGET = Drawing
SOURCES += src/main.cpp src/canvas.cpp src/project.cpp src/mainwindow.cpp src/selftest.cpp
HEADERS += src/canvas.h src/project.h src/mainwindow.h src/selftest.h
win32:RC_FILE = resources/drawing.rc
