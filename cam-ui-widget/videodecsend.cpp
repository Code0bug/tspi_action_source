#include "videodecsend.h"

#include <QFileInfo>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <unistd.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/time.h>
}

namespace {
std::string ffErrStr(int errnum)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, errbuf, sizeof(errbuf));
    return std::string(errbuf);
}

int64_t packetTimeUs(const AVPacket &pkt, AVStream *stream)
{
    int64_t ts = pkt.pts;
    if (ts == AV_NOPTS_VALUE) {
        ts = pkt.dts;
    }
    if (ts == AV_NOPTS_VALUE || !stream) {
        return AV_NOPTS_VALUE;
    }
    return av_rescale_q(ts, stream->time_base, AVRational{1, 1000000});
}
}

static RK_S32 mpi_vdec_free(void *opaque) {
    if (opaque)
        free(opaque);
    return 0;
}

VideoDecSend::VideoDecSend(QObject *parent) : QObject(parent)
{

}

int VideoDecSend::send_stream_data(const uint8_t *src, RK_U32 len, bool eos, bool endOfFrame, uint64_t ptsUs)
{
    MB_EXT_CONFIG_S pstMbExtConfig;
    MB_BLK buffer = RK_NULL;
    VDEC_STREAM_S stStream;
    const RK_U32 allocLen = std::max<RK_U32>(len, 1);
    RK_U8 *data = reinterpret_cast<RK_U8 *>(calloc(allocLen, sizeof(RK_U8)));
    if (!data) {
        return -1;
    }
    if (src && len > 0) {
        memcpy(data, src, len);
    }

    memset(&pstMbExtConfig, 0, sizeof(MB_EXT_CONFIG_S));
    pstMbExtConfig.pFreeCB = mpi_vdec_free;
    pstMbExtConfig.pOpaque = data;
    pstMbExtConfig.pu8VirAddr = data;
    pstMbExtConfig.u64Size = len;

    RK_S32 s32Ret = RK_MPI_SYS_CreateMB(&buffer, &pstMbExtConfig);
    if (s32Ret != RK_SUCCESS) {
        free(data);
        printf("create mb failed! 0x%08X\n", s32Ret);
        return s32Ret;
    }

    memset(&stStream, 0, sizeof(VDEC_STREAM_S));
    stStream.u64PTS = ptsUs;
    stStream.pMbBlk = buffer;
    stStream.u32Len = len;
    stStream.bEndOfStream = eos ? RK_TRUE : RK_FALSE;
    stStream.bEndOfFrame = endOfFrame ? RK_TRUE : RK_FALSE;
    stStream.bBypassMbBlk = RK_TRUE;

    while (!stop_send_frame) {
        s32Ret = RK_MPI_VDEC_SendStream(0, &stStream, 20);
        if (s32Ret >= 0) {
            printf("send stream success!\n");
            break;
        }
        printf("send stream failed! 0x%08X\n", s32Ret);
        usleep(1000llu);
    }

    RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
    return s32Ret;
}

