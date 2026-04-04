#include "savevideo.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/mathematics.h>
#include <libavutil/error.h>
}
#include <iostream>
#include <string>
#include <cstring>
#include <time.h>

namespace {
std::string ffErrStr(int errnum)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, errbuf, sizeof(errbuf));
    return std::string(errbuf);
}

uint64_t nowUsMonotonic()
{
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL +
           static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
}

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

bool annexbToAvcc(const uint8_t *data, uint32_t size, QByteArray &out)
{
    out.clear();
    if (!data || size == 0) {
        return false;
    }

    auto findStartCode = [&](uint32_t from, uint32_t *scLen) -> int {
        for (uint32_t p = from; p + 3 < size; ++p) {
            if (data[p] != 0x00 || data[p + 1] != 0x00) continue;
            if (data[p + 2] == 0x01) { *scLen = 3; return static_cast<int>(p); }
            if (p + 4 < size && data[p + 2] == 0x00 && data[p + 3] == 0x01) { *scLen = 4; return static_cast<int>(p); }
        }
        return -1;
    };

    uint32_t scLen = 0;
    int start = findStartCode(0, &scLen);
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
        int next = findStartCode(naluStart, &nextScLen);
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


SaveVideo::SaveVideo()
{

    // RK_S32 s32Ret = 0;

    stFrame.pstPack = reinterpret_cast<VENC_PACK_S *>(malloc(sizeof(VENC_PACK_S)));

    data = new unsigned char;
    // 内存缓存池配置
    MB_POOL_CONFIG_S PoolCfg;
    memset(&PoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
    PoolCfg.u64MBSize = encWidth * encHeight * 3 ;     // 缓存块大小
    PoolCfg.u32MBCnt = 1;                              // 内存缓存池中缓存块个数
    PoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;           // 申请内存类型
    //PoolCfg.bPreAlloc = RK_FALSE;
    MB_POOL src_Pool = RK_MPI_MB_CreatePool(&PoolCfg); // 创建内存缓存池
    printf("Create Pool success !\n");

    // 从内存缓存池中获取一个缓存块，返回该缓存块的地址
    MB_BLK src_Blk = RK_MPI_MB_GetMB(src_Pool, encWidth * encHeight * 3, RK_TRUE);

    // 配置编码帧信息
    h264_frame.stVFrame.u32Width = encWidth;
    h264_frame.stVFrame.u32Height = encHeight;
    h264_frame.stVFrame.u32VirWidth = encWidth;
    h264_frame.stVFrame.u32VirHeight = encHeight;
    h264_frame.stVFrame.enPixelFormat =  RK_FMT_RGB888;
    h264_frame.stVFrame.u32FrameFlag = 160;
    h264_frame.stVFrame.pMbBlk = src_Blk;   // 将帧地址关联到缓存块
    // 将缓存块地址转为虚拟地址，并创建Mat类型变量指向该地址
    data = (unsigned char *)RK_MPI_MB_Handle2VirAddr(src_Blk);


    // rkmpi init
    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
        RK_LOGE("rk mpi sys init fail!");
        // return -1;
    }

    // venc init
    RK_CODEC_ID_E enCodecType = RK_VIDEO_ID_AVC;
    venc_init(0, encWidth, encHeight, enCodecType);

}

SaveVideo::~SaveVideo()
{
    closeMp4Mux();
    if (stFrame.pstPack) {
        free(stFrame.pstPack);
        stFrame.pstPack = nullptr;
    }
}

bool SaveVideo::initMp4Mux(const QString filename)
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
    m_firstSpsPpsFound = 0;
    m_videoPtsInited = false;
    m_videoPtsBaseUs = 0;
    m_sps.clear();
    m_pps.clear();

    return true;
}

void SaveVideo::closeMp4Mux()
{
    if (!m_outFmt) {
        return;
    }
    if (m_headerWritten && m_videoStreamIndex >= 0 && m_outFmt->pb) {
        printf("av_write_trailer\r\n");
        int ret = av_write_trailer(m_outFmt);
        if (ret < 0) {
            std::cerr << "av_write_trailer failed: "
                      << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        }
        printf("av_write_trailer finish\r\n");
    }
    if (!(m_outFmt->oformat->flags & AVFMT_NOFILE) && m_outFmt->pb) {
        printf("avio_closep\r\n");
        avio_closep(&m_outFmt->pb);
    }
    avformat_free_context(m_outFmt);
    m_outFmt = nullptr;
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_headerWritten = false;
    m_firstSpsPpsFound = 0;
    m_videoPtsInited = false;
    m_videoPtsBaseUs = 0;
    m_sps.clear();
    m_pps.clear();
}

