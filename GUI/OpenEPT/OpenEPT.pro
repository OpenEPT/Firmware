QT       += core gui uitools opengl concurrent
QT       += network
LIBS     += -lws2_32

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

CONFIG += c++17
#DEFINES += QCUSTOMPLOT_USE_OPENGL
# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
LIBS     += "C:\Users\Haris\Documents\OpenEPT\OpenEPT\GUI\OpenEPT\Processing\fftw\libfftw3-3.lib"

SOURCES += \
    Chart/qcustomplot.cpp \
    Links/controllink.cpp \
    Links/eplink.cpp \
    Links/statuslink.cpp \
    Links/streamlink.cpp \
    Processing/dataprocessing.cpp \
    Processing/epprocessing.cpp \
    Processing/fileprocessing.cpp \
    Utility/log.cpp \
    Windows/AddDevice/adddevicewnd.cpp \
    Windows/Console/consolewnd.cpp \
    Windows/DataAnalyzer/dataanalyzer.cpp \
    Windows/Device/advanceconfigurationwnd.cpp \
    Windows/Device/datastatistics.cpp \
    Windows/Device/devicewnd.cpp \
    Windows/Plot/plot.cpp \
    Windows/WSSelection/selectworkspace.cpp \
    device.cpp \
    devicecontainer.cpp \
    main.cpp \
    openept.cpp

HEADERS += \
    Chart/qcustomplot.h \
    Links/controllink.h \
    Links/eplink.h \
    Links/statuslink.h \
    Links/streamlink.h \
    Processing/dataprocessing.h \
    Processing/epprocessing.h \
    Processing/fftw/fftw3.h \
    Processing/fileprocessing.h \
    Utility/log.h \
    Windows/AddDevice/adddevicewnd.h \
    Windows/Console/consolewnd.h \
    Windows/DataAnalyzer/dataanalyzer.h \
    Windows/Device/advanceconfigurationwnd.h \
    Windows/Device/datastatistics.h \
    Windows/Device/devicewnd.h \
    Windows/Plot/plot.h \
    Windows/WSSelection/selectworkspace.h \
    device.h \
    devicecontainer.h \
    openept.h

FORMS += \
    Windows/AddDevice/adddevicewnd.ui \
    Windows/Console/consolewnd.ui \
    Windows/DataAnalyzer/dataanalyzer.ui \
    Windows/Device/advanceconfigurationwnd.ui \
    Windows/Device/datastatistics.ui \
    Windows/Device/devicewnd.ui \
    Windows/WSSelection/selectworkspace.ui \
    openept.ui

QTPLUGIN += qjpeg
# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource.qrc

