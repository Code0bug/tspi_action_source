#include "mp4demuxer.h"

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

Mp4Demuxer::Mp4Demuxer(QObject *parent) : QObject(parent)
{

}

int Mp4Demuxer::send_stream_data(const uint8_t *src, RK_U32 len, bool eos, bool endOfFrame, uint64_t ptsUs)
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

    int failCount = 0;
    while (!stop_send_frame) {
        s32Ret = RK_MPI_VDEC_SendStream(0, &stStream, 20);
        if (s32Ret >= 0) {
            break;
        }
        if (++failCount > 50) {
            printf("send stream failed %d times, giving up! 0x%08X\n", failCount, s32Ret);
            break;
        }
        usleep(1000llu);
    }

    RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
    return s32Ret;
}

int Mp4Demuxer::do_send_mp4_frame(const QString &path)
{
    AVFormatContext *inFmt = nullptr;  // FFmpeg 解封装上下文，持有 MP4 文件的全部信息（流、编解码器、时长等）
    AVBSFContext *bsfCtx = nullptr;    // Bitstream Filter 上下文，用于 H.264 AVCC → Annex-B 转换
    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    int ret = 0;

    // ---- 第一步：打开 MP4 容器 ----
    // avformat_open_input 读取文件头，识别容器格式，填充 inFmt。
    // 不会解码任何帧，只是解析容器结构。
    ret = avformat_open_input(&inFmt, path.toUtf8().constData(), nullptr, nullptr);
    if (ret < 0) {
        std::cerr << "avformat_open_input failed: " << ffErrStr(ret)
                  << " path=" << path.toStdString() << std::endl;
        return ret;
    }

    // avformat_find_stream_info 读取少量 packet 来探测每个流的详细信息
    //（编码参数、帧率、时长等）。对 MP4 来说通常很快，因为 moov atom 里已有元数据。
    ret = avformat_find_stream_info(inFmt, nullptr);
    if (ret < 0) {
        std::cerr << "avformat_find_stream_info failed: " << ffErrStr(ret)
                  << " (ret=" << ret << ")" << std::endl;
        avformat_close_input(&inFmt);
        return ret;
    }

    // ---- 第二步：查找视频流和音频流 ----
    // av_find_best_stream 按优先级找到指定类型的"最佳"流。
    // 视频流必须存在，音频流可能没有（返回负值，不报错）。
    ret = av_find_best_stream(inFmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (ret < 0) {
        std::cerr << "av_find_best_stream(video) failed: " << ffErrStr(ret)
                  << " (ret=" << ret << ")" << std::endl;
        avformat_close_input(&inFmt);
        return ret;
    }
    videoStreamIndex = ret;
    audioStreamIndex = av_find_best_stream(inFmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    // 当前 VDEC 只支持 H.264，其他编码直接拒绝。
    // codecpar 存储的是 MP4 脱壳后的编解码参数（不含压缩数据）。
    AVStream *videoStream = inFmt->streams[videoStreamIndex];
    if (videoStream->codecpar->codec_id != AV_CODEC_ID_H264) {
        std::cerr << "do_send_mp4_frame: unsupported video codec id="
                  << videoStream->codecpar->codec_id << ", only H.264 is supported" << std::endl;
        avformat_close_input(&inFmt);
        return AVERROR(EINVAL);
    }

    // ---- 第三步：创建 H.264 BSF（Bitstream Filter）----
    // MP4 中的 H.264 使用 AVCC 格式：每个 NAL 前面是 4 字节长度前缀。
    // RK VDEC 需要 Annex-B 格式：每个 NAL 前面是起始码 00 00 00 01。
    // "h264_mp4toannexb" 是 FFmpeg 内置的 BSF，完成这个格式转换，
    // 同时会在流开始时注入 SPS/PPS（从 extradata 中读取）。
    const AVBitStreamFilter *bsf = av_bsf_get_by_name("h264_mp4toannexb");
    if (!bsf) {
        std::cerr << "av_bsf_get_by_name(h264_mp4toannexb) failed" << std::endl;
        avformat_close_input(&inFmt);
        return AVERROR_BSF_NOT_FOUND;
    }

    // av_bsf_alloc：根据 BSF 定义分配上下文。
    // 还需要把视频流的编解码参数复制进去（par_in），然后 av_bsf_init 初始化。
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
    bsfCtx->time_base_in = videoStream->time_base;  // BSF 需要知道输入的时间基来转换 PTS

    ret = av_bsf_init(bsfCtx);
    if (ret < 0) {
        std::cerr << "av_bsf_init failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        av_bsf_free(&bsfCtx);
        avformat_close_input(&inFmt);
        return ret;
    }

    // ---- 第四步：通知 AudioManager 音频流信息 ----
    // 如果 MP4 有音频流，把编解码参数发给 AudioManager，让它初始化 FFmpeg 音频解码器和 AO 硬件。
    // extradata 对于 AAC 是 AudioSpecificConfig（采样率、声道等），解码器初始化必需。
    if (audioStreamIndex >= 0) {
        AVCodecParameters *apar = inFmt->streams[audioStreamIndex]->codecpar;
        QByteArray extradata;
        if (apar->extradata && apar->extradata_size > 0) {
            extradata = QByteArray(reinterpret_cast<const char *>(apar->extradata), apar->extradata_size);
        }
        emit audioStreamStarted(static_cast<int>(apar->codec_id), apar->sample_rate, apar->channels, extradata);
    }

    // ---- 第五步：逐 packet 读取并分发 ----
    // av_read_frame 从容器中读取一个压缩数据包（packet）。
    // 每个 packet 属于某条流（视频或音频），通过 pkt.stream_index 区分。
    // 注意：av_read_frame 返回的视频 packet 是 AVCC 格式，还需要经过 BSF 转换。
    AVPacket pkt;
    av_init_packet(&pkt);
    int64_t firstMediaUs = AV_NOPTS_VALUE;  // 第一个有效 packet 的媒体时间（微秒）
    int64_t playStartUs = av_gettime_relative();  // 播放起始的墙钟时间（微秒）
    while (!stop_send_frame && (ret = av_read_frame(inFmt, &pkt)) >= 0) {
        // ---- 墙钟同步：控制发包节奏 ----
        // 用 packet 的 PTS 计算"理想发送时间"，和当前墙钟比较，
        // 如果还没到就 sleep，避免瞬间把所有 packet 推给下游。
        AVStream *pktStream = inFmt->streams[pkt.stream_index];
        int64_t pktMediaUs = packetTimeUs(pkt, pktStream);  // 转成微秒
        if (pktMediaUs != AV_NOPTS_VALUE) {
            if (firstMediaUs == AV_NOPTS_VALUE) {
                firstMediaUs = pktMediaUs;        // 记录第一个 packet 的时间作为基准
                playStartUs = av_gettime_relative(); // 同时记录墙钟起点
            }
            // 目标发送时间 = 墙钟起点 + (当前 packet PTS - 第一个 packet PTS)
            int64_t targetUs = playStartUs + std::max<int64_t>(0, pktMediaUs - firstMediaUs);
            while (!stop_send_frame) {
                int64_t delayUs = targetUs - av_gettime_relative();
                if (delayUs <= 0) {
                    break;  // 时间到了，发包
                }
                usleep(static_cast<useconds_t>(std::min<int64_t>(delayUs, 10000)));  // 最多 sleep 10ms
            }
        }

        // ---- 音频 packet：直接发给 AudioManager 解码播放 ----
        // 音频 packet 不需要 BSF 转换（AAC 在 MP4 中已经是解码器能直接吃的格式）。
        // AudioManager::playEncodedAudioPacket 会用 FFmpeg 解码 + swr_convert 转 PCM 再送 AO。
        if (pkt.stream_index == audioStreamIndex) {
            if (pkt.data && pkt.size > 0) {
                emit audioPacketReady(QByteArray(reinterpret_cast<const char *>(pkt.data), pkt.size),
                                      static_cast<qint64>(pkt.pts));
            }
            av_packet_unref(&pkt);  // 释放 packet 引用（data 指针作废，但 QByteArray 已拷贝）
            continue;
        }

        // 跳过既不是视频也不是音频的 packet（如字幕流）
        if (pkt.stream_index != videoStreamIndex) {
            av_packet_unref(&pkt);
            continue;
        }

        // ---- 视频 packet：经过 BSF 转换后送 VDEC ----
        // av_bsf_send_packet 把 AVCC 格式的 packet 送入 BSF。
        // BSF 内部会把 4 字节长度前缀替换成 00 00 00 01 起始码，
        // 并在流开始时自动注入 SPS/PPS（从 extradata 解析）。
        ret = av_bsf_send_packet(bsfCtx, &pkt);
        av_packet_unref(&pkt);  // 原始 AVCC packet 不再需要
        if (ret < 0) {
            std::cerr << "av_bsf_send_packet failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
            break;
        }

        // av_bsf_receive_packet 从 BSF 取出转换后的 Annex-B packet。
        // 一个输入 packet 可能产生多个输出 packet（如 SPS + PPS + IDR），
        // 所以要用 while 循环把所有输出都取干净。
        while (!stop_send_frame && (ret = av_bsf_receive_packet(bsfCtx, &pkt)) == 0) {
            int64_t videoPtsUs = packetTimeUs(pkt, videoStream);
            send_stream_data(pkt.data, pkt.size, false, true,
                             videoPtsUs == AV_NOPTS_VALUE ? 0 : static_cast<uint64_t>(videoPtsUs));
            av_packet_unref(&pkt);
        }
        // EAGAIN 表示 BSF 需要更多输入，不是错误；EOF 表示流结束。
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            ret = 0;
        }
        if (ret < 0) {
            break;
        }
    }

    // ---- 第六步：发送 EOS 并清理 ----
    if (ret == AVERROR_EOF) {
        ret = 0;  // 正常结束，不是错误
    }
    if (!stop_send_frame) {
        send_stream_data(nullptr, 0, true, true);  // 发送 EOS 标记，通知 VDEC 解码结束
    }
    if (audioStreamIndex >= 0) {
        emit audioStreamFinished();  // 通知 AudioManager 停止播放
    }

    av_bsf_free(&bsfCtx);           // 释放 BSF 上下文
    avformat_close_input(&inFmt);    // 关闭容器并释放 inFmt（设为 nullptr 后自动释放）
    return ret;
}

void Mp4Demuxer::do_send_frame(QString path)
{
    RK_S32 s32ReachEOS = 0;

    stop_send_frame = false;

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


void Mp4Demuxer::do_stop_send_frame()
{
    stop_send_frame = true;
}