int SaveVideo::addMp4Stream()
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
    vst->time_base = AVRational{1, 1000000};

    AVCodecParameters *vpar = vst->codecpar;
    vpar->codec_type = AVMEDIA_TYPE_VIDEO;
    vpar->codec_id = AV_CODEC_ID_H264;
    vpar->bit_rate = 4 * 1000 * 1000;
    vpar->width = encWidth;
    vpar->height = encHeight;
    vpar->format = AV_PIX_FMT_YUV420P;

    m_videoStreamIndex = vst->index;
    return 0;
}

int SaveVideo::addSpsPpsAndWriteHeader(uint8_t *buf, uint32_t size)
{
    if (!m_outFmt || !buf || size == 0) {
        return -1;
    }
    if (addMp4Stream() < 0) {
        return -1;
    }
    if (m_videoStreamIndex < 0) {
        return -1;
    }

    AVStream *vst = m_outFmt->streams[m_videoStreamIndex];
    AVCodecParameters *par = vst->codecpar;

    QByteArray sps(reinterpret_cast<const char *>(buf), static_cast<int>(m_sps.size()));
    QByteArray pps(reinterpret_cast<const char *>(buf + m_sps.size()), static_cast<int>(m_pps.size()));
    QByteArray avcc = buildAvccFromSpsPps(sps, pps);
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

    int ret = avformat_write_header(m_outFmt, nullptr);
    if (ret < 0) {
        std::cerr << "avformat_write_header failed: "
                  << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return -1;
    }
    m_headerWritten = true;
    return 0;
}

