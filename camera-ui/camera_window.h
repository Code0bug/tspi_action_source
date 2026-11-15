#ifndef CAMERA_WINDOW_H
#define CAMERA_WINDOW_H

#include "media_client.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

class CameraWindow : public QWidget
{
    Q_OBJECT

public:
    explicit CameraWindow(QWidget *parent = nullptr);

private slots:
    void showPreview();
    void showAlbum();
    void onThumbClicked(QListWidgetItem *item);
    void showNextPhoto();
    void showPrevPhoto();
    void backToAlbum();

private:
    void setupPreviewPage();
    void setupAlbumPage();
    void setupViewerPage();
    void loadAlbum();
    void showPhotoAt(int index);
    void layoutOverlayButtons();
    void resizeEvent(QResizeEvent *event) override;

    QStackedWidget *stack_ = nullptr;
    QWidget *previewPage_ = nullptr;
    QWidget *albumPage_ = nullptr;
    QWidget *viewerPage_ = nullptr;

    QLabel *timeLabel_ = nullptr;
    QPushButton *albumBtn_ = nullptr;
    QPushButton *videoBtn_ = nullptr;

    QListWidget *pictureList_ = nullptr;
    QLabel *emptyLabel_ = nullptr;
    QPushButton *albumBackBtn_ = nullptr;

    QLabel *pictureLabel_ = nullptr;
    QPushButton *viewerBackBtn_ = nullptr;
    QPushButton *prevBtn_ = nullptr;
    QPushButton *nextBtn_ = nullptr;
    int photoIndex_ = 0;
    bool albumDragged_ = false;
    MediaClient media_;
};

#endif
