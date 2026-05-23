/********************************************************************************
** Form generated from reading UI file 'widget.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WIDGET_H
#define UI_WIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Widget
{
public:
    QStackedWidget *stackedWidget;
    QWidget *page0;
    QLabel *video_label;
    QPushButton *switch2photo_button;
    QPushButton *switch2video_button;
    QWidget *page1;
    QPushButton *photoback_button;
    QListWidget *picture_listWidget;
    QWidget *page2;
    QPushButton *videoback_button;
    QListWidget *video_listWidget;
    QWidget *page3;
    QLabel *picture_label;
    QPushButton *next_button;
    QPushButton *prev_button;
    QPushButton *photo1back_button;
    QWidget *page4;
    QPushButton *video1back_button;
    QLabel *video_label_2;
    QWidget *page5;
    QLabel *video_label_3;

    void setupUi(QWidget *Widget)
    {
        if (Widget->objectName().isEmpty())
            Widget->setObjectName(QString::fromUtf8("Widget"));
        Widget->resize(545, 360);
        stackedWidget = new QStackedWidget(Widget);
        stackedWidget->setObjectName(QString::fromUtf8("stackedWidget"));
        stackedWidget->setGeometry(QRect(0, 0, 552, 368));
        page0 = new QWidget();
        page0->setObjectName(QString::fromUtf8("page0"));
        video_label = new QLabel(page0);
        video_label->setObjectName(QString::fromUtf8("video_label"));
        video_label->setGeometry(QRect(0, 0, 552, 368));
        video_label->setScaledContents(false);
        switch2photo_button = new QPushButton(page0);
        switch2photo_button->setObjectName(QString::fromUtf8("switch2photo_button"));
        switch2photo_button->setGeometry(QRect(0, 100, 89, 25));
        switch2video_button = new QPushButton(page0);
        switch2video_button->setObjectName(QString::fromUtf8("switch2video_button"));
        switch2video_button->setGeometry(QRect(0, 270, 89, 25));
        stackedWidget->addWidget(page0);
        page1 = new QWidget();
        page1->setObjectName(QString::fromUtf8("page1"));
        photoback_button = new QPushButton(page1);
        photoback_button->setObjectName(QString::fromUtf8("photoback_button"));
        photoback_button->setGeometry(QRect(0, 20, 89, 25));
        picture_listWidget = new QListWidget(page1);
        picture_listWidget->setObjectName(QString::fromUtf8("picture_listWidget"));
        picture_listWidget->setGeometry(QRect(0, 0, 551, 361));
        stackedWidget->addWidget(page1);
        picture_listWidget->raise();
        photoback_button->raise();
        page2 = new QWidget();
        page2->setObjectName(QString::fromUtf8("page2"));
        videoback_button = new QPushButton(page2);
        videoback_button->setObjectName(QString::fromUtf8("videoback_button"));
        videoback_button->setGeometry(QRect(0, 30, 89, 25));
        video_listWidget = new QListWidget(page2);
        video_listWidget->setObjectName(QString::fromUtf8("video_listWidget"));
        video_listWidget->setGeometry(QRect(0, 0, 551, 361));
        stackedWidget->addWidget(page2);
        video_listWidget->raise();
        videoback_button->raise();
        page3 = new QWidget();
        page3->setObjectName(QString::fromUtf8("page3"));
        picture_label = new QLabel(page3);
        picture_label->setObjectName(QString::fromUtf8("picture_label"));
        picture_label->setGeometry(QRect(0, 0, 551, 371));
        picture_label->setScaledContents(true);
        next_button = new QPushButton(page3);
        next_button->setObjectName(QString::fromUtf8("next_button"));
        next_button->setGeometry(QRect(460, 150, 89, 25));
        prev_button = new QPushButton(page3);
        prev_button->setObjectName(QString::fromUtf8("prev_button"));
        prev_button->setGeometry(QRect(0, 150, 89, 25));
        photo1back_button = new QPushButton(page3);
        photo1back_button->setObjectName(QString::fromUtf8("photo1back_button"));
        photo1back_button->setGeometry(QRect(0, 10, 89, 25));
        stackedWidget->addWidget(page3);
        page4 = new QWidget();
        page4->setObjectName(QString::fromUtf8("page4"));
        video1back_button = new QPushButton(page4);
        video1back_button->setObjectName(QString::fromUtf8("video1back_button"));
        video1back_button->setGeometry(QRect(0, 30, 89, 25));
        video_label_2 = new QLabel(page4);
        video_label_2->setObjectName(QString::fromUtf8("video_label_2"));
        video_label_2->setGeometry(QRect(0, 0, 552, 368));
        video_label_2->setScaledContents(false);
        stackedWidget->addWidget(page4);
        video_label_2->raise();
        video1back_button->raise();
        page5 = new QWidget();
        page5->setObjectName(QString::fromUtf8("page5"));
        video_label_3 = new QLabel(page5);
        video_label_3->setObjectName(QString::fromUtf8("video_label_3"));
        video_label_3->setGeometry(QRect(0, -10, 552, 368));
        video_label_3->setScaledContents(false);
        stackedWidget->addWidget(page5);

        retranslateUi(Widget);

        QMetaObject::connectSlotsByName(Widget);
    } // setupUi

    void retranslateUi(QWidget *Widget)
    {
        Widget->setWindowTitle(QCoreApplication::translate("Widget", "Widget", nullptr));
        video_label->setText(QCoreApplication::translate("Widget", "TextLabel", nullptr));
        switch2photo_button->setText(QString());
        switch2video_button->setText(QString());
        photoback_button->setText(QString());
        videoback_button->setText(QString());
        picture_label->setText(QCoreApplication::translate("Widget", "TextLabel", nullptr));
        next_button->setText(QString());
        prev_button->setText(QString());
        photo1back_button->setText(QString());
        video1back_button->setText(QString());
        video_label_2->setText(QCoreApplication::translate("Widget", "TextLabel", nullptr));
        video_label_3->setText(QCoreApplication::translate("Widget", "TextLabel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Widget: public Ui_Widget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WIDGET_H