int SaveVideo::writeVideoToMp4(VENC_STREAM_S *pstStream)
{
    if (!m_outFmt || !pstStream || pstStream->u32PackCount == 0 || !pstStream->pstPack) {
        return 0;
    }

    // printf("pstStream->u32PackCount = %d\r\n", pstStream->u32PackCount);

    uint8_t spsBuf[128] = {0};
    uint8_t ppsBuf[128] = {0};
    uint8_t spsPpsBuf[256] = {0};
    uint32_t spsLen = 0;
    uint32_t ppsLen = 0;

    for (uint32_t i = 0; i < pstStream->u32PackCount; ++i) {
        VENC_PACK_S *pack = &pstStream->pstPack[i];
        uint8_t *packBase = reinterpret_cast<uint8_t *>(RK_MPI_MB_Handle2VirAddr(pack->pMbBlk));
        if (!packBase || pack->u32Offset > pack->u32Len) {
            continue;
        }
        uint8_t *payload = packBase + pack->u32Offset;
        uint32_t payloadLen = pack->u32Len - pack->u32Offset;
        if (payloadLen == 0) {
            continue;
        }

        bool foundSpsInPayload = false;
        bool foundPpsInPayload = false;
        bool hasVideoSlice = false;

        auto findStartCode = [&](uint32_t from, uint32_t *scLen) -> int {
            for (uint32_t p = from; p + 3 < payloadLen; ++p) {
                if (payload[p] != 0x00 || payload[p + 1] != 0x00) {
                    continue;
                }
                if (payload[p + 2] == 0x01) {
                    *scLen = 3;
                    return static_cast<int>(p);
                }
                if (p + 4 < payloadLen && payload[p + 2] == 0x00 && payload[p + 3] == 0x01) {
                    *scLen = 4;
                    return static_cast<int>(p);
                }
            }
            return -1;
        };

        uint32_t scLen = 0;
        int start = findStartCode(0, &scLen);
        if (start >= 0) {
            while (start >= 0) {
                uint32_t naluStart = static_cast<uint32_t>(start) + scLen;
                uint32_t nextScLen = 0;
                int next = findStartCode(naluStart, &nextScLen);
                uint32_t naluEnd = next >= 0 ? static_cast<uint32_t>(next) : payloadLen;
                if (naluEnd > naluStart) {
                    uint8_t nalType = payload[naluStart] & 0x1F;
                    if (nalType == 7 && m_sps.isEmpty()) {
                        spsLen = naluEnd - naluStart;
                        if (spsLen > sizeof(spsBuf)) spsLen = sizeof(spsBuf);
                        memcpy(spsBuf, payload + naluStart, spsLen);
                        m_sps = QByteArray(reinterpret_cast<const char *>(spsBuf), static_cast<int>(spsLen));
                        foundSpsInPayload = true;
                    } else if (nalType == 8 && m_pps.isEmpty()) {
                        ppsLen = naluEnd - naluStart;
                        if (ppsLen > sizeof(ppsBuf)) ppsLen = sizeof(ppsBuf);
                        memcpy(ppsBuf, payload + naluStart, ppsLen);
                        m_pps = QByteArray(reinterpret_cast<const char *>(ppsBuf), static_cast<int>(ppsLen));
                        foundPpsInPayload = true;
                    } else if (nalType == 1 || nalType == 5) {
                        hasVideoSlice = true;
                    }
                }
                start = next;
                scLen = nextScLen;
            }
        } else {
            hasVideoSlice = true;
        }

        if ((foundSpsInPayload || foundPpsInPayload) && !m_sps.isEmpty() && !m_pps.isEmpty() && !m_headerWritten) {
            spsLen = static_cast<uint32_t>(m_sps.size());
            ppsLen = static_cast<uint32_t>(m_pps.size());
            if (spsLen + ppsLen > sizeof(spsPpsBuf)) {
                return -1;
            }
            memcpy(spsPpsBuf, m_sps.constData(), spsLen);
            memcpy(spsPpsBuf + spsLen, m_pps.constData(), ppsLen);
            m_firstSpsPpsFound = 2;
            if (addSpsPpsAndWriteHeader(reinterpret_cast<uint8_t *>(spsPpsBuf), spsLen + ppsLen) < 0) {
                return -1;
            }
        }

        if (m_firstSpsPpsFound != 2 || m_videoStreamIndex < 0 || !m_headerWritten || !hasVideoSlice) {
            continue;
        }

        if (!m_videoPtsInited) {
            m_videoPtsInited = true;
            m_videoPtsBaseUs = pack->u64PTS;
        }
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.flags = (pack->DataType.enH264EType == H264E_NALU_IDRSLICE ||
                     pack->DataType.enH264EType == H264E_NALU_ISLICE) ? AV_PKT_FLAG_KEY : 0;
        AVStream *vst = m_outFmt->streams[m_videoStreamIndex];
        pkt.stream_index = vst->index;
        int64_t deltaUs = static_cast<int64_t>(pack->u64PTS - m_videoPtsBaseUs);
        printf("deltaUs = %ld\r\n", deltaUs);
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
        pkt.duration = 33333; // 30fps in us timebase

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

void SaveVideo::do_save_video(cv::Mat buf, FILE* fp)
{
//    printf("start video===\n");

    memcpy(data, buf.data, encWidth * encHeight * 3);
    
    /**************************
     * 向VENC发送原始图像进行编码
     * 0为编码通道号
     * h264_frame 为原始图像信息
     * -1 表示阻塞，发送成功后释放
     **************************/
    h264_frame.stVFrame.u64PTS = nowUsMonotonic();  // 更新时间戳
    RK_MPI_VENC_SendFrame(0, &h264_frame, -1);

    /**************************
     * 获取编码码流
     * 0为编码通道号
     * stFrame为码流结构体指针
     * -1 表示阻塞，获取编码流后释放
     **************************/
    RK_S32 s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, -1);

    if(s32Ret == RK_SUCCESS) {

        uint8_t *pData = reinterpret_cast<uint8_t *>(RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk)); // 编码帧数据
        uint32_t len = stFrame.pstPack->u32Len;
        writeVideoToMp4(&stFrame);

        // 保存录像
        if (fp) {
            size_t written = fwrite(pData, 1, len, fp);
            if (written != len) {
                fprintf(stderr, "fwrite failed, written=%zu, len=%u\n", written, len);
            }
            fflush(fp); // 保证写入磁盘

            int fd = fileno(fp);      // 获取文件描述符
            fsync(fd);                // 强制刷到 eMMC
        }

        // QByteArray packet(payload, static_cast<int>(len));
        // emit sigVideoEncoded(packet, static_cast<uint64_t>(stFrame.pstPack->u64PTS), keyFrame);
    }

    s32Ret = RK_MPI_VENC_ReleaseStream(0, &stFrame);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("RK_MPI_VENC_ReleaseStream fail %x", s32Ret);
    }

}