int VideoDecSend::do_send_mp4_frame(const QString &path)
{
    AVFormatContext *inFmt = nullptr;
    AVBSFContext *bsfCtx = nullptr;
    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    int ret = 0;

    ret = avformat_open_input(&inFmt, path.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        std::cerr << "avformat_open_input failed: " << ffErrStr(ret)
                  << " path=" << path.toStdString() << std::endl;
        return ret;
    }

    ret = avformat_find_stream_info(inFmt, nullptr);
    if (ret < 0) {
        std::cerr << "avformat_find_stream_info failed: " << ffErrStr(ret)
                  << " (ret=" << ret << ")" << std::endl;
        avformat_close_input(&inFmt);
        return ret;
    }

    ret = av_find_best_stream(inFmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ret < 0) {
        std::cerr << "av_find_best_stream(video) failed: " << ffErrStr(ret)
                  << " (ret=" << ret << ")" << std::endl;
        avformat_close_input(&inFmt);
        return ret;
    }
    videoStreamIndex = ret;
    audioStreamIndex = av_find_best_stream(inFmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    AVStream *videoStream = inFmt->streams[videoStreamIndex];
    if (videoStream->codecpar->codec_id != AV_CODEC_ID_H264) {
        std::cerr << "do_send_mp4_frame: unsupported video codec id="
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

    if (audioStreamIndex >= 0) {
        AVCodecParameters *apar = inFmt->streams[audioStreamIndex]->codecpar;
        QByteArray extradata;
        if (apar->extradata && apar->extradata_size > 0) {
            extradata = QByteArray(reinterpret_cast<const char *>(apar->extradata), apar->extradata_size);
        }
        emit audioStreamStarted(static_cast<int>(apar->codec_id), apar->sample_rate, apar->channels, extradata);
    }

    AVPacket pkt;
    av_init_packet(&pkt);
    int64_t firstMediaUs = AV_NOPTS_VALUE;
    int64_t playStartUs = av_gettime_relative();
    while (!stop_send_frame && (ret = av_read_frame(inFmt, &pkt)) >= 0) {
        AVStream *pktStream = inFmt->streams[pkt.stream_index];
        int64_t pktMediaUs = packetTimeUs(pkt, pktStream);
        if (pktMediaUs != AV_NOPTS_VALUE) {
            if (firstMediaUs == AV_NOPTS_VALUE) {
                firstMediaUs = pktMediaUs;
                playStartUs = av_gettime_relative();
            }
            int64_t targetUs = playStartUs + std::max<int64_t>(0, pktMediaUs - firstMediaUs);
            while (!stop_send_frame) {
                int64_t delayUs = targetUs - av_gettime_relative();
                if (delayUs <= 0) {
                    break;
                }
                usleep(static_cast<useconds_t>(std::min<int64_t>(delayUs, 10000)));
            }
        }

        if (pkt.stream_index == audioStreamIndex) {
            if (pkt.data && pkt.size > 0) {
                emit audioPacketReady(QByteArray(reinterpret_cast<const char *>(pkt.data), pkt.size),
                                      static_cast<qint64>(pkt.pts));
            }
            av_packet_unref(&pkt);
            continue;
        }

        if (pkt.stream_index != videoStreamIndex) {
            av_packet_unref(&pkt);
            continue;
        }

        ret = av_bsf_send_packet(bsfCtx, &pkt);
        av_packet_unref(&pkt);
        if (ret < 0) {
            std::cerr << "av_bsf_send_packet failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
            break;
        }

        while (!stop_send_frame && (ret = av_bsf_receive_packet(bsfCtx, &pkt)) == 0) {
            int64_t videoPtsUs = packetTimeUs(pkt, videoStream);
            send_stream_data(pkt.data, pkt.size, false, true,
                             videoPtsUs == AV_NOPTS_VALUE ? 0 : static_cast<uint64_t>(videoPtsUs));
            av_packet_unref(&pkt);
        }
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            ret = 0;
        }
        if (ret < 0) {
            break;
        }
    }

    if (ret == AVERROR_EOF) {
        ret = 0;
    }
    if (!stop_send_frame) {
        send_stream_data(nullptr, 0, true, true);
    }
    if (audioStreamIndex >= 0) {
        emit audioStreamFinished();
    }

    av_bsf_free(&bsfCtx);
    avformat_close_input(&inFmt);
    return ret;
}

void VideoDecSend::do_send_frame(QString path)
{
    RK_S32 s32ReachEOS = 0;

    stop_send_frame = 0;

    QFileInfo inputInfo(path);
    if (inputInfo.suffix().compare("mp4", Qt::CaseInsensitive) == 0) {
        if (do_send_mp4_frame(path) < 0) {
            printf("demux mp4 to h264 failed: %s\n", path.toStdString().c_str());
        }
        return;
    }

    FILE* fp = fopen(path.toStdString().c_str(), "rb");
    if (fp == RK_NULL) {
        printf("fopen failed!\n");
        return;
    }

    while(!s32ReachEOS && !stop_send_frame) {
        RK_U8 *data = reinterpret_cast<RK_U8 *>(calloc(65536, sizeof(RK_U8)));
        if (!data) {
            break;
        }
        RK_S32 s32Size = fread(data, 1, 65536, fp);  // 一次读取65536个字节
        printf("s32Size = %d\n", s32Size);  // 实际读取到的字节数
        if (s32Size <= 0) {
            s32ReachEOS = 1; // 表示文件读完了
        }

        send_stream_data(data, s32Size, s32ReachEOS, s32ReachEOS);
        free(data);

    }

    fclose(fp);

}


void VideoDecSend::do_stop_send_frame()
{
    stop_send_frame = 1;
}











