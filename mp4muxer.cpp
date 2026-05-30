/**
 * MP4封装和解封装
 * 1. H.264 -> MP4
 * - H.264 有两种主要的封装格式，一种是 Annex-B 格式，一种是 AVCC 格式
 * - 编码器输出的H.264码流是Annex-B格式的裸码流，因此需要将其手动转为AVCC格式，再写入MP4
 * - MP4文件头需要SPS和PPS信息，注意这里也要转为AVCC格式，否则MP4文件无法播放
 * - 因此封装MP4的基本步骤为：
 *      ① 开始写MP4，初始化MP4容器输出上下文 
 *      ② 读取H.264裸码流，从中提取出SPS和PPS信息
 *      ③ 添加视频流，将SPS+PPS封装为AVCC，然后写MP4容器头
 *      ④ 将Annex-B码流转换为AVCC格式，然后写入MP4帧
 *      ⑤ 结束写MP4，释放相关变量
 */
#include "mp4muxer.h"
#include "audiomanager.h"
#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <cerrno>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

namespace {
constexpr int kEncWidth = 1920;
constexpr int kEncHeight = 1080;
constexpr int kAudioBitrate = 64000; // 语音场景下 AAC 64kbps 一般足够

std::string ffErrStr(int errnum)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, errbuf, sizeof(errbuf));
    return std::string(errbuf);
}

uint64_t channelLayoutFromChannels(int channels)
{
    return channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
}


/*
 * 将 Annex-B 中提取出的 SPS/PPS 组装为 MP4(H.264) 需要的 avcC 数据块
 * (AVCDecoderConfigurationRecord)。
 *
 * 为什么需要它：
 * - MP4 容器中的 H.264 轨道不会直接在头部保存 Annex-B 起始码格式参数；
 * - 解码初始化信息要放在 codec extradata 中，格式即 avcC；
 * - 后续写 header 时会把该返回值写入 AVCodecParameters::extradata。
 *
 * 入参：
 * - sps: 不带起始码的 SPS NALU 原始字节（通常 nal_type=7）
 * - pps: 不带起始码的 PPS NALU 原始字节（通常 nal_type=8）
 *
 * 返回：
 * - 成功：符合 avcC 结构的字节序列
 * - 失败：空 QByteArray（例如 SPS 太短或 PPS 为空）
 *
 * avcC 结构（本实现只写单个 SPS + 单个 PPS）：
 * - configurationVersion = 0x01
 * - AVCProfileIndication / profile_compatibility / AVCLevelIndication <- 来自 SPS[1..3]
 * - lengthSizeMinusOne = 3（即 NALU 长度前缀 4 字节）
 * - numOfSequenceParameterSets = 1，随后写 SPS 长度和内容
 * - numOfPictureParameterSets  = 1，随后写 PPS 长度和内容
 */
QByteArray buildAvccFromSpsPps(const QByteArray &sps, const QByteArray &pps)
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
    avcc.append(char(0xFF));
    avcc.append(char(0xE1));
    avcc.append(char((sps.size() >> 8) & 0xFF));
    avcc.append(char(sps.size() & 0xFF));
    avcc.append(sps);
    avcc.append(char(0x01));
    avcc.append(char((pps.size() >> 8) & 0xFF));
    avcc.append(char(pps.size() & 0xFF));
    avcc.append(pps);
    return avcc;
}

/*
 * 在 Annex-B 格式的 H.264 码流中查找下一个起始码（start code）位置。
 *
 * 参数说明：
 * - buf:
 *   输入码流缓冲区首地址。
 * - size:
 *   缓冲区总长度（字节数）。
 * - from:
 *   查找起始偏移（从 buf[from] 开始向后扫描）。
 * - scLen:
 *   输出参数，返回匹配到的起始码长度：
 *     3 -> 00 00 01
 *     4 -> 00 00 00 01
 *
 * 返回值：
 * - >= 0:
 *   找到起始码时，返回起始码第一个字节在 buf 中的下标（offset）。
 * - -1:
 *   未找到起始码，或输入参数无效（如 buf/scLen 为空，from >= size）。
 */
