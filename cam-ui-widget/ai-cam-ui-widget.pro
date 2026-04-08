QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    audiocapture.cpp \
    cameracapture.cpp \
    imureader.cpp \
    keycontrol.cpp \
    main.cpp \
    mp4muxdemux.cpp \
    rk_mpi.cpp \
    savepicture.cpp \
    savevideo.cpp \
    videodecget.cpp \
    videodecsend.cpp \
    widget.cpp

HEADERS += \
    audiocapture.h \
    cameracapture.h \
    imureader.h \
    include/rk_mpi.h \
    keycontrol.h \
    mp4muxdemux.h \
    safeQueue.h \
    savepicture.h \
    savevideo.h \
    videodecget.h \
    videodecsend.h \
    widget.h

# 添加rockit/opencv/rknn头文件和动态库 $$PWD表示.pro文件所在目录
INCLUDEPATH += \
    $$PWD/include \
    $$PWD/include/mpi/sdk \
    $$PWD/include/opencv4 \
    $$PWD/include/rga \
    /home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host/aarch64-buildroot-linux-gnu/sysroot/usr/include
#    $$PWD/include/rkaiq/uAPI \
#    $$PWD/include/rkaiq/uAPI2 \
#    $$PWD/include/rkaiq/xcore \
#    $$PWD/include/rkaiq/common \
#    $$PWD/include/rkaiq/algos \
#    $$PWD/include/rkaiq/iq_parser \
#    $$PWD/include/rkaiq/iq_parser_v2


LIBS += \
        -lpthread \
        -L$$PWD/lib \
        -lrockit \
        -L/home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host/aarch64-buildroot-linux-gnu/sysroot/usr/lib \
        -lopencv_core \
        -lopencv_highgui \
        -lopencv_imgproc \
        -lopencv_imgcodecs \
        -lopencv_video \
        -lrga \
        -lavformat \
        -lavcodec \
        -lavutil \
        -lswresample \
        -lswscale
#        -lrkaiq

FORMS += \
    widget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=
