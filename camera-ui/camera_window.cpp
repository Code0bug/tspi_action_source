#include "camera_window.h"

#include <cstdio>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFont>
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

static void setupOsdLabel(QLabel *lbl, int x, int y, int w, int h, Qt::Alignment align)
{
    QFont font = lbl->font();
    font.setPixelSize(25);
    font.setBold(true);
    lbl->setFont(font);
    lbl->setAlignment(align);
    lbl->setGeometry(x, y, w, h);
    lbl->setFixedSize(w, h);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: white; background: transparent; padding: 2px 4px; }"));
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
    setupViewerPage();
    stack_->setCurrentWidget(previewPage_);

    connect(&media_, &MediaClient::recordStarted, this, &CameraWindow::onRecordStarted);
    connect(&media_, &MediaClient::recordStopped, this, &CameraWindow::onRecordStopped);
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

    albumBtn_ = new QPushButton(previewPage_);
    setupButton(albumBtn_, QStringLiteral("/root/photo1.png"));
    albumBtn_->move(0, 100);
    connect(albumBtn_, &QPushButton::clicked, this, &CameraWindow::showAlbum);

    videoBtn_ = new QPushButton(previewPage_);
    setupButton(videoBtn_, QStringLiteral("/root/video1.png"));
    videoBtn_->move(0, 270);

    stack_->addWidget(previewPage_);
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
        if (state == QScroller::Dragging || state == QScroller::Scrolling)
            albumDragged_ = true;
        else if (state == QScroller::Inactive)
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

void CameraWindow::layoutOverlayButtons()
{
    if (emptyLabel_)
        emptyLabel_->setGeometry(albumPage_->rect());
    if (albumBackBtn_)
        albumBackBtn_->raise();

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

void CameraWindow::onRecordStarted()
{
    std::fprintf(stderr, "CameraWindow: record started\n");
}

void CameraWindow::onRecordStopped()
{
    std::fprintf(stderr, "CameraWindow: record stopped\n");
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

void CameraWindow::onThumbClicked(QListWidgetItem *item)
{
    if (albumDragged_)
        return;
    showPhotoAt(pictureList_->row(item));
    stack_->setCurrentWidget(viewerPage_);
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
