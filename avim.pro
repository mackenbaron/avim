
QT       += core gui widgets multimedia
CXXFLAGS += "c++11"

TEMPLATE = subdirs

CONFIG += mobility no_keywords
MOBILITY =

SUBDIRS = src

src.file = src/avim.pro