int findAnnexbStartCode(const uint8_t *buf, uint32_t size, uint32_t from, uint32_t *scLen)
{
    if (!buf || !scLen || from >= size) {
        return -1;
    }
    for (uint32_t p = from; p + 3 < size; ++p) {
        if (buf[p] != 0x00 || buf[p + 1] != 0x00) {
            continue;
        }
        if (buf[p + 2] == 0x01) {
            *scLen = 3;
            return static_cast<int>(p);
        }
        if (p + 4 < size && buf[p + 2] == 0x00 && buf[p + 3] == 0x01) {
            *scLen = 4;
            return static_cast<int>(p);
        }
    }
    return -1;
}

bool annexbToAvcc(const uint8_t *data, uint32_t size, QByteArray &out)
{
    out.clear();
    if (!data || size == 0) {
        return false;
    }

    uint32_t scLen = 0;
    int start = findAnnexbStartCode(data, size, 0, &scLen);
    if (start < 0) {
        out.resize(static_cast<int>(4 + size));
        out[0] = char((size >> 24) & 0xFF);
        out[1] = char((size >> 16) & 0xFF);
        out[2] = char((size >> 8) & 0xFF);
        out[3] = char(size & 0xFF);
        memcpy(out.data() + 4, data, size);
        return true;
    }

    while (start >= 0) {
        uint32_t naluStart = static_cast<uint32_t>(start) + scLen;
        uint32_t nextScLen = 0;
        int next = findAnnexbStartCode(data, size, naluStart, &nextScLen);
        uint32_t naluEnd = next >= 0 ? static_cast<uint32_t>(next) : size;
        if (naluEnd > naluStart) {
            uint32_t naluLen = naluEnd - naluStart;
            char lenBytes[4] = {
                char((naluLen >> 24) & 0xFF),
                char((naluLen >> 16) & 0xFF),
                char((naluLen >> 8) & 0xFF),
                char(naluLen & 0xFF)
            };
            out.append(lenBytes, 4);
            out.append(reinterpret_cast<const char *>(data + naluStart), static_cast<int>(naluLen));
        }
        start = next;
        scLen = nextScLen;
    }
    return !out.isEmpty();
}
} // namespace

Mp4Muxer::Mp4Muxer(QObject *parent)
    : QObject(parent)
{
}

Mp4Muxer::~Mp4Muxer()
{
    closeMp4Mux();
}

