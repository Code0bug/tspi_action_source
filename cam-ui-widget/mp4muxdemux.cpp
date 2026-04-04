#include "mp4muxdemux.h"

#include <QList>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

namespace {
static QString ffErr(int errnum)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, errbuf, sizeof(errbuf));
    return QString::fromLatin1(errbuf);
}

static bool findStartCode(const uint8_t *buf, int size, int start, int *scPos, int *scLen)
{
    for (int i = start; i + 3 < size; ++i) {
        if (buf[i] == 0x00 && buf[i + 1] == 0x00) {
            if (buf[i + 2] == 0x01) {
                *scPos = i;
                *scLen = 3;
                return true;
            }
            if (i + 4 < size && buf[i + 2] == 0x00 && buf[i + 3] == 0x01) {
                *scPos = i;
                *scLen = 4;
                return true;
            }
        }
    }
    return false;
}

static QByteArray buildAvccFromSpsPps(const QByteArray &sps, const QByteArray &pps)
{
    if (sps.size() < 4 || pps.isEmpty()) {
        return QByteArray();
    }

    QByteArray avcc;
    avcc.reserve(11 + sps.size() + pps.size());

    avcc.append(char(0x01));
    avcc.append(sps[1]);
    avcc.append(sps[2]);
    avcc.append(sps[3]);
    avcc.append(char(0xFF)); // 111111 + lengthSizeMinusOne(3 => 4 bytes)

    avcc.append(char(0xE1)); // 111 + numOfSPS(1)
    avcc.append(char((sps.size() >> 8) & 0xFF));
    avcc.append(char(sps.size() & 0xFF));
    avcc.append(sps);

    avcc.append(char(0x01)); // numOfPPS
    avcc.append(char((pps.size() >> 8) & 0xFF));
    avcc.append(char(pps.size() & 0xFF));
    avcc.append(pps);

    return avcc;
}

static bool hasAnnexBStartCode(const uint8_t *data, int size)
{
    if (!data || size < 4) {
        return false;
    }
    return (data[0] == 0x00 && data[1] == 0x00 &&
            ((data[2] == 0x01) || (data[2] == 0x00 && data[3] == 0x01)));
}

static quint32 readBeN(const uint8_t *p, int n)
{
    quint32 v = 0;
    for (int i = 0; i < n; ++i) {
        v = (v << 8) | p[i];
    }
    return v;
}
}

Mp4MuxDemux::Mp4MuxDemux(QObject *parent)
    : QObject(parent)
{
}

Mp4MuxDemux::~Mp4MuxDemux()
{
    closeWrite();
    closeRead();
}

bool Mp4MuxDemux::openForWrite(const QString &path, const VideoConfig &videoCfg)
{
    closeWrite();
    m_lastError.clear();

    m_videoCfg = videoCfg;
    m_sps.clear();
    m_pps.clear();
    m_headerWritten = false;
    m_writeOpened = false;
    clearPendingVideo();

    // 创建 MP4 输出上下文（容器级）
    int ret = avformat_alloc_output_context2(&m_outFmt, nullptr, "mp4", path.toUtf8().constData());
    if (ret < 0 || !m_outFmt) {
        m_lastError = QString("avformat_alloc_output_context2 failed: %1").arg(ffErr(ret));
        return false;
    }

    // 新建视频流（这里不做编码，只声明“这是 H264 码流”）
    m_videoOutStream = avformat_new_stream(m_outFmt, nullptr);
    if (!m_videoOutStream) {
        m_lastError = "avformat_new_stream(video) failed";
        closeWrite();
        return false;
    }

    m_videoOutStream->time_base = AVRational{m_videoCfg.timebaseNum, m_videoCfg.timebaseDen};
    AVCodecParameters *videoPar = m_videoOutStream->codecpar;
    videoPar->codec_type = AVMEDIA_TYPE_VIDEO;
    videoPar->codec_id = AV_CODEC_ID_H264;
    videoPar->width = m_videoCfg.width;
    videoPar->height = m_videoCfg.height;
    videoPar->format = AV_PIX_FMT_RGB24;

    // 打开输出文件句柄
    if (!(m_outFmt->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_outFmt->pb, path.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            m_lastError = QString("avio_open failed: %1").arg(ffErr(ret));
            closeWrite();
            return false;
        }
    }

    m_writeOpened = true;
    return true;
}

