#ifndef CAMERA_WINDOW_H
#define CAMERA_WINDOW_H

#include "media_client.h"

#include <QDateTime>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QWidget>

class CameraWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CameraWindow(QWidget *parent = nullptr);

private slots:
    void showPreview();
    void showAlbum();
    void showVideoAlbum();
    void onThumbClicked(QListWidgetItem *item);
    void showNextPhoto();
    void showPrevPhoto();
    void backToAlbum();
    void onRecordStarted();
    void onRecordStopped();

private:
    void setupPreviewPage();
    void setupAlbumPage();
    void setupVideoAlbumPage();
    void setupViewerPage();
    void setupRecordPage();
    void loadAlbum();
    void loadVideoAlbum();
    void showPhotoAt(int index);
    void updateRecTime();
    void layoutOverlayButtons();
    void resizeEvent(QResizeEvent *event) override;

    QStackedWidget *stack_ = nullptr;
    QWidget *previewPage_ = nullptr;
    QWidget *albumPage_ = nullptr;
    QWidget *videoAlbumPage_ = nullptr;
    QWidget *viewerPage_ = nullptr;
    QWidget *recordPage_ = nullptr;

    QLabel *timeLabel_ = nullptr;
    QPushButton *albumBtn_ = nullptr;
    QPushButton *videoBtn_ = nullptr;

    QListWidget *pictureList_ = nullptr;
    QLabel *emptyLabel_ = nullptr;
    QPushButton *albumBackBtn_ = nullptr;

    QListWidget *videoList_ = nullptr;
    QLabel *videoEmptyLabel_ = nullptr;
    QPushButton *videoBackBtn_ = nullptr;
    bool videoAlbumDragged_ = false;

    QLabel *pictureLabel_ = nullptr;
    QPushButton *viewerBackBtn_ = nullptr;
    QPushButton *prevBtn_ = nullptr;
    QPushButton *nextBtn_ = nullptr;
    int photoIndex_ = 0;
    bool albumDragged_ = false;
    MediaClient media_;

    QLabel *recTimeLabel_ = nullptr;
    QLabel *recFpsLabel_ = nullptr;
    QTimer *recTimer_ = nullptr;
    QDateTime recordStart_;
};

#endif