int Mp4Muxer::demuxMp4ToH264(const QString &mp4Path,
                                const QString &h264OutPath,
                                const QString &audioOutPath)
{
    Q_UNUSED(audioOutPath); // 预留音频解封装接口，当前播放链路先只接视频。

    AVFormatContext *inFmt = nullptr;
    AVBSFContext *bsfCtx = nullptr;
    FILE *videoFp = nullptr;
    int videoStreamIndex = -1;
    int ret = 0;

    ret = avformat_open_input(&inFmt, mp4Path.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        std::cerr << "avformat_open_input failed: "
                  << ffErrStr(ret) << " path=" << mp4Path.toStdString() << std::endl;
        return ret;
    }

    ret = avformat_find_stream_info(inFmt, nullptr);
    if (ret < 0) {
        std::cerr << "avformat_find_stream_info failed: "
                  << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        avformat_close_input(&inFmt);
        return ret;
    }

    ret = av_find_best_stream(inFmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ret < 0) {
        std::cerr << "av_find_best_stream(video) failed: "
                  << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        avformat_close_input(&inFmt);
        return ret;
    }
    videoStreamIndex = ret;

    AVStream *videoStream = inFmt->streams[videoStreamIndex];
    if (videoStream->codecpar->codec_id != AV_CODEC_ID_H264) {
        std::cerr << "demuxMp4ToH264: unsupported video codec id="
                  << videoStream->codecpar->codec_id << ", only H.264 is supported" << std::endl;
        avformat_close_input(&inFmt);
        return AVERROR(EINVAL);
    }

    const AVBitStreamFilter *bsf = av_bsf_get_by_name("h264_mp4toannexb");
    if (!bsf) {
        std::cerr << "av_bsf_get_by_name(h264_mp4toannexb) failed" << std::endl;
        avformat_close_input(&inFmt);
        return AVERROR_BSF_NOT_FOUND;
    }

    ret = av_bsf_alloc(bsf, &bsfCtx);
    if (ret < 0) {
        std::cerr << "av_bsf_alloc failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        avformat_close_input(&inFmt);
        return ret;
    }

    ret = avcodec_parameters_copy(bsfCtx->par_in, videoStream->codecpar);
    if (ret < 0) {
        std::cerr << "avcodec_parameters_copy failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        av_bsf_free(&bsfCtx);
        avformat_close_input(&inFmt);
        return ret;
    }
    bsfCtx->time_base_in = videoStream->time_base;

    ret = av_bsf_init(bsfCtx);
    if (ret < 0) {
        std::cerr << "av_bsf_init failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        av_bsf_free(&bsfCtx);
        avformat_close_input(&inFmt);
        return ret;
    }

    videoFp = fopen(h264OutPath.toUtf8().constData(), "wb");
    if (!videoFp) {
        perror("fopen h264OutPath");
        av_bsf_free(&bsfCtx);
        avformat_close_input(&inFmt);
        return AVERROR(errno);
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    while ((ret = av_read_frame(inFmt, &pkt)) >= 0) {
        if (pkt.stream_index == videoStreamIndex) {
            ret = av_bsf_send_packet(bsfCtx, &pkt);
            av_packet_unref(&pkt);
            if (ret < 0) {
                std::cerr << "av_bsf_send_packet failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
                break;
            }

            while ((ret = av_bsf_receive_packet(bsfCtx, &pkt)) == 0) {
                if (pkt.size > 0 && fwrite(pkt.data, 1, pkt.size, videoFp) != static_cast<size_t>(pkt.size)) {
                    perror("fwrite h264");
                    av_packet_unref(&pkt);
                    ret = AVERROR(EIO);
                    break;
                }
                av_packet_unref(&pkt);
            }
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                ret = 0;
            }
            if (ret < 0) {
                break;
            }
        } else {
            av_packet_unref(&pkt);
        }
    }

    if (ret == AVERROR_EOF) {
        ret = 0;
    }

    if (ret == 0) {
        ret = av_bsf_send_packet(bsfCtx, nullptr);
        if (ret >= 0) {
            while ((ret = av_bsf_receive_packet(bsfCtx, &pkt)) == 0) {
                if (pkt.size > 0 && fwrite(pkt.data, 1, pkt.size, videoFp) != static_cast<size_t>(pkt.size)) {
                    perror("fwrite h264 flush");
                    av_packet_unref(&pkt);
                    ret = AVERROR(EIO);
                    break;
                }
                av_packet_unref(&pkt);
            }
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                ret = 0;
            }
        }
    }

    fclose(videoFp);
    av_bsf_free(&bsfCtx);
    avformat_close_input(&inFmt);
    return ret;
}


bool Mp4Muxer::initMp4Mux(const QString &filename)
{
    closeMp4Mux();

    // 创建 MP4 输出上下文（容器级）
    int ret = avformat_alloc_output_context2(&m_outFmt, nullptr, "mp4", filename.toUtf8().constData());
    if (ret < 0 || !m_outFmt) {
        std::cerr << "avformat_alloc_output_context2 failed: "
                  << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return false;
    }

    if (!(m_outFmt->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_outFmt->pb, filename.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            std::cerr << "avio_open failed: "
                      << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
            avformat_free_context(m_outFmt);
            m_outFmt = nullptr;
            return false;
        }
    }

    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_headerWritten = false;
    m_videoPtsInited = false;
    m_audioPtsInited = false;
    m_syncBaseInited = false;
    m_videoPtsBaseUs = 0;
    m_syncBaseUs = 0;
    m_audioNextPts = 0;
    m_audioInputChannels = gAudioCaptureConfig.outChannel;
    m_audioInputBytesPerSample = 2; // AudioCapture 当前输出 s16
    m_audioPcmCache.clear();
    m_pendingAudioChunks.clear();
    m_sps.clear();
    m_pps.clear();

    // 先创建音频编码器和音频流。MP4 header 尚未写，等拿到视频 SPS/PPS 后一起写 header。
    if (initAudioEncoder() != 0) {
        closeMp4Mux();
        return false;
    }

    return true;
}