bool Mp4MuxDemux::setAudioConfigForWrite(const AudioConfig &audioCfg)
{
    if (!m_outFmt || !m_writeOpened) {
        m_lastError = "setAudioConfigForWrite called before openForWrite";
        return false;
    }
    if (m_headerWritten) {
        m_lastError = "setAudioConfigForWrite must be called before header is written";
        return false;
    }

    if (m_audioOutStream) {
        return true;
    }

    m_audioCfg = audioCfg;
    m_audioOutStream = avformat_new_stream(m_outFmt, nullptr);
    if (!m_audioOutStream) {
        m_lastError = "avformat_new_stream(audio) failed";
        return false;
    }

    AVCodecParameters *audioPar = m_audioOutStream->codecpar;
    audioPar->codec_type = AVMEDIA_TYPE_AUDIO;
    audioPar->codec_id = m_audioCfg.codecId > 0 ? AVCodecID(m_audioCfg.codecId) : AV_CODEC_ID_AAC;
    audioPar->sample_rate = m_audioCfg.sampleRate;
    audioPar->channels = m_audioCfg.channels;
    audioPar->channel_layout = av_get_default_channel_layout(m_audioCfg.channels);
    audioPar->bit_rate = m_audioCfg.bitRate;

    m_audioOutStream->time_base = AVRational{1, (m_audioCfg.sampleRate > 0 ? m_audioCfg.sampleRate : 48000)};
    m_audioConfigured = true;
    return true;
}

