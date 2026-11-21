#include "camera_window.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QImageReader>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollBar>
#include <QScroller>
#include <QTimer>
#include <QVBoxLayout>

static const QString kPictureDir = QStringLiteral("/userdata/picture");
static const QString kVideoDir = QStringLiteral("/userdata/video");
static const QString kFirstFrameDir = QStringLiteral("/userdata/firstFrame");

static void setupButton(QPushButton *btn, const QString &iconPath)
{
    btn->setIcon(QIcon(QPixmap(iconPath)));
    btn->setFixedSize(80, 80);
    btn->setIconSize(QSize(80, 80));
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { border: none; background: transparent; }"
        "QPushButton:pressed { background: transparent; }"
        "QPushButton:focus { outline: none; }"));
}

static void setupOsdLabel(QLabel *lbl, int x, int y, int w, int h,
                          Qt::Alignment align, bool recording = false)
{
    QFont font = lbl->font();
    font.setPixelSize(25);
    font.setBold(true);
    lbl->setFont(font);
    lbl->setAlignment(align);
    lbl->setGeometry(x, y, w, h);
    lbl->setFixedSize(w, h);
    if (recording) {
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: white; background-color: rgb(200, 0, 0); padding: 2px 4px; }"));
    } else {
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: white; background: transparent; padding: 2px 4px; }"));
    }
}

static void setupTouchScrollBar(QScrollBar *bar)
{
    bar->setStyleSheet(QStringLiteral(
        "QScrollBar:vertical {"
        "    width: 40px;"
        "    background: #f0f0f0;"
        "    margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #a0a0a0;"
        "    min-height: 60px;"
        "    border-radius: 8px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: #808080; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"));
}

static QPixmap makeRoundThumb(const QPixmap &src, int size)
{
    QPixmap dst(size, size);
    dst.fill(Qt::transparent);

    QPainter painter(&dst);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    path.addRoundedRect(0, 0, size, size, 20, 20);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, src.scaled(size, size,
                                        Qt::KeepAspectRatioByExpanding,
                                        Qt::SmoothTransformation));
    return dst;
}

static QPixmap loadThumb(const QString &path, int size)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QSize sz = reader.size();
    if (sz.isValid())
        sz.scale(size, size, Qt::KeepAspectRatioByExpanding);
    reader.setScaledSize(sz);
    return makeRoundThumb(QPixmap::fromImage(reader.read()), size);
}