void Mp4Muxer::closeMp4Mux()
{
    if (!m_outFmt) {
        return;
    }
    if (m_headerWritten) {
        flushAudioEncoder();
    }

    if (m_headerWritten && m_outFmt->pb) {
        int ret = av_write_trailer(m_outFmt);
        if (ret < 0) {
            std::cerr << "av_write_trailer failed: "
                      << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        }
    }
    if (!(m_outFmt->oformat->flags & AVFMT_NOFILE) && m_outFmt->pb) {
        avio_closep(&m_outFmt->pb);
    }
    releaseAudioEncoder();
    avformat_free_context(m_outFmt);
    m_outFmt = nullptr;
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_headerWritten = false;
    m_videoPtsInited = false;
    m_audioPtsInited = false;
    m_syncBaseInited = false;
    m_videoPtsBaseUs = 0;
    m_syncBaseUs = 0;
    m_audioNextPts = 0;
    m_audioPcmCache.clear();
    m_pendingAudioChunks.clear();
    m_sps.clear();
    m_pps.clear();
}

void Mp4Muxer::ensureSyncBase(uint64_t tsUs)
{
    // 音视频统一同步基准：
    // - 第一份到达的音频/视频时间戳作为 m_syncBaseUs
    // - 后续所有流都减去该基准，得到同一“录制起点”下的相对时间
    if (!m_syncBaseInited) {
        m_syncBaseInited = true;
        m_syncBaseUs = tsUs;
    }
}

int Mp4Muxer::initAudioEncoder()
{
    if (!m_outFmt || m_audioEncCtx) {
        return -1;
    }

    const AVCodec *acodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!acodec) {
        std::cerr << "initAudioEncoder: avcodec_find_encoder(AAC) failed" << std::endl;
        return -1;
    }

    m_audioEncCtx = avcodec_alloc_context3(acodec);
    if (!m_audioEncCtx) {
        return -1;
    }

    const int outSampleRate = gAudioCaptureConfig.outSampleRate > 0 ? gAudioCaptureConfig.outSampleRate : 16000;
    m_audioInputChannels = gAudioCaptureConfig.outChannel == 2 ? 2 : 1;
    const uint64_t inLayout = channelLayoutFromChannels(m_audioInputChannels);

    m_audioEncCtx->codec_type = AVMEDIA_TYPE_AUDIO;
    m_audioEncCtx->codec_id = AV_CODEC_ID_AAC;
    m_audioEncCtx->sample_rate = outSampleRate;
    m_audioEncCtx->channel_layout = inLayout;
    m_audioEncCtx->channels = m_audioInputChannels;
    m_audioEncCtx->bit_rate = kAudioBitrate;
    m_audioEncCtx->time_base = AVRational{1, outSampleRate};
    m_audioEncCtx->sample_fmt = acodec->sample_fmts ? acodec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    if (m_outFmt->oformat->flags & AVFMT_GLOBALHEADER) {
        m_audioEncCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    int ret = avcodec_open2(m_audioEncCtx, acodec, nullptr);
    if (ret < 0) {
        std::cerr << "avcodec_open2(aac) failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return -1;
    }

    AVStream *ast = avformat_new_stream(m_outFmt, nullptr);
    if (!ast) {
        std::cerr << "avformat_new_stream(audio) failed" << std::endl;
        return -1;
    }
    ast->id = static_cast<int>(m_outFmt->nb_streams - 1);
    ast->time_base = AVRational{1, outSampleRate};

    ret = avcodec_parameters_from_context(ast->codecpar, m_audioEncCtx);
    if (ret < 0) {
        std::cerr << "avcodec_parameters_from_context(audio) failed: "
                  << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return -1;
    }
    m_audioStreamIndex = ast->index;

    // 输入是 AudioCapture 输出的 s16 PCM；AAC 编码器通常是 fltp，需要 swr 做格式转换。
    m_swrCtx = swr_alloc_set_opts(
        nullptr,
        m_audioEncCtx->channel_layout,
        m_audioEncCtx->sample_fmt,
        m_audioEncCtx->sample_rate,
        inLayout,
        AV_SAMPLE_FMT_S16,
        outSampleRate,
        0,
        nullptr
    );
    if (!m_swrCtx) {
        std::cerr << "swr_alloc_set_opts failed" << std::endl;
        return -1;
    }
    ret = swr_init(m_swrCtx);
    if (ret < 0) {
        std::cerr << "swr_init failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return -1;
    }

    m_audioFrame = av_frame_alloc();
    if (!m_audioFrame) {
        return -1;
    }
    m_audioFrame->format = m_audioEncCtx->sample_fmt;
    m_audioFrame->channel_layout = m_audioEncCtx->channel_layout;
    m_audioFrame->sample_rate = m_audioEncCtx->sample_rate;
    m_audioFrame->nb_samples = m_audioEncCtx->frame_size > 0 ? m_audioEncCtx->frame_size : 1024;
    ret = av_frame_get_buffer(m_audioFrame, 0);
    if (ret < 0) {
        std::cerr << "av_frame_get_buffer(audio) failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return -1;
    }

    return 0;
}

