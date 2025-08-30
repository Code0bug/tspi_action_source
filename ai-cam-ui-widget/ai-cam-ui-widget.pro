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
    cameracapture.cpp \
    encoderrtsp.cpp \
    imgprocess.cpp \
    main.cpp \
    rk_mpi.cpp \
    rknn/utils/file_utils.c \
    rknn/utils/image_drawing.c \
    rknn/utils/image_utils.c \
    rknn/yolov5/postprocess.cc \
    rknn/yolov5/yolov5.cc \
    widget.cpp

HEADERS += \
    cameracapture.h \
    encoderrtsp.h \
    imgprocess.h \
    widget.h

# 添加rockit/opencv/rknn头文件和动态库 $$PWD表示.pro文件所在目录
INCLUDEPATH += \
    $$PWD/include \
    $$PWD/include/mpi/example \
    $$PWD/include/mpi/sdk \
    $$PWD/include/rknn/rknpu2 \
    $$PWD/include/rknn/utils \
    $$PWD/include/rknn/yolov5 \
    $$PWD/include/rknn/librga \
    $$PWD/include/rknn/stb_image \
    $$PWD/include/rknn/jpeg_turbo \
    $$PWD/include/ZLMediaKit

LIBS += \
        -lpthread \
        -L$$PWD/lib \
        -lrockit \
        -lrt_test_comm \
        -lopencv_core \
        -lopencv_highgui \
        -lopencv_imgproc \
        -lopencv_imgcodecs \
        -lrknnrt \
        -lrga \
        -lturbojpeg \
        -lmk_api

FORMS += \
    widget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=
