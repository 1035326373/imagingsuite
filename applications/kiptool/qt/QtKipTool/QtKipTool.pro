#-------------------------------------------------
#
# Project created by QtCreator 2013-10-30T07:42:56
#
#-------------------------------------------------

QT       += core widgets printsupport concurrent

TARGET = QtKipTool
TEMPLATE = app
CONFIG += c++11

CONFIG(release, debug|release): DESTDIR = $$PWD/../../../../../Applications
else:CONFIG(debug, debug|release): DESTDIR = $$PWD/../../../../../Applications/debug

unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }
    INSTALLS += target

    unix:macx {
        QMAKE_MAC_SDK = macosx10.12
        QMAKE_CXXFLAGS += -fPIC -O2
        INCLUDEPATH += /opt/local/include
        QMAKE_LIBDIR += /opt/local/lib
     #   QMAKE_INFO_PLIST = Info.plist

    }
    else {
        QMAKE_CXXFLAGS += -fPIC -fopenmp -O2
        QMAKE_LFLAGS += -lgomp
        LIBS += -lgomp
    }

    LIBS += -ltiff -lxml2 -lcfitsio
    INCLUDEPATH += /usr/include/libxml2
}

win32 {
    contains(QMAKE_HOST.arch, x86_64):{
        QMAKE_LFLAGS += /MACHINE:X64
    }
    INCLUDEPATH += $$PWD/../../../../../external/src/linalg $$PWD/../../../../../external/include $$PWD/../../../../../external/include/cfitsio $$PWD/../../../../../external/include/libxml2
    QMAKE_LIBDIR += $$_PRO_FILE_PWD_/../../../../../external/lib64

    LIBS += -llibxml2_dll -llibtiff -lcfitsio
    QMAKE_CXXFLAGS += /openmp /O2
}

ICON = kip_icon.icns

SOURCES +=  ../../src/main.cpp\
            ../../src/kiptoolmainwindow.cpp \
            ../../src/ImageIO.cpp \
            ../../src/confighistorydialog.cpp \
            ../../src/genericconversion.cpp \
    ../../src/Reslicer.cpp \
    ../../src/reslicerdialog.cpp \
    ../../src/mergevolume.cpp \
    ../../src/mergevolumesdialog.cpp \
    ../../src/ImagingToolConfig.cpp \
    ../../src/fileconversiondialog.cpp \
    ../../src/Fits2Tif.cpp

HEADERS  += ../../src/kiptoolmainwindow.h \
            ../../src/ImageIO.h \
            ../../src/confighistorydialog.h \
            ../../src/genericconversion.h \
    ../../src/Reslicer.h \
    ../../src/reslicerdialog.h \
    ../../src/mergevolume.h \
    ../../src/mergevolumesdialog.h \
    ../../src/ImagingToolConfig.h \
    ../../src/fileconversiondialog.h \
    ../../src/Fits2Tif.h

FORMS    += ../../src/kiptoolmainwindow.ui \
            ../../src/confighistorydialog.ui \
            ../../src/genericconversion.ui \
    ../../src/reslicerdialog.ui \
    ../../src/mergevolumesdialog.ui \
    ../../src/fileconversiondialog.ui

CONFIG(release, debug|release): LIBS += -L$$PWD/../../../../../lib/
else:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../../../../lib/debug/

LIBS += -lkipl -lModuleConfig -lProcessFramework -lQtAddons -lQtModuleConfigure -lReaderConfig -lReaderGUI -lImagingAlgorithms

INCLUDEPATH += $$PWD/../../../../frameworks/imageprocessing/ProcessFramework/include
DEPENDPATH += $$PWD/../../../../frameworks/imageprocessing/ProcessFramework/include/

INCLUDEPATH += $$PWD/../../../../GUI/qt/QtAddons
DEPENDPATH += $$PWD/../../../../GUI/qt/QtAddons

INCLUDEPATH += $$PWD/../../../../GUI/qt/QtModuleConfigure

INCLUDEPATH += $$PWD/../../../../core/modules/ModuleConfig/include
DEPENDPATH += $$PWD/../../../../core/modules/ModuleConfig/include

INCLUDEPATH += $$PWD/../../../../core/modules/ReaderConfig
DEPENDPATH += $$PWD/../../../../core/modules/ReaderConfig

INCLUDEPATH += $$PWD/../../../../core/modules/ReaderGUI
DEPENDPATH += $$PWD/../../../../core/modules/ReaderGUI

INCLUDEPATH += $$PWD/../../../../core/kipl/kipl/include
DEPENDPATH += $$PWD/../../../../core/kipl/kipl/include

INCLUDEPATH += $$PWD/../../../../core/algorithms/ImagingAlgorithms/include
DEPENDPATH += $$PWD/../../../../core/algorithms/ImagingAlgorithms/src