void Mp4Muxer::releaseAudioEncoder()
{
    if (m_audioFrame) {
        av_frame_free(&m_audioFrame);
    }
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
    }
    if (m_audioEncCtx) {
        avcodec_free_context(&m_audioEncCtx);
    }
}

int Mp4Muxer::writeAudioFrame(AVFrame *frame)
{
    if (!m_outFmt || !m_audioEncCtx || m_audioStreamIndex < 0) {
        return -1;
    }
    int ret = avcodec_send_frame(m_audioEncCtx, frame);
    if (ret < 0) {
        std::cerr << "avcodec_send_frame(audio) failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return -1;
    }

    while (true) {
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.data = nullptr;
        pkt.size = 0;

        ret = avcodec_receive_packet(m_audioEncCtx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_unref(&pkt);
            break;
        }
        if (ret < 0) {
            std::cerr << "avcodec_receive_packet(audio) failed: "
                      << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
            av_packet_unref(&pkt);
            return -1;
        }

        AVStream *ast = m_outFmt->streams[m_audioStreamIndex];
        av_packet_rescale_ts(&pkt, m_audioEncCtx->time_base, ast->time_base);
        pkt.stream_index = m_audioStreamIndex;
        pkt.pos = -1;

        ret = av_interleaved_write_frame(m_outFmt, &pkt);
        av_packet_unref(&pkt);
        if (ret < 0) {
            std::cerr << "av_interleaved_write_frame(audio) failed: "
                      << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
            return -1;
        }
    }
    return 0;
}

int Mp4Muxer::processAudioChunk(const QByteArray &pcmData, uint64_t captureTsUs)
{
    if (!m_audioEncCtx || !m_audioFrame || !m_swrCtx) {
        return -1;
    }

    ensureSyncBase(captureTsUs);
    if (!m_audioPtsInited) {
        // 关键同步点：
        // - 音频首块的采集时间与全局基准做差，映射到“采样数时间轴”
        // - 后续按编码帧样本数递增，避免受线程抖动影响
        int64_t deltaUs = static_cast<int64_t>(captureTsUs - m_syncBaseUs);
        if (deltaUs < 0) deltaUs = 0;
        m_audioNextPts = av_rescale_q(deltaUs, AVRational{1, 1000000}, m_audioEncCtx->time_base);
        m_audioPtsInited = true;
    }

    m_audioPcmCache.append(pcmData);

    const int samplesPerFrame = m_audioFrame->nb_samples;
    const int bytesPerInputSample = m_audioInputChannels * m_audioInputBytesPerSample;
    const int bytesPerCodecFrame = samplesPerFrame * bytesPerInputSample;

    while (m_audioPcmCache.size() >= bytesPerCodecFrame) {
        int ret = av_frame_make_writable(m_audioFrame);
        if (ret < 0) {
            std::cerr << "av_frame_make_writable(audio) failed: "
                      << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
            return -1;
        }

        const uint8_t *srcData[1] = {
            reinterpret_cast<const uint8_t *>(m_audioPcmCache.constData())
        };
        ret = swr_convert(
            m_swrCtx,
            m_audioFrame->data,
            samplesPerFrame,
            srcData,
            samplesPerFrame
        );
        if (ret < 0) {
            std::cerr << "swr_convert failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
            return -1;
        }

        m_audioFrame->pts = m_audioNextPts;
        m_audioNextPts += m_audioFrame->nb_samples;

        if (writeAudioFrame(m_audioFrame) < 0) {
            return -1;
        }

        m_audioPcmCache.remove(0, bytesPerCodecFrame);
    }
    return 0;
}