bool Mp4MuxDemux::buildVideoExtradataFromAnnexB(const uint8_t *data, int size)
{
    if (!data || size <= 0) {
        return false;
    }

    // 从 AnnexB 码流中解析 SPS/PPS，用于生成 MP4 所需 avcC(extradata)
    int pos = 0;
    while (pos < size) {
        int scPos = -1;
        int scLen = 0;
        if (!findStartCode(data, size, pos, &scPos, &scLen)) {
            break;
        }

        int nalStart = scPos + scLen;
        int nextScPos = -1;
        int nextScLen = 0;
        if (!findStartCode(data, size, nalStart, &nextScPos, &nextScLen)) {
            nextScPos = size;
        }

        int nalLen = nextScPos - nalStart;
        if (nalLen > 0) {
            uint8_t nalType = data[nalStart] & 0x1F;
            if (nalType == 7 && m_sps.isEmpty()) {
                m_sps = QByteArray(reinterpret_cast<const char *>(data + nalStart), nalLen);
            } else if (nalType == 8 && m_pps.isEmpty()) {
                m_pps = QByteArray(reinterpret_cast<const char *>(data + nalStart), nalLen);
            }
        }

        pos = nextScPos;
    }

    if (m_sps.isEmpty() || m_pps.isEmpty()) {
        return false;
    }

    // 将 SPS/PPS 组装成 AVCDecoderConfigurationRecord(avcC)
    QByteArray avcc = buildAvccFromSpsPps(m_sps, m_pps);
    if (avcc.isEmpty()) {
        return false;
    }

    AVCodecParameters *par = m_videoOutStream->codecpar;
    av_freep(&par->extradata);
    par->extradata_size = 0;

    par->extradata = reinterpret_cast<uint8_t *>(av_malloc(static_cast<size_t>(avcc.size()) + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!par->extradata) {
        m_lastError = "alloc video extradata failed";
        return false;
    }

    memcpy(par->extradata, avcc.constData(), static_cast<size_t>(avcc.size()));
    memset(par->extradata + avcc.size(), 0, AV_INPUT_BUFFER_PADDING_SIZE);
    par->extradata_size = avcc.size();
    return true;
}

bool Mp4MuxDemux::ensureWriteHeader()
{
    if (m_headerWritten) {
        return true;
    }

    if (!m_videoOutStream || !m_outFmt) {
        m_lastError = "mux context not ready";
        return false;
    }

    // MP4 写头前必须有 H264 extradata（也就是 SPS/PPS 形成的 avcC）
    if (m_videoOutStream->codecpar->extradata_size <= 0) {
        return false;
    }

    int ret = avformat_write_header(m_outFmt, nullptr);
    if (ret < 0) {
        m_lastError = QString("avformat_write_header failed: %1").arg(ffErr(ret));
        return false;
    }

    m_headerWritten = true;

    // 写头成功后，回放之前缓存的前几帧视频包
    for (const PendingVideo &pv : m_pendingVideo) {
        if (!cacheOrWriteVideo(reinterpret_cast<const uint8_t *>(pv.data.constData()), pv.data.size(), pv.ptsUs, pv.keyFrame)) {
            return false;
        }
    }
    clearPendingVideo();
    return true;
}

void Mp4MuxDemux::clearPendingVideo()
{
    m_pendingVideo.clear();
}

qint64 Mp4MuxDemux::rescaleToStreamTimebase(qint64 ptsUs, const AVRational &tb) const
{
    return av_rescale_q(ptsUs, AVRational{1, 1000000}, tb);
}

qint64 Mp4MuxDemux::rescaleToUs(qint64 ts, const AVRational &tb) const
{
    return av_rescale_q(ts, tb, AVRational{1, 1000000});
}

bool Mp4MuxDemux::cacheOrWriteVideo(const uint8_t *data, int size, qint64 ptsUs, bool keyFrame)
{
    if (!m_headerWritten) {
        // 还没写 header 时先缓存，避免丢掉首帧
        PendingVideo pv;
        pv.data = QByteArray(reinterpret_cast<const char *>(data), size);
        pv.ptsUs = ptsUs;
        pv.keyFrame = keyFrame;
        m_pendingVideo.push_back(pv);
        return true;
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = const_cast<uint8_t *>(data);
    pkt.size = size;
    pkt.stream_index = m_videoOutStream->index;
    pkt.flags = keyFrame ? AV_PKT_FLAG_KEY : 0;

    qint64 ts = rescaleToStreamTimebase(ptsUs, m_videoOutStream->time_base);
    pkt.pts = ts;
    pkt.dts = ts;
    pkt.duration = av_rescale_q(1, AVRational{1, m_videoCfg.fps > 0 ? m_videoCfg.fps : 30}, m_videoOutStream->time_base);

    int ret = av_interleaved_write_frame(m_outFmt, &pkt);
    if (ret < 0) {
        m_lastError = QString("av_interleaved_write_frame(video) failed: %1").arg(ffErr(ret));
        return false;
    }

    return true;
}

bool Mp4MuxDemux::writeVideoPacket(const uint8_t *data, int size, qint64 ptsUs, bool keyFrame)
{
    if (!m_writeOpened || !m_outFmt || !m_videoOutStream) {
        m_lastError = "writeVideoPacket called before openForWrite";
        return false;
    }
    if (!data || size <= 0) {
        m_lastError = "writeVideoPacket invalid data";
        return false;
    }

    // 首次写视频时尽量从码流里提取 SPS/PPS，供后续写 header
    if (m_videoOutStream->codecpar->extradata_size <= 0) {
        buildVideoExtradataFromAnnexB(data, size);
    }

    if (!m_headerWritten) {
        if (!ensureWriteHeader()) {
            // header not ready yet, packet has been cached when needed
            if (m_videoOutStream->codecpar->extradata_size <= 0) {
                PendingVideo pv;
                pv.data = QByteArray(reinterpret_cast<const char *>(data), size);
                pv.ptsUs = ptsUs;
                pv.keyFrame = keyFrame;
                m_pendingVideo.push_back(pv);
                return true;
            }
            return false;
        }
    }

    return cacheOrWriteVideo(data, size, ptsUs, keyFrame);
}

bool Mp4MuxDemux::writeAudioPacket(const uint8_t *data, int size, qint64 ptsUs)
{
    if (!m_writeOpened || !m_outFmt) {
        m_lastError = "writeAudioPacket called before openForWrite";
        return false;
    }
    if (!m_audioOutStream) {
        m_lastError = "audio stream not configured";
        return false;
    }
    if (!m_headerWritten) {
        if (!ensureWriteHeader()) {
            m_lastError = "header is not ready before writeAudioPacket";
            return false;
        }
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = const_cast<uint8_t *>(data);
    pkt.size = size;
    pkt.stream_index = m_audioOutStream->index;
    qint64 ts = rescaleToStreamTimebase(ptsUs, m_audioOutStream->time_base);
    pkt.pts = ts;
    pkt.dts = ts;

    int ret = av_interleaved_write_frame(m_outFmt, &pkt);
    if (ret < 0) {
        m_lastError = QString("av_interleaved_write_frame(audio) failed: %1").arg(ffErr(ret));
        return false;
    }

    return true;
}

void Mp4MuxDemux::closeWrite()
{
    if (m_outFmt) {
        if (m_headerWritten) {
            // 结束 MP4：写 trailer（moov 等索引信息在这里落盘）
            av_write_trailer(m_outFmt);
        }

        if (!(m_outFmt->oformat->flags & AVFMT_NOFILE) && m_outFmt->pb) {
            avio_closep(&m_outFmt->pb);
        }

        avformat_free_context(m_outFmt);
    }

    m_outFmt = nullptr;
    m_videoOutStream = nullptr;
    m_audioOutStream = nullptr;
    m_writeOpened = false;
    m_headerWritten = false;
    m_audioConfigured = false;
    clearPendingVideo();
    m_sps.clear();
    m_pps.clear();
}

bool Mp4MuxDemux::openForRead(const QString &path)
{
    closeRead();
    m_lastError.clear();

    // 打开输入文件并解析流信息
    int ret = avformat_open_input(&m_inFmt, path.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        m_lastError = QString("avformat_open_input failed: %1").arg(ffErr(ret));
        return false;
    }

    ret = avformat_find_stream_info(m_inFmt, nullptr);
    if (ret < 0) {
        m_lastError = QString("avformat_find_stream_info failed: %1").arg(ffErr(ret));
        closeRead();
        return false;
    }

    for (unsigned int i = 0; i < m_inFmt->nb_streams; ++i) {
        AVCodecParameters *par = m_inFmt->streams[i]->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_VIDEO && m_videoIndex < 0) {
            m_videoIndex = static_cast<int>(i);
        } else if (par->codec_type == AVMEDIA_TYPE_AUDIO && m_audioIndex < 0) {
            m_audioIndex = static_cast<int>(i);
        }
    }

    if (m_videoIndex < 0 && m_audioIndex < 0) {
        m_lastError = "no audio/video streams in file";
        closeRead();
        return false;
    }

    // 如果是 H264，解析 avcC，准备把 MP4 的 AVCC 包还原为 AnnexB 包
    if (m_videoIndex >= 0) {
        AVCodecParameters *vpar = m_inFmt->streams[m_videoIndex]->codecpar;
        if (vpar->codec_id == AV_CODEC_ID_H264) {
            if (!initH264AvccInfoFromExtradata()) {
                closeRead();
                return false;
            }
        }
    }

    m_readOpened = true;
    return true;
}

bool Mp4MuxDemux::initH264AvccInfoFromExtradata()
{
    m_h264NalLenSize = 4;
    m_h264AnnexbExtradata.clear();

    if (m_videoIndex < 0 || !m_inFmt) {
        m_lastError = "invalid video stream for avcc init";
        return false;
    }

    AVCodecParameters *vpar = m_inFmt->streams[m_videoIndex]->codecpar;
    if (!vpar || !vpar->extradata || vpar->extradata_size < 7) {
        // Some files may already be annexb-like packets; fallback to 4-byte NAL size parsing.
        return true;
    }

    const uint8_t *p = vpar->extradata;
    int sz = vpar->extradata_size;

    // 解析 AVCDecoderConfigurationRecord(avcC)：
    // - 提取 NAL 长度字段字节数（1~4）
    // - 提取 SPS/PPS 并转成 AnnexB 前缀，后续关键帧前可注入
    if (p[0] != 1) {
        return true;
    }

    m_h264NalLenSize = (p[4] & 0x03) + 1;
    if (m_h264NalLenSize < 1 || m_h264NalLenSize > 4) {
        m_h264NalLenSize = 4;
    }

    int offset = 5;
    int numSps = p[offset] & 0x1F;
    ++offset;

    QByteArray annexbExtra;
    for (int i = 0; i < numSps; ++i) {
        if (offset + 2 > sz) {
            break;
        }
        int spsLen = (p[offset] << 8) | p[offset + 1];
        offset += 2;
        if (offset + spsLen > sz) {
            break;
        }
        annexbExtra.append("\x00\x00\x00\x01", 4);
        annexbExtra.append(reinterpret_cast<const char *>(p + offset), spsLen);
        offset += spsLen;
    }

    if (offset >= sz) {
        m_h264AnnexbExtradata = annexbExtra;
        return true;
    }

    int numPps = p[offset];
    ++offset;
    for (int i = 0; i < numPps; ++i) {
        if (offset + 2 > sz) {
            break;
        }
        int ppsLen = (p[offset] << 8) | p[offset + 1];
        offset += 2;
        if (offset + ppsLen > sz) {
            break;
        }
        annexbExtra.append("\x00\x00\x00\x01", 4);
        annexbExtra.append(reinterpret_cast<const char *>(p + offset), ppsLen);
        offset += ppsLen;
    }

    m_h264AnnexbExtradata = annexbExtra;
    return true;
}

QByteArray Mp4MuxDemux::convertH264AvccToAnnexB(const uint8_t *data, int size, bool keyFrame) const
{
    QByteArray out;
    if (!data || size <= 0) {
        return out;
    }

    // 已是 AnnexB 则直接透传
    if (hasAnnexBStartCode(data, size)) {
        out.append(reinterpret_cast<const char *>(data), size);
        return out;
    }

    if (m_h264NalLenSize < 1 || m_h264NalLenSize > 4) {
        return out;
    }

    // 关键帧前补一份 SPS/PPS，便于下游硬解码器独立起播
    if (keyFrame && !m_h264AnnexbExtradata.isEmpty()) {
        out.append(m_h264AnnexbExtradata);
    }

    int off = 0;
    while (off + m_h264NalLenSize <= size) {
        quint32 nalLen = readBeN(data + off, m_h264NalLenSize);
        off += m_h264NalLenSize;
        if (nalLen == 0) {
            continue;
        }
        if (off + static_cast<int>(nalLen) > size) {
            out.clear();
            return out;
        }
        out.append("\x00\x00\x00\x01", 4);
        out.append(reinterpret_cast<const char *>(data + off), static_cast<int>(nalLen));
        off += static_cast<int>(nalLen);
    }

    if (out.isEmpty()) {
        out.append(reinterpret_cast<const char *>(data), size);
    }

    return out;
}

bool Mp4MuxDemux::handleDemuxPacket(AVPacket *pkt, Packet *outPacket)
{
    if (!pkt || !outPacket) {
        m_lastError = "handleDemuxPacket invalid argument";
        return false;
    }

    outPacket->eof = false;

    if (pkt->stream_index == m_videoIndex) {
        outPacket->type = StreamVideo;
        outPacket->keyFrame = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
        outPacket->ptsUs = pkt->pts == AV_NOPTS_VALUE ? -1 : rescaleToUs(pkt->pts, m_inFmt->streams[m_videoIndex]->time_base);
        outPacket->dtsUs = pkt->dts == AV_NOPTS_VALUE ? -1 : rescaleToUs(pkt->dts, m_inFmt->streams[m_videoIndex]->time_base);

        // 对 H264 视频包做 AVCC->AnnexB，和你当前 VDEC 喂流方式对齐
        AVCodecParameters *vpar = m_inFmt->streams[m_videoIndex]->codecpar;
        if (vpar && vpar->codec_id == AV_CODEC_ID_H264) {
            outPacket->data = convertH264AvccToAnnexB(pkt->data, pkt->size, outPacket->keyFrame);
            if (outPacket->data.isEmpty() && pkt->size > 0) {
                m_lastError = "convertH264AvccToAnnexB failed";
                return false;
            }
        } else {
            outPacket->data = QByteArray(reinterpret_cast<const char *>(pkt->data), pkt->size);
        }
        return true;
    }

    if (pkt->stream_index == m_audioIndex) {
        outPacket->type = StreamAudio;
        outPacket->keyFrame = false;
        outPacket->ptsUs = pkt->pts == AV_NOPTS_VALUE ? -1 : rescaleToUs(pkt->pts, m_inFmt->streams[m_audioIndex]->time_base);
        outPacket->dtsUs = pkt->dts == AV_NOPTS_VALUE ? -1 : rescaleToUs(pkt->dts, m_inFmt->streams[m_audioIndex]->time_base);
        outPacket->data = QByteArray(reinterpret_cast<const char *>(pkt->data), pkt->size);
        return true;
    }

    outPacket->type = StreamUnknown;
    outPacket->data.clear();
    outPacket->ptsUs = -1;
    outPacket->dtsUs = -1;
    outPacket->keyFrame = false;
    return true;
}

bool Mp4MuxDemux::readNextPacket(Packet *outPacket)
{
    if (!m_readOpened || !m_inFmt) {
        m_lastError = "readNextPacket called before openForRead";
        return false;
    }
    if (!outPacket) {
        m_lastError = "readNextPacket output is null";
        return false;
    }

    AVPacket pkt;
    av_init_packet(&pkt);

    int ret = av_read_frame(m_inFmt, &pkt);
    if (ret == AVERROR_EOF) {
        outPacket->type = StreamUnknown;
        outPacket->data.clear();
        outPacket->ptsUs = -1;
        outPacket->dtsUs = -1;
        outPacket->keyFrame = false;
        outPacket->eof = true;
        return true;
    }
    if (ret < 0) {
        m_lastError = QString("av_read_frame failed: %1").arg(ffErr(ret));
        return false;
    }

    bool ok = handleDemuxPacket(&pkt, outPacket);
    av_packet_unref(&pkt);
    return ok;
}

void Mp4MuxDemux::closeRead()
{
    if (m_inFmt) {
        avformat_close_input(&m_inFmt);
        m_inFmt = nullptr;
    }

    m_videoIndex = -1;
    m_audioIndex = -1;
    m_h264NalLenSize = 4;
    m_h264AnnexbExtradata.clear();
    m_readOpened = false;
}

QString Mp4MuxDemux::lastError() const
{
    return m_lastError;
}

void Mp4MuxDemux::slotStartMux(const QString &path, int width, int height, int fps)
{
    VideoConfig cfg;
    cfg.width = width;
    cfg.height = height;
    cfg.fps = fps;

    bool ok = openForWrite(path, cfg);
    if (ok) {
        emit sigMuxOpened(true, QStringLiteral("mp4 mux opened"));
    } else {
        emit sigMuxOpened(false, m_lastError);
        emit sigMuxError(m_lastError);
    }
}

void Mp4MuxDemux::slotStopMux()
{
    closeWrite();
    printf("slotStopMux\r\n");
    emit sigMuxClosed();
}

void Mp4MuxDemux::slotWriteVideoEncoded(const QByteArray &data, qint64 ptsUs, bool keyFrame)
{
    bool ok = writeVideoPacket(reinterpret_cast<const uint8_t *>(data.constData()), data.size(), ptsUs, keyFrame);
    if (!ok) {
        emit sigMuxError(m_lastError);
    }
}

void Mp4MuxDemux::slotWriteAudioEncoded(const QByteArray &data, qint64 ptsUs)
{
    bool ok = writeAudioPacket(reinterpret_cast<const uint8_t *>(data.constData()), data.size(), ptsUs);
    if (!ok) {
        emit sigMuxError(m_lastError);
    }
}

void Mp4MuxDemux::slotStartDemux(const QString &path)
{
    bool ok = openForRead(path);
    if (ok) {
        emit sigDemuxOpened(true, QStringLiteral("mp4 demux opened"));
    } else {
        emit sigDemuxOpened(false, m_lastError);
        emit sigDemuxError(m_lastError);
    }
}

void Mp4MuxDemux::slotStopDemux()
{
    closeRead();
    emit sigDemuxClosed();
}

void Mp4MuxDemux::slotPullNextPacket()
{
    Packet pkt;
    bool ok = readNextPacket(&pkt);
    if (!ok) {
        emit sigDemuxError(m_lastError);
        return;
    }

    if (pkt.eof) {
        emit sigDemuxPacketReady(StreamUnknown, QByteArray(), -1, -1, false, true);
        emit sigDemuxEof();
        return;
    }

    emit sigDemuxPacketReady(pkt.type, pkt.data, pkt.ptsUs, pkt.dtsUs, pkt.keyFrame, false);
}