CameraWindow::CameraWindow(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet(QStringLiteral("background: transparent;"));

    stack_ = new QStackedWidget(this);
    stack_->setStyleSheet(QStringLiteral("QStackedWidget { background: transparent; }"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(stack_);

    setupPreviewPage();
    setupAlbumPage();
    setupVideoAlbumPage();
    setupViewerPage();
    setupRecordPage();
    setupPlayerPage();
    setupConfirmMask();
    longPressTimer_ = new QTimer(this);
    longPressTimer_->setSingleShot(true);
    connect(longPressTimer_, &QTimer::timeout, this, &CameraWindow::onLongPress);
    stack_->setCurrentWidget(previewPage_);

    connect(&media_, &MediaClient::recordStarted, this, &CameraWindow::onRecordStarted);
    connect(&media_, &MediaClient::recordStopped, this, &CameraWindow::onRecordStopped);
    connect(&media_, &MediaClient::photoStarted, this, &CameraWindow::onPhotoStarted);
    connect(&media_, &MediaClient::photoSaved, this, &CameraWindow::onPhotoSaved);
    connect(&media_, &MediaClient::videoSaved, this, &CameraWindow::onVideoSaved);
}

void CameraWindow::setupPreviewPage()
{
    previewPage_ = new QWidget;
    previewPage_->setAttribute(Qt::WA_TranslucentBackground);
    previewPage_->setStyleSheet(QStringLiteral("background: transparent;"));

    timeLabel_ = new QLabel(previewPage_);
    setupOsdLabel(timeLabel_, 20, 20, 130, 30, Qt::AlignLeft | Qt::AlignVCenter);
    auto updateTime = [this]() {
        timeLabel_->setText(QDateTime::currentDateTime().toString(QStringLiteral("HH'h'mm'm'")));
    };
    updateTime();
    auto *timeTimer = new QTimer(this);
    connect(timeTimer, &QTimer::timeout, this, updateTime);
    timeTimer->start(1000);

    captureHint_ = new QLabel(QStringLiteral("拍摄中"), previewPage_);
    setupOsdLabel(captureHint_, 380, 20, 110, 30,
                  Qt::AlignRight | Qt::AlignVCenter, true);
    captureHint_->hide();

    albumBtn_ = new QPushButton(previewPage_);
    setupButton(albumBtn_, QStringLiteral("/root/photo1.png"));
    albumBtn_->move(0, 100);
    connect(albumBtn_, &QPushButton::clicked, this, &CameraWindow::showAlbum);

    videoBtn_ = new QPushButton(previewPage_);
    setupButton(videoBtn_, QStringLiteral("/root/video1.png"));
    videoBtn_->move(0, 270);
    connect(videoBtn_, &QPushButton::clicked, this, &CameraWindow::showVideoAlbum);

    stack_->addWidget(previewPage_);
}

void CameraWindow::setupRecordPage()
{
    recordPage_ = new QWidget;
    recordPage_->setAttribute(Qt::WA_TranslucentBackground);
    recordPage_->setStyleSheet(QStringLiteral("background: transparent;"));

    recTimeLabel_ = new QLabel(recordPage_);
    setupOsdLabel(recTimeLabel_, 20, 20, 90, 30, Qt::AlignLeft | Qt::AlignVCenter, true);

    recFpsLabel_ = new QLabel(recordPage_);
    setupOsdLabel(recFpsLabel_, 380, 20, 160, 30, Qt::AlignRight | Qt::AlignVCenter);

    recTimer_ = new QTimer(this);
    connect(recTimer_, &QTimer::timeout, this, &CameraWindow::updateRecTime);

    stack_->addWidget(recordPage_);
}

void CameraWindow::setupPlayerPage()
{
    // 透明页：primary plane 上只画返回按钮，视频由下层 overlay plane 显示
    playerPage_ = new QWidget;
    playerPage_->setAttribute(Qt::WA_TranslucentBackground);
    playerPage_->setStyleSheet(QStringLiteral("background: transparent;"));

    playerBackBtn_ = new QPushButton(playerPage_);
    setupButton(playerBackBtn_, QStringLiteral("/root/back.png"));
    playerBackBtn_->move(0, 20);
    connect(playerBackBtn_, &QPushButton::clicked, this, &CameraWindow::backToVideoAlbum);

    stack_->addWidget(playerPage_);
}

void CameraWindow::setupAlbumPage()
{
    albumPage_ = new QWidget;
    albumPage_->setStyleSheet(QStringLiteral("background: #f0f0f0;"));

    pictureList_ = new QListWidget(albumPage_);
    pictureList_->setViewMode(QListView::IconMode);
    pictureList_->setResizeMode(QListView::Adjust);
    pictureList_->setMovement(QListView::Static);
    pictureList_->setIconSize(QSize(150, 150));
    pictureList_->setGridSize(QSize(165, 165));
    pictureList_->setSpacing(4);
    pictureList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    pictureList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScroller::grabGesture(pictureList_->viewport(), QScroller::LeftMouseButtonGesture);
    QScroller::grabGesture(pictureList_->viewport(), QScroller::TouchGesture);
    QScroller *scroller = QScroller::scroller(pictureList_->viewport());
    connect(scroller, &QScroller::stateChanged, this, [this](QScroller::State state) {
        if (state == QScroller::Dragging || state == QScroller::Scrolling) {
            albumDragged_ = true;
            cancelLongPress();
        } else if (state == QScroller::Inactive)
            QTimer::singleShot(0, this, [this] { albumDragged_ = false; });
    });
    pictureList_->setStyleSheet(QStringLiteral(
        "QListWidget { background: #f0f0f0; border: none; outline: none; }"
        "QListWidget::item { background: transparent; border: none; }"
        "QListWidget::item:selected, QListWidget::item:pressed {"
        "    background: rgba(0, 0, 0, 50);"
        "    border-radius: 20px;"
        "}"));
    setupTouchScrollBar(pictureList_->verticalScrollBar());
    connect(pictureList_, &QListWidget::itemClicked, this, &CameraWindow::onThumbClicked);
    connect(pictureList_, &QListWidget::itemPressed, this, &CameraWindow::onPhotoPressed);

    auto *layout = new QVBoxLayout(albumPage_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(pictureList_);

    emptyLabel_ = new QLabel(QStringLiteral("暂无照片"), albumPage_);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setStyleSheet(QStringLiteral(
        "QLabel { color: #666666; background: transparent; }"));
    QFont emptyFont = emptyLabel_->font();
    emptyFont.setPixelSize(28);
    emptyLabel_->setFont(emptyFont);

    albumBackBtn_ = new QPushButton(albumPage_);
    setupButton(albumBackBtn_, QStringLiteral("/root/back.png"));
    albumBackBtn_->move(0, 20);
    connect(albumBackBtn_, &QPushButton::clicked, this, &CameraWindow::showPreview);

    loadAlbum();
    stack_->addWidget(albumPage_);
}

void CameraWindow::setupVideoAlbumPage()
{
    videoAlbumPage_ = new QWidget;
    videoAlbumPage_->setStyleSheet(QStringLiteral("background: #f0f0f0;"));

    videoList_ = new QListWidget(videoAlbumPage_);
    videoList_->setViewMode(QListView::IconMode);
    videoList_->setResizeMode(QListView::Adjust);
    videoList_->setMovement(QListView::Static);
    videoList_->setIconSize(QSize(150, 150));
    videoList_->setGridSize(QSize(165, 165));
    videoList_->setSpacing(4);
    videoList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    videoList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScroller::grabGesture(videoList_->viewport(), QScroller::LeftMouseButtonGesture);
    QScroller::grabGesture(videoList_->viewport(), QScroller::TouchGesture);
    QScroller *scroller = QScroller::scroller(videoList_->viewport());
    connect(scroller, &QScroller::stateChanged, this, [this](QScroller::State state) {
        if (state == QScroller::Dragging || state == QScroller::Scrolling) {
            videoAlbumDragged_ = true;
            cancelLongPress();
        } else if (state == QScroller::Inactive)
            QTimer::singleShot(0, this, [this] { videoAlbumDragged_ = false; });
    });
    videoList_->setStyleSheet(QStringLiteral(
        "QListWidget { background: #f0f0f0; border: none; outline: none; }"
        "QListWidget::item { background: transparent; border: none; }"
        "QListWidget::item:selected, QListWidget::item:pressed {"
        "    background: rgba(0, 0, 0, 50);"
        "    border-radius: 20px;"
        "}"));
    setupTouchScrollBar(videoList_->verticalScrollBar());
    connect(videoList_, &QListWidget::itemClicked, this, &CameraWindow::onVideoClicked);
    connect(videoList_, &QListWidget::itemPressed, this, &CameraWindow::onVideoPressed);

    auto *layout = new QVBoxLayout(videoAlbumPage_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(videoList_);

    videoEmptyLabel_ = new QLabel(QStringLiteral("暂无视频"), videoAlbumPage_);
    videoEmptyLabel_->setAlignment(Qt::AlignCenter);
    videoEmptyLabel_->setStyleSheet(QStringLiteral(
        "QLabel { color: #666666; background: transparent; }"));
    QFont emptyFont = videoEmptyLabel_->font();
    emptyFont.setPixelSize(28);
    videoEmptyLabel_->setFont(emptyFont);

    videoBackBtn_ = new QPushButton(videoAlbumPage_);
    setupButton(videoBackBtn_, QStringLiteral("/root/back.png"));
    videoBackBtn_->move(0, 20);
    connect(videoBackBtn_, &QPushButton::clicked, this, &CameraWindow::showPreview);

    loadVideoAlbum();
    stack_->addWidget(videoAlbumPage_);
}

void CameraWindow::setupViewerPage()
{
    viewerPage_ = new QWidget;
    viewerPage_->setStyleSheet(QStringLiteral("background: black;"));

    pictureLabel_ = new QLabel(viewerPage_);
    pictureLabel_->setAlignment(Qt::AlignCenter);
    pictureLabel_->setStyleSheet(QStringLiteral("background: black;"));
    auto *layout = new QVBoxLayout(viewerPage_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(pictureLabel_);

    viewerBackBtn_ = new QPushButton(viewerPage_);
    setupButton(viewerBackBtn_, QStringLiteral("/root/back.png"));
    connect(viewerBackBtn_, &QPushButton::clicked, this, &CameraWindow::backToAlbum);

    prevBtn_ = new QPushButton(viewerPage_);
    setupButton(prevBtn_, QStringLiteral("/root/chevron-left.png"));
    connect(prevBtn_, &QPushButton::clicked, this, &CameraWindow::showPrevPhoto);

    nextBtn_ = new QPushButton(viewerPage_);
    setupButton(nextBtn_, QStringLiteral("/root/chevron-right.png"));
    connect(nextBtn_, &QPushButton::clicked, this, &CameraWindow::showNextPhoto);

    stack_->addWidget(viewerPage_);
}

void CameraWindow::setupConfirmMask()
{
    confirmMask_ = new QWidget(this);
    confirmMask_->hide();
    confirmMask_->setStyleSheet(QStringLiteral("background: rgba(0, 0, 0, 180);"));

    auto *msg = new QLabel(QStringLiteral("确认删除?"), confirmMask_);
    msg->setAlignment(Qt::AlignCenter);
    QFont msgFont = msg->font();
    msgFont.setPixelSize(32);
    msgFont.setBold(true);
    msg->setFont(msgFont);
    msg->setStyleSheet(QStringLiteral("QLabel { color: white; background: transparent; }"));

    auto *cancelBtn = new QPushButton(QStringLiteral("取消"), confirmMask_);
    auto *okBtn = new QPushButton(QStringLiteral("删除"), confirmMask_);
    cancelBtn->setFixedSize(160, 72);
    okBtn->setFixedSize(160, 72);
    QFont btnFont = cancelBtn->font();
    btnFont.setPixelSize(28);
    btnFont.setBold(true);
    cancelBtn->setFont(btnFont);
    okBtn->setFont(btnFont);
    cancelBtn->setStyleSheet(QStringLiteral(
        "QPushButton { color: white; background: #555555; border: none; border-radius: 12px; }"
        "QPushButton:pressed { background: #777777; }"
        "QPushButton:focus { outline: none; }"));
    okBtn->setStyleSheet(QStringLiteral(
        "QPushButton { color: white; background: #c80000; border: none; border-radius: 12px; }"
        "QPushButton:pressed { background: #ff3333; }"
        "QPushButton:focus { outline: none; }"));
    connect(cancelBtn, &QPushButton::clicked, this, &CameraWindow::cancelDelete);
    connect(okBtn, &QPushButton::clicked, this, &CameraWindow::confirmDelete);

    auto *btns = new QHBoxLayout;
    btns->setSpacing(24);
    btns->addStretch();
    btns->addWidget(cancelBtn);
    btns->addWidget(okBtn);
    btns->addStretch();

    auto *layout = new QVBoxLayout(confirmMask_);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->addStretch();
    layout->addWidget(msg);
    layout->addSpacing(32);
    layout->addLayout(btns);
    layout->addStretch();
}

void CameraWindow::layoutOverlayButtons()
{
    if (emptyLabel_ && albumPage_)
        emptyLabel_->setGeometry(albumPage_->rect());
    if (albumBackBtn_)
        albumBackBtn_->raise();
    if (videoEmptyLabel_ && videoAlbumPage_)
        videoEmptyLabel_->setGeometry(videoAlbumPage_->rect());
    if (videoBackBtn_)
        videoBackBtn_->raise();
    if (playerBackBtn_)
        playerBackBtn_->raise();
    if (confirmMask_) {
        confirmMask_->setGeometry(rect());
        confirmMask_->raise();
    }
    if (captureHint_ && previewPage_)
        captureHint_->move(previewPage_->width() - captureHint_->width() - 20, 20);
    if (recFpsLabel_ && recordPage_)
        recFpsLabel_->move(recordPage_->width() - recFpsLabel_->width() - 20, 20);

    if (!viewerPage_)
        return;
    viewerBackBtn_->move(0, 10);
    prevBtn_->move(0, (viewerPage_->height() - prevBtn_->height()) / 2);
    nextBtn_->move(viewerPage_->width() - nextBtn_->width(),
                   (viewerPage_->height() - nextBtn_->height()) / 2);
    viewerBackBtn_->raise();
    prevBtn_->raise();
    nextBtn_->raise();
}

void CameraWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutOverlayButtons();
    if (stack_->currentWidget() == viewerPage_)
        showPhotoAt(photoIndex_);
}

void CameraWindow::loadAlbum()
{
    QDir dir(kPictureDir);
    const QStringList filters = {
        QStringLiteral("*.png"), QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg"), QStringLiteral("*.bmp")};
    const QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

    for (const QFileInfo &file : files) {
        auto *item = new QListWidgetItem;
        item->setIcon(QIcon(loadThumb(file.absoluteFilePath(), 160)));
        item->setData(Qt::UserRole, file.absoluteFilePath());
        item->setSizeHint(QSize(165, 165));
        pictureList_->addItem(item);
    }

    emptyLabel_->setVisible(pictureList_->count() == 0);
}

void CameraWindow::loadVideoAlbum()
{
    videoList_->clear();

    QDir dir(kVideoDir);
    const QFileInfoList files = dir.entryInfoList(
        {QStringLiteral("*.mp4")}, QDir::Files);

    for (const QFileInfo &file : files) {
        const QString thumbPath = kFirstFrameDir + QLatin1Char('/')
            + file.completeBaseName() + QStringLiteral(".jpg");
        auto *item = new QListWidgetItem;
        item->setIcon(QIcon(loadThumb(thumbPath, 160)));
        item->setData(Qt::UserRole, file.absoluteFilePath());
        item->setSizeHint(QSize(165, 165));
        videoList_->addItem(item);
    }

    videoEmptyLabel_->setVisible(videoList_->count() == 0);
}

void CameraWindow::updateRecTime()
{
    const qint64 ms = recordStart_.msecsTo(QDateTime::currentDateTime());
    const int seconds = static_cast<int>((ms / 1000) % 60);
    const int minutes = static_cast<int>((ms / 60000) % 60);
    recTimeLabel_->setText(QStringLiteral("%1:%2")
                               .arg(minutes, 2, 10, QChar('0'))
                               .arg(seconds, 2, 10, QChar('0')));
}

void CameraWindow::onRecordStarted()
{
    recordStart_ = QDateTime::currentDateTime();
    updateRecTime();
    recTimer_->start(200);
    stack_->setCurrentWidget(recordPage_);
    layoutOverlayButtons();
}

void CameraWindow::onRecordStopped()
{
    recTimer_->stop();
    stack_->setCurrentWidget(previewPage_);
}

void CameraWindow::onPhotoStarted()
{
    if (captureHint_) {
        captureHint_->show();
        captureHint_->raise();
    }
}

void CameraWindow::onPhotoSaved(const QString &path)
{
    if (captureHint_)
        captureHint_->hide();
    if (path.isEmpty())
        return;
    auto *item = new QListWidgetItem;
    item->setIcon(QIcon(loadThumb(path, 160)));
    item->setData(Qt::UserRole, path);
    item->setSizeHint(QSize(165, 165));
    pictureList_->addItem(item);
    emptyLabel_->setVisible(false);
}

void CameraWindow::onVideoSaved(const QString &path)
{
    QFileInfo fi(path);
    const QString mp4 = kVideoDir + QLatin1Char('/')
        + fi.completeBaseName() + QStringLiteral(".mp4");
    auto *item = new QListWidgetItem;
    item->setIcon(QIcon(loadThumb(path, 160)));
    item->setData(Qt::UserRole, mp4);
    item->setSizeHint(QSize(165, 165));
    videoList_->addItem(item);
    videoEmptyLabel_->setVisible(false);
}

void CameraWindow::showPreview()
{
    stack_->setCurrentWidget(previewPage_);
    media_.startCapture();
}

void CameraWindow::showAlbum()
{
    media_.stopCapture();
    stack_->setCurrentWidget(albumPage_);
    layoutOverlayButtons();
}

void CameraWindow::showVideoAlbum()
{
    media_.stopCapture();
    stack_->setCurrentWidget(videoAlbumPage_);
    layoutOverlayButtons();
}

void CameraWindow::onThumbClicked(QListWidgetItem *item)
{
    cancelLongPress();
    if (albumDragged_ || longPressTriggered_) {
        longPressTriggered_ = false;
        return;
    }
    showPhotoAt(pictureList_->row(item));
    stack_->setCurrentWidget(viewerPage_);
    layoutOverlayButtons();
}

void CameraWindow::onVideoClicked(QListWidgetItem *item)
{
    cancelLongPress();
    if (videoAlbumDragged_ || longPressTriggered_) {
        longPressTriggered_ = false;
        return;
    }
    playingPath_ = item->data(Qt::UserRole).toString();
    media_.playVideo(playingPath_);
    stack_->setCurrentWidget(playerPage_);
    layoutOverlayButtons();
}

void CameraWindow::onPhotoPressed(QListWidgetItem *item)
{
    startLongPress(item, false);
}

void CameraWindow::onVideoPressed(QListWidgetItem *item)
{
    startLongPress(item, true);
}

void CameraWindow::startLongPress(QListWidgetItem *item, bool video)
{
    longPressTriggered_ = false;
    longPressItem_ = item;
    longPressIsVideo_ = video;
    longPressTimer_->start(800);
}

void CameraWindow::cancelLongPress()
{
    if (longPressTimer_)
        longPressTimer_->stop();
    longPressItem_ = nullptr;
}

void CameraWindow::onLongPress()
{
    if (!longPressItem_)
        return;
    longPressTriggered_ = true;
    pendingPath_ = longPressItem_->data(Qt::UserRole).toString();
    pendingDelete_ = longPressIsVideo_ ? PendingDelete::Video : PendingDelete::Photo;
    longPressItem_ = nullptr;
    confirmMask_->show();
    confirmMask_->raise();
}

void CameraWindow::backToVideoAlbum()
{
    media_.stopVideo();
    playingPath_.clear();
    stack_->setCurrentWidget(videoAlbumPage_);
    layoutOverlayButtons();
}

void CameraWindow::cancelDelete()
{
    pendingDelete_ = PendingDelete::None;
    pendingPath_.clear();
    confirmMask_->hide();
}

void CameraWindow::confirmDelete()
{
    confirmMask_->hide();
    const PendingDelete kind = pendingDelete_;
    pendingDelete_ = PendingDelete::None;
    if (kind == PendingDelete::Photo)
        deleteCurrentPhoto();
    else if (kind == PendingDelete::Video)
        deleteCurrentVideo();
}

void CameraWindow::deleteCurrentPhoto()
{
    const QString path = pendingPath_;
    pendingPath_.clear();
    if (path.isEmpty())
        return;

    QFile::remove(path);
    for (int i = 0; i < pictureList_->count(); ++i) {
        if (pictureList_->item(i)->data(Qt::UserRole).toString() == path) {
            if (photoIndex_ >= i && photoIndex_ > 0)
                --photoIndex_;
            delete pictureList_->takeItem(i);
            break;
        }
    }
    emptyLabel_->setVisible(pictureList_->count() == 0);
}

void CameraWindow::deleteCurrentVideo()
{
    const QString path = pendingPath_;
    pendingPath_.clear();
    if (path.isEmpty())
        return;

    if (playingPath_ == path) {
        media_.stopVideo();
        playingPath_.clear();
        if (stack_->currentWidget() == playerPage_)
            stack_->setCurrentWidget(videoAlbumPage_);
    }

    const QFileInfo fi(path);
    QFile::remove(path);
    QFile::remove(kFirstFrameDir + QLatin1Char('/')
                  + fi.completeBaseName() + QStringLiteral(".jpg"));

    for (int i = 0; i < videoList_->count(); ++i) {
        if (videoList_->item(i)->data(Qt::UserRole).toString() == path) {
            delete videoList_->takeItem(i);
            break;
        }
    }
    videoEmptyLabel_->setVisible(videoList_->count() == 0);
    layoutOverlayButtons();
}

void CameraWindow::showPhotoAt(int index)
{
    if (pictureList_->count() == 0)
        return;
    if (index < 0)
        index = pictureList_->count() - 1;
    if (index >= pictureList_->count())
        index = 0;
    photoIndex_ = index;

    const QString path = pictureList_->item(index)->data(Qt::UserRole).toString();
    QPixmap pix(path);
    if (pix.isNull() || pictureLabel_->size().isEmpty()) {
        pictureLabel_->setPixmap(QPixmap());
        return;
    }
    pictureLabel_->setPixmap(pix.scaled(pictureLabel_->size(),
                                        Qt::IgnoreAspectRatio,
                                        Qt::SmoothTransformation));
}

void CameraWindow::showNextPhoto()
{
    showPhotoAt(photoIndex_ + 1);
}

void CameraWindow::showPrevPhoto()
{
    showPhotoAt(photoIndex_ - 1);
}

void CameraWindow::backToAlbum()
{
    stack_->setCurrentWidget(albumPage_);
    layoutOverlayButtons();
}