void Mp4Muxer::flushPendingAudio()
{
    if (!m_headerWritten || m_pendingAudioChunks.isEmpty()) {
        return;
    }

    for (const AudioChunk &chunk : m_pendingAudioChunks) {
        processAudioChunk(chunk.pcm, chunk.tsUs);
    }
    m_pendingAudioChunks.clear();
}

void Mp4Muxer::flushAudioEncoder()
{
    if (!m_audioEncCtx || !m_audioFrame || !m_swrCtx) {
        return;
    }

    if (!m_audioPcmCache.isEmpty()) {
        // 收尾时不足一帧的 PCM，补 0 到完整 AAC 帧，避免尾音丢失。
        const int bytesPerInputSample = m_audioInputChannels * m_audioInputBytesPerSample;
        const int bytesPerCodecFrame = m_audioFrame->nb_samples * bytesPerInputSample;
        if (m_audioPcmCache.size() < bytesPerCodecFrame) {
            m_audioPcmCache.append(QByteArray(bytesPerCodecFrame - m_audioPcmCache.size(), '\0'));
        }
        QByteArray lastChunk = m_audioPcmCache;  // 拷贝补零后的数据
        m_audioPcmCache.clear();                  // 先清缓存，避免 processAudioChunk 重复 append
        processAudioChunk(lastChunk, m_syncBaseUs);
    }

    writeAudioFrame(nullptr); // flush encoder delayed packets
}

int Mp4Muxer::addMp4Stream()
{
    if (!m_outFmt) {
        return -1;
    }
    if (m_videoStreamIndex >= 0) {
        return 0;
    }

    AVOutputFormat *outFmt = m_outFmt->oformat;
    if (!outFmt || outFmt->video_codec == AV_CODEC_ID_NONE) {
        std::cerr << "addMp4Stream: invalid video codec id from output format" << std::endl;
        return -1;
    }

    const AVCodec *vcodec = avcodec_find_encoder(outFmt->video_codec);
    if (!vcodec) {
        std::cerr << "addMp4Stream: could not find video encoder" << std::endl;
        return -1;
    }
    AVStream *vst = avformat_new_stream(m_outFmt, vcodec);
    if (!vst) {
        std::cerr << "addMp4Stream: avformat_new_stream(video) failed" << std::endl;
        return -1;
    }
    vst->id = static_cast<int>(m_outFmt->nb_streams - 1);
    vst->time_base = AVRational{1, 1000000};  // 设置时间基：时间戳单位为us

    // 配置编码参数
    AVCodecParameters *vpar = vst->codecpar;  // 流的参数结构体
    vpar->codec_type = AVMEDIA_TYPE_VIDEO; // 视频流
    vpar->codec_id = AV_CODEC_ID_H264;     // 码流类型
    vpar->bit_rate = 4 * 1000 * 1000;      // 码率
    vpar->width = kEncWidth;
    vpar->height = kEncHeight;
    vpar->format = AV_PIX_FMT_YUV420P;

    m_videoStreamIndex = vst->index;
    return 0;
}

/*
 * 在 MP4 封装流程中完成“创建视频轨道配置 + 写容器头”。
 * - 在从H264码流中获取到SPS和PPS信息后，执行此函数
 * - 将SPS和PPS封装为avcc格式，然后写入AVCodecParameters结构体的extradata变量中
 * - 如果没有SPS和PPS信息就直接写header，会导致MP4文件无法打开，avcc格式的SPS+PPS信息就是告诉解码器如何解码视频帧的
 */
