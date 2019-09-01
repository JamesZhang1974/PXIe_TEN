#-------------------------------------------------
#
# Project created by QtCreator 2014-11-28T03:04:36
#
#-------------------------------------------------

include( C:/qwt-6.1.4/qwt.prf )

DEFINES += QT_DLL QWT_DLL

LIBS += -L"C:\qwt-6.1.4\lib" -lqwt

LIBS += -L"C:\qwt-6.1.4\lib" -lqwtd

INCLUDEPATH += C:\Qt\5.12.4\mingw73_64\include\Qwt


QT       += core gui serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QMAKE_CXXFLAGS += -std=c++11

TEMPLATE = app

CONFIG += qt qwt

#static {
#    CONFIG += static
#    DEFINES += STATIC
#}


SOURCES += main.cpp\
    PCA9557A.cpp \
    PCA9557B.cpp \
           mainwindow.cpp \
           globals.cpp \
           BertWorker.cpp \
           Serial.cpp \
           I2CComms.cpp \
           GT1724.cpp \
           BertComponent.cpp \
           LMX2594.cpp \
           EyeMonitor.cpp \
           BertFile.cpp \
           BertChannel.cpp \
    tlc59108.cpp \
           widgets/BertUIButton.cpp \
           widgets/BertUICheckBox.cpp \
           widgets/BertUIConsts.cpp \
           widgets/BertUIGroup.cpp \
           widgets/BertUIImage.cpp \
           widgets/BertUILabel.cpp \
           widgets/BertUILamp.cpp \
           widgets/BertUIList.cpp \
           widgets/BertUIPane.cpp \
           widgets/BertUIStatusMessage.cpp \
           widgets/BertUITabs.cpp \
           widgets/BertUITextArea.cpp \
           widgets/BertUITextInfo.cpp \
           widgets/BertUIPGChannel.cpp \
           widgets/BertUIEDChannel.cpp \
           widgets/BertUIPlotBathtub.cpp \
           widgets/BertUIPlotBER.cpp \
           widgets/BertUIPlotEye.cpp \
           widgets/BertUIEyescanChannel.cpp \
           widgets/BertUIBathtubChannel.cpp \
    M24M02.cpp \
    widgets/BertUITextInput.cpp \
    LMXFrequencyProfile.cpp \
    SI5340.cpp \
    widgets/BertUIBGWidget.cpp \
    BertModel.cpp \
    BertBranding.cpp \
    widgets/BertUICDRChannel.cpp

HEADERS += mainwindow.h \
    PCA9557A.h \
    PCA9557B.h \
           globals.h \
           BertWorker.h \
           Serial.h \
           I2CComms.h \
           GT1724.h \
           BertComponent.h \
           LMX2594.h \
           EyeMonitor.h \
           BertFile.h \
           BertChannel.h \
    tlc59108.h \
           widgets/BertUIButton.h \
           widgets/BertUICheckBox.h \
           widgets/BertUIConsts.h \
           widgets/BertUIGroup.h \
           widgets/BertUIImage.h \
           widgets/BertUILabel.h \
           widgets/BertUILamp.h \
           widgets/BertUIList.h \
           widgets/BertUIPane.h \
           widgets/BertUIStatusMessage.h \
           widgets/BertUITabs.h \
           widgets/BertUITextArea.h \
           widgets/BertUITextInfo.h \
           widgets/BertUIPGChannel.h \
           widgets/BertUIEDChannel.h \
           widgets/BertUIPlotBathtub.h \
           widgets/BertUIPlotBER.h \
           widgets/BertUIPlotEye.h \
           widgets/BertUIEyescanChannel.h \
           widgets/BertUIBathtubChannel.h \
    M24M02.h \
    widgets/BertUITextInput.h \
    LMXFrequencyProfile.h \
    SI5340.h \
    widgets/BertUIBGWidget.h \
    BertModel.h \
    BertBranding.h \
    widgets/BertUICDRChannel.h

FORMS   += \
    dialog.ui

RC_FILE = resources\PG3204.rc

RESOURCES = resources\PG3204.qrc

# For Release:
#DEFINES  += QT_NO_DEBUG_OUTPUT QT_NO_WARNING_OUTPUT

