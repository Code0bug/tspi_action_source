#ifndef CAMERA_WINDOW_H
#define CAMERA_WINDOW_H

#include "media_client.h"

#include <QDateTime>
#include <QLabel>
#include <QString>
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
    void onVideoClicked(QListWidgetItem *item);
    void onPhotoPressed(QListWidgetItem *item);
    void onVideoPressed(QListWidgetItem *item);
    void onLongPress();
    void showNextPhoto();
    void showPrevPhoto();
    void backToAlbum();
    void backToVideoAlbum();
    void confirmDelete();
    void cancelDelete();
    void onRecordStarted();
    void onRecordStopped();
    void onPhotoStarted();
    void onPhotoSaved(const QString &path);
    void onVideoSaved(const QString &path);

private:
    void setupPreviewPage();
    void setupAlbumPage();
    void setupVideoAlbumPage();
    void setupViewerPage();
    void setupRecordPage();
    void setupPlayerPage();
    void setupConfirmMask();
    void loadAlbum();
    void loadVideoAlbum();
    void showPhotoAt(int index);
    void updateRecTime();
    void layoutOverlayButtons();
    void startLongPress(QListWidgetItem *item, bool video);
    void cancelLongPress();
    void deleteCurrentPhoto();
    void deleteCurrentVideo();
    void resizeEvent(QResizeEvent *event) override;

    QStackedWidget *stack_ = nullptr;
    QWidget *previewPage_ = nullptr;
    QWidget *albumPage_ = nullptr;
    QWidget *videoAlbumPage_ = nullptr;
    QWidget *viewerPage_ = nullptr;
    QWidget *recordPage_ = nullptr;
    QWidget *playerPage_ = nullptr;
    QPushButton *playerBackBtn_ = nullptr;

    QLabel *timeLabel_ = nullptr;
    QLabel *captureHint_ = nullptr;
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
    QWidget *confirmMask_ = nullptr;
    enum class PendingDelete { None, Photo, Video };
    PendingDelete pendingDelete_ = PendingDelete::None;
    QString pendingPath_;
    QString playingPath_;
    QTimer *longPressTimer_ = nullptr;
    QListWidgetItem *longPressItem_ = nullptr;
    bool longPressIsVideo_ = false;
    bool longPressTriggered_ = false;
    int photoIndex_ = 0;
    bool albumDragged_ = false;
    MediaClient media_;

    QLabel *recTimeLabel_ = nullptr;
    QLabel *recFpsLabel_ = nullptr;
    QTimer *recTimer_ = nullptr;
    QDateTime recordStart_;
};

#endif