int Mp4Muxer::addSpsPpsAndWriteHeader()
{
    if (!m_outFmt || m_sps.isEmpty() || m_pps.isEmpty()) {
        return -1;
    }
    if (addMp4Stream() < 0) {  // 添加MP4视频流
        return -1;
    }
    if (m_videoStreamIndex < 0) {
        return -1;
    }

    AVStream *vst = m_outFmt->streams[m_videoStreamIndex];
    AVCodecParameters *par = vst->codecpar;

    QByteArray avcc = buildAvccFromSpsPps(m_sps, m_pps);  // 将SPS和PPS封装为AVCC格式
    if (avcc.isEmpty()) {
        std::cerr << "buildAvccFromSpsPps failed" << std::endl;
        return -1;
    }

    av_freep(&par->extradata);
    par->extradata = reinterpret_cast<uint8_t *>(av_malloc(avcc.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!par->extradata) {
        return -1;
    }
    memcpy(par->extradata, avcc.constData(), static_cast<size_t>(avcc.size()));
    memset(par->extradata + avcc.size(), 0, AV_INPUT_BUFFER_PADDING_SIZE);
    par->extradata_size = avcc.size();

    int ret = avformat_write_header(m_outFmt, nullptr); // 写MP4容器头
    if (ret < 0) {
        std::cerr << "avformat_write_header failed: "
                  << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return -1;
    }
    m_headerWritten = true;
    flushPendingAudio();
    return 0;
}

int Mp4Muxer::writeAudioToMp4(QByteArray pcmData, uint64_t captureTsUs)
{
    if (!m_outFmt || !m_audioEncCtx || pcmData.isEmpty()) {
        return 0;
    }

    if (!m_headerWritten) {
        // MP4 header 写完前不能写包；先缓存，待拿到 SPS/PPS 写完 header 后再补写。
        m_pendingAudioChunks.push_back({pcmData, captureTsUs});
        return 0;
    }

    return processAudioChunk(pcmData, captureTsUs);
}

int Mp4Muxer::writeVideoToMp4(VENC_STREAM_S *pstStream)
{
    if (!m_outFmt || !pstStream || pstStream->u32PackCount == 0 || !pstStream->pstPack) {
        return 0;
    }

    uint8_t spsBuf[128] = {0};     // 存储sps帧的缓冲区
    uint8_t ppsBuf[128] = {0};     // 存储pps帧的缓冲区
    uint32_t spsLen = 0;
    uint32_t ppsLen = 0;

    for (uint32_t i = 0; i < pstStream->u32PackCount; ++i) {  // u32PackCount 实际为1
        VENC_PACK_S *pack = &pstStream->pstPack[i];
        uint8_t *packBase = reinterpret_cast<uint8_t *>(RK_MPI_MB_Handle2VirAddr(pack->pMbBlk));
        if (!packBase || pack->u32Offset > pack->u32Len) {
            continue;
        }
        uint8_t *payload = packBase + pack->u32Offset;  // RK中u32Offset无效 这里就是0
        uint32_t payloadLen = pack->u32Len - pack->u32Offset;
        if (payloadLen == 0) {
            continue;
        }

        /**
         * 从 Annex-B 码流数据中找到PPS和SPS信息
         * - Annex-B 格式的H264码流由 起始码 和 NALU序列 组成 
         * - 每个 NALU 代表一段视频数据，比如一个 I 帧、一部分 P 帧、SPS、PPS 等
         * - 所以逻辑就是：
         * - 1. 先跳过起始码，找到有效NALU数据的位置
         * - 2. NALU数据的第一个字节的低5位存储了类型信息，找到SPS和PPS，然后将其保存
         */
        bool hasVideoSlice = m_headerWritten;  // 是否有视频帧
        if (!m_headerWritten) {  // 只写一次头 写完之后就不用再重复找PPS和SPS了 因为后面的PPS和SPS信息都是一样的
            uint32_t scLen = 0;
            int start = findAnnexbStartCode(payload, payloadLen, 0, &scLen);  // 查找起始码 返回起始码的开始位置
            if (start >= 0) {
                while (start >= 0) {
                    uint32_t naluStart = static_cast<uint32_t>(start) + scLen;  // 有效NALU数据的起始位置
                    uint32_t nextScLen = 0;
                    // 找下一个起始码 返回下一个起始码的开始位置 也就是当前NALU数据的结束位置
                    int next = findAnnexbStartCode(payload, payloadLen, naluStart, &nextScLen);  
                    uint32_t naluEnd = next >= 0 ? static_cast<uint32_t>(next) : payloadLen;  // 当前NALU数据的结束位置

                    if (naluEnd > naluStart) {
                        /**
                         * 从NALU头中获取类型信息
                         * nalType = 7 -> SPS
                         * nalType = 8 -> PPS
                         * nalType = 5 -> IDR
                         * nalType = 1 -> 非 IDR slice
                         */
                        uint8_t nalType = payload[naluStart] & 0x1F;  // 提取低5位为类型信息

                        if (nalType == 7 && m_sps.isEmpty()) {        // SPS
                            spsLen = naluEnd - naluStart;             // SPS数据段长度
                            if (spsLen > sizeof(spsBuf)) spsLen = sizeof(spsBuf);  // 防止越界 实测SPS数据占27个字节 所以这里无影响
                            // printf("spsLen = %d\r\n", spsLen);
                            memcpy(spsBuf, payload + naluStart, spsLen);  // 将SPS数据拷贝进缓冲区spsBuf
                            m_sps = QByteArray(reinterpret_cast<const char *>(spsBuf), static_cast<int>(spsLen));  // 写入上下文SPS变量

                        } else if (nalType == 8 && m_pps.isEmpty()) {  // PPS
                            ppsLen = naluEnd - naluStart;              // PPS数据段长度
                            if (ppsLen > sizeof(ppsBuf)) ppsLen = sizeof(ppsBuf);  // 防止越界 实测PPS数据占5个字节 所以这里无影响
                            // printf("ppsLen = %d\r\n", ppsLen);
                            memcpy(ppsBuf, payload + naluStart, ppsLen);  // 将PPS数据拷贝进缓冲区ppsBuf
                            m_pps = QByteArray(reinterpret_cast<const char *>(ppsBuf), static_cast<int>(ppsLen));  // 写入上下文PPS变量

                        } else if (nalType == 1 || nalType == 5) {    // IDR 或者 非IDR slice
                            hasVideoSlice = true;
                        }
                    }
                    start = next;
                    scLen = nextScLen;
                }
            } else {
                hasVideoSlice = true;
            }
        }

        // 用提取出SPS和PPS初始化 MP4 视频轨并写容器头
        if (!m_headerWritten && !m_sps.isEmpty() && !m_pps.isEmpty()) {
            if (addSpsPpsAndWriteHeader() < 0) {  // 写容器头
                return -1;
            }
        }

        if (m_videoStreamIndex < 0 || !m_headerWritten || !hasVideoSlice) {
            continue;
        }

        ensureSyncBase(pack->u64PTS);
        if (!m_videoPtsInited) {
            m_videoPtsInited = true;
            m_videoPtsBaseUs = m_syncBaseUs;
        }
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.flags = (pack->DataType.enH264EType == H264E_NALU_IDRSLICE ||
                     pack->DataType.enH264EType == H264E_NALU_ISLICE) ? AV_PKT_FLAG_KEY : 0;
        AVStream *vst = m_outFmt->streams[m_videoStreamIndex];
        pkt.stream_index = vst->index;
        // 视频时间戳与音频共享同一个同步基准 m_syncBaseUs。
        int64_t deltaUs = static_cast<int64_t>(pack->u64PTS - m_videoPtsBaseUs);
        if (deltaUs < 0) deltaUs = 0;
        pkt.pos = -1;
        QByteArray avccPacket;
        if (!annexbToAvcc(payload, payloadLen, avccPacket)) {
            continue;
        }
        if (av_new_packet(&pkt, avccPacket.size()) < 0) {
            std::cerr << "av_new_packet failed, len=" << payloadLen << std::endl;
            return -1;
        }
        memcpy(pkt.data, avccPacket.constData(), static_cast<size_t>(avccPacket.size()));
        pkt.size = avccPacket.size();
        pkt.stream_index = m_videoStreamIndex;
        pkt.pts = deltaUs;
        pkt.dts = deltaUs;
        pkt.duration = 33333;

        int ret = av_interleaved_write_frame(m_outFmt, &pkt);
        av_packet_unref(&pkt);
        if (ret < 0) {
            std::cerr << "av_interleaved_write_frame(video) failed: "
                      << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
            return -1;
        }
    }
    return 0;
}
