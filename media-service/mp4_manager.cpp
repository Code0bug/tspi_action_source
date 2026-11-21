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
 *
 * 2. PCM -> AAC -> MP4
 * - 捕获的音频数据是原始PCM，需要封装成AVFrame进行AAC编码，再写入MP4
 * - AAC编码要求一帧为1024个采样点，那么一个AVFrame的数据大小就应该是4096个字节（1024帧 x 2声道 x 2字节）
 * - AAC要求的音频数据格式是32位浮点的fltp类型，而PCM原始数据是s16类型，因此要做转换
 * - 音频的时间戳一般以采样点为单位，也就是1/采样频率，而输入时间戳是微秒单位，因此要做转换
 * - 因此音频封装MP4的步骤为：
 *      ① 初始化音频编码器和编码帧，创建音频流
 *      ② 转换音频格式，转换时间戳
 *      ③ 发送给编码器进行AAC编码，取出编码帧，然后写入MP4帧
 *      ④ 结束写MP4前要进行flush操作，防止还有编码帧没有被写入
 *      ⑤ 结束写MP4，释放相关变量
 */
#include "mp4_manager.h"

#include "media_manager.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <unistd.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

namespace {
constexpr int kEncWidth = 1920;
constexpr int kEncHeight = 1080;
constexpr int kAudioBitrate = 64000; // 语音场景下 AAC 64kbps 一般足够
constexpr int kAudioSampleRate = 16000;
constexpr int kAudioChannels = 2;

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
 * - 失败：空 vector（例如 SPS 太短或 PPS 为空）
 *
 * avcC 结构（本实现只写单个 SPS + 单个 PPS）：
 * - configurationVersion = 0x01
 * - AVCProfileIndication / profile_compatibility / AVCLevelIndication <- 来自 SPS[1..3]
 * - lengthSizeMinusOne = 3（即 NALU 长度前缀 4 字节）
 * - numOfSequenceParameterSets = 1，随后写 SPS 长度和内容
 * - numOfPictureParameterSets  = 1，随后写 PPS 长度和内容
 */
std::vector<uint8_t> buildAvccFromSpsPps(const std::vector<uint8_t> &sps,
                                         const std::vector<uint8_t> &pps)
{
    if (sps.size() < 4 || pps.empty()) {
        return std::vector<uint8_t>();
    }
    std::vector<uint8_t> avcc;
    avcc.reserve(11 + sps.size() + pps.size());
    avcc.push_back(0x01);
    avcc.push_back(sps[1]);
    avcc.push_back(sps[2]);
    avcc.push_back(sps[3]);
    avcc.push_back(0xFF);
    avcc.push_back(0xE1);
    avcc.push_back(static_cast<uint8_t>((sps.size() >> 8) & 0xFF));
    avcc.push_back(static_cast<uint8_t>(sps.size() & 0xFF));
    avcc.insert(avcc.end(), sps.begin(), sps.end());
    avcc.push_back(0x01);
    avcc.push_back(static_cast<uint8_t>((pps.size() >> 8) & 0xFF));
    avcc.push_back(static_cast<uint8_t>(pps.size() & 0xFF));
    avcc.insert(avcc.end(), pps.begin(), pps.end());
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

bool annexbToAvcc(const uint8_t *data, uint32_t size, std::vector<uint8_t> &out)
{
    out.clear();
    if (!data || size == 0) {
        return false;
    }

    uint32_t scLen = 0;
    int start = findAnnexbStartCode(data, size, 0, &scLen);
    if (start < 0) {
        out.resize(static_cast<size_t>(4 + size));
        out[0] = static_cast<uint8_t>((size >> 24) & 0xFF);
        out[1] = static_cast<uint8_t>((size >> 16) & 0xFF);
        out[2] = static_cast<uint8_t>((size >> 8) & 0xFF);
        out[3] = static_cast<uint8_t>(size & 0xFF);
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
            uint8_t lenBytes[4] = {
                static_cast<uint8_t>((naluLen >> 24) & 0xFF),
                static_cast<uint8_t>((naluLen >> 16) & 0xFF),
                static_cast<uint8_t>((naluLen >> 8) & 0xFF),
                static_cast<uint8_t>(naluLen & 0xFF)
            };
            out.insert(out.end(), lenBytes, lenBytes + 4);
            out.insert(out.end(), data + naluStart, data + naluStart + naluLen);
        }
        start = next;
        scLen = nextScLen;
    }
    return !out.empty();
}
} // namespace

Mp4Manager::Mp4Manager(MediaManager &media)
    : media_(media)
    , m_outFmt(nullptr)
    , m_audioEncCtx(nullptr)
    , m_swrCtx(nullptr)
    , m_audioFrame(nullptr)
    , m_videoStreamIndex(-1)
    , m_audioStreamIndex(-1)
    , m_headerWritten(false)
    , m_videoPtsInited(false)
    , m_audioPtsInited(false)
    , m_videoPtsBaseUs(0)
    , m_audioPtsBaseUs(0)
    , m_lastVideoPtsUs(-1)
{
}

Mp4Manager::~Mp4Manager()
{
    closeMp4Mux();
}

void Mp4Manager::startMux()
{
    muxer_thread = std::thread(&Mp4Manager::muxRun, this);
}

void Mp4Manager::stopMux()
{
    MuxJob job = {};
    job.type = MuxJobType::Close;
    media_.mux_queue.push(job);
    muxer_thread.join();
}

void Mp4Manager::stop()
{
    if (muxer_thread.joinable())
        stopMux();
    stopDemux();
}

void Mp4Manager::startDemux(const std::string &path)
{
    stop_demux_ = false;
    // detach：线程退出后自动回收，下次播放可重新创建
    std::thread demuxer_thread(&Mp4Manager::demuxRun, this, path);
    demuxer_thread.detach();
}

void Mp4Manager::stopDemux()
{
    stop_demux_ = true;
}

void Mp4Manager::muxRun()
{
    for (;;) {
        MuxJob job = media_.mux_queue.pop();
        switch (job.type) {
        case MuxJobType::Open:
            /**
             * 每次开启新的录像都要重新初始化编码器，如果初始化一次，只有第一个IDR帧会带SPS/PPS信息
             * 后面再进行第二次录像时会因为找不到SPS/PPS而封装MP4失败（0字节）
             */
            mpp_encoder_init(1920, 1080, 1920);
            initMp4Mux(job.path);
            break;
        case MuxJobType::Video: {
            EncodedPacket pkt;
            if (mpp_encoder_encode(job.video.dmabuf_fd, job.video.length,
                                   (uint64_t)job.video.timestamp_us, &pkt) == 0)
                writeVideoToMp4(&pkt);
            media_.releaseVideoFrame(job.video);
            break;
        }
        case MuxJobType::Audio:
            writeAudioToMp4(job.pcm.data(), (int)job.pcm.size(), job.pts);
            break;
        case MuxJobType::Close:
            mpp_encoder_deinit();
            closeMp4Mux();
            return;
        }
    }
}

void Mp4Manager::demuxRun(std::string path)
{
    if (demuxMp4(path) < 0) {
        printf("demux mp4 failed: %s\n", path.c_str());
    }

    // 无论正常结束、出错还是被打断，都要通知下游线程退出：
    // 视频：向 MPP 发送 EOS，视频线程取到 eos 帧后退出
    // 音频：向队列推送 AudioEnd，音频线程取到后退出（没有音频流时也一样）
    sendVideoStream(nullptr, 0, true);
    DemuxJob job = {};
    job.type = DemuxJobType::AudioEnd;
    media_.demux_queue.push(std::move(job));

    // 播放结束，回到暂停状态（相册）
    media_.statusUpdate(MediaStatus::Paused);
}

int Mp4Manager::sendVideoStream(const uint8_t *src, uint32_t len, bool eos, uint64_t ptsUs)
{
    int s32Ret = 0;
    int failCount = 0;
    // EOS 必须送达，即使已经请求停止
    while (eos || !stop_demux_) {
        s32Ret = mpp_decoder_put_packet(src, len, ptsUs, eos);
        if (s32Ret == 0) {
            break;
        }
        if (++failCount > 50) {
            printf("send stream failed %d times, giving up! %d\n", failCount, s32Ret);
            break;
        }
        usleep(1000llu);
    }
    return s32Ret;
}

int Mp4Manager::demuxMp4(const std::string &path)
{
    AVFormatContext *inFmt = nullptr;  // FFmpeg 解封装上下文，持有 MP4 文件的全部信息（流、编解码器、时长等）
    AVBSFContext *bsfCtx = nullptr;    // Bitstream Filter 上下文，用于 H.264 AVCC → Annex-B 转换
    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    int ret = 0;

    // ---- 第一步：打开 MP4 容器 ----
    // avformat_open_input 读取文件头，识别容器格式，填充 inFmt。
    // 不会解码任何帧，只是解析容器结构。
    ret = avformat_open_input(&inFmt, path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        std::cerr << "avformat_open_input failed: " << ffErrStr(ret)
                  << " path=" << path << std::endl;
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
        std::cerr << "demuxMp4: unsupported video codec id="
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
        DemuxJob audioStart = {};
        audioStart.type = DemuxJobType::AudioStart;
        audioStart.codec_id = static_cast<int>(apar->codec_id);
        audioStart.sample_rate = apar->sample_rate;
        audioStart.channels = apar->channels;
        if (apar->extradata && apar->extradata_size > 0) {
            audioStart.extradata.assign(apar->extradata, apar->extradata + apar->extradata_size);
        }
        media_.demux_queue.push(std::move(audioStart));
    }

    // ---- 第五步：逐 packet 读取并分发 ----
    // av_read_frame 从容器中读取一个压缩数据包（packet）。
    // 每个 packet 属于某条流（视频或音频），通过 pkt.stream_index 区分。
    // 注意：av_read_frame 返回的视频 packet 是 AVCC 格式，还需要经过 BSF 转换。
    AVPacket pkt;
    av_init_packet(&pkt);
    int64_t firstMediaUs = AV_NOPTS_VALUE;  // 第一个有效 packet 的媒体时间（微秒）
    int64_t playStartUs = av_gettime_relative();  // 播放起始的墙钟时间（微秒）
    while (!stop_demux_ && (ret = av_read_frame(inFmt, &pkt)) >= 0) {
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
            while (!stop_demux_) {
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
                DemuxJob audioPacket = {};
                audioPacket.type = DemuxJobType::AudioPacket;
                audioPacket.data.assign(pkt.data, pkt.data + pkt.size);
                audioPacket.pts = static_cast<int64_t>(pkt.pts);
                media_.demux_queue.push(std::move(audioPacket));
            }
            av_packet_unref(&pkt);  // 释放 packet 引用（data 指针作废，但 vector 已拷贝）
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
        while (!stop_demux_ && (ret = av_bsf_receive_packet(bsfCtx, &pkt)) == 0) {
            int64_t videoPtsUs = packetTimeUs(pkt, videoStream);
            sendVideoStream(pkt.data, pkt.size, false,
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
    // EOS 和 AudioEnd 由 demuxRun 统一发送（出错、被打断时同样需要通知下游）
    if (ret == AVERROR_EOF) {
        ret = 0;  // 正常结束，不是错误
    }

    av_bsf_free(&bsfCtx);           // 释放 BSF 上下文
    avformat_close_input(&inFmt);    // 关闭容器并释放 inFmt（设为 nullptr 后自动释放）
    return ret;
}

bool Mp4Manager::initMp4Mux(const std::string &filename)
{
    closeMp4Mux();

    // 创建 MP4 输出上下文（容器级）
    int ret = avformat_alloc_output_context2(&m_outFmt, nullptr, "mp4", filename.c_str());
    if (ret < 0 || !m_outFmt) {
        std::cerr << "avformat_alloc_output_context2 failed: "
                  << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return false;
    }

    // 打开I/O上下文并关联到MP4文件
    if (!(m_outFmt->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_outFmt->pb, filename.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            std::cerr << "avio_open failed: "
                      << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
            avformat_free_context(m_outFmt);
            m_outFmt = nullptr;
            return false;
        }
    }

    // 创建音频编码器和音频流
    if (initAudioEncoder() != 0) {
        closeMp4Mux();
        return false;
    }

    return true;
}

void Mp4Manager::closeMp4Mux()
{
    if (!m_outFmt) {
        return;
    }

    if (m_headerWritten && m_outFmt->pb) {
        writeAudioFrame(nullptr); // flush encoder delayed packets

        int ret = av_write_trailer(m_outFmt); // 写MP4文件尾部
        if (ret < 0) {
            std::cerr << "av_write_trailer failed: "
                      << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        }
    }
    if (!(m_outFmt->oformat->flags & AVFMT_NOFILE) && m_outFmt->pb) {
        avio_closep(&m_outFmt->pb);
    }

    // 释放音频相关资源
    if (m_audioFrame) {
        av_frame_free(&m_audioFrame);
    }
    if (m_swrCtx) {
        swr_free(&m_swrCtx);
    }
    if (m_audioEncCtx) {
        avcodec_free_context(&m_audioEncCtx);
    }

    avformat_free_context(m_outFmt);
    m_outFmt = nullptr;
    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_headerWritten = false;
    m_videoPtsInited = false;
    m_audioPtsInited = false;
    m_videoPtsBaseUs = 0;
    m_audioPtsBaseUs = 0;
    m_lastVideoPtsUs = -1;
    m_sps.clear();
    m_pps.clear();
}

int Mp4Manager::initAudioEncoder()
{
    /**
     * 1. 查找 AAC 编码器，分配并配置编码上下文，avcodec_open2 打开编码器
     * 2. avformat_new_stream 创建音频流，avcodec_parameters_from_context 同步参数
     * 3. swr_alloc_set_opts 初始化重采样器（s16→fltp）
     * 4. av_frame_alloc 分配编码输入帧 buffer
     */
    if (!m_outFmt || m_audioEncCtx) {
        return -1;
    }

    // 查找AAC编码器
    const AVCodec *acodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!acodec) {
        std::cerr << "initAudioEncoder: avcodec_find_encoder(AAC) failed" << std::endl;
        return -1;
    }

    // 分配编码上下文
    m_audioEncCtx = avcodec_alloc_context3(acodec);
    if (!m_audioEncCtx) {
        return -1;
    }

    // 配置编码上下文参数
    m_audioEncCtx->codec_type = AVMEDIA_TYPE_AUDIO;                  // 音频流
    m_audioEncCtx->codec_id = AV_CODEC_ID_AAC;                       // AAC编码
    m_audioEncCtx->sample_rate = kAudioSampleRate;                   // 采样率
    m_audioEncCtx->channel_layout =
        channelLayoutFromChannels(kAudioChannels);                   // 声道布局
    m_audioEncCtx->channels = kAudioChannels;                        // 声道数
    m_audioEncCtx->bit_rate = kAudioBitrate;                         // 比特率
    m_audioEncCtx->time_base = AVRational{1, kAudioSampleRate};      // 时间基
    m_audioEncCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;                  // AAC 编码器只支持 fltp
    if (m_outFmt->oformat->flags & AVFMT_GLOBALHEADER) {
        m_audioEncCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 打开编码器
    int ret = avcodec_open2(m_audioEncCtx, acodec, nullptr);
    if (ret < 0) {
        std::cerr << "avcodec_open2(aac) failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return -1;
    }

    // 创建音频流
    AVStream *ast = avformat_new_stream(m_outFmt, nullptr);
    if (!ast) {
        std::cerr << "avformat_new_stream(audio) failed" << std::endl;
        return -1;
    }
    ast->id = static_cast<int>(m_outFmt->nb_streams - 1);  // 流标识
    ast->time_base = AVRational{1, kAudioSampleRate};  // 时间基

    // 把编码器上下文里的参数（codec_id、sample_rate、channels、extradata 等）复制到流的 codecpar 里
    ret = avcodec_parameters_from_context(ast->codecpar, m_audioEncCtx);
    if (ret < 0) {
        std::cerr << "avcodec_parameters_from_context(audio) failed: "
                  << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return -1;
    }
    m_audioStreamIndex = ast->index;

    // 输入是 AudioCapture 输出的 s16 PCM；AAC 编码器通常是 fltp，需要 swr 做格式转换。
    m_swrCtx = swr_alloc_set_opts(
        nullptr,                              // swr ctx（nullptr 表示新建）
        m_audioEncCtx->channel_layout,        // 输出声道布局（编码器要求的）
        m_audioEncCtx->sample_fmt,            // 输出采样格式（fltp）
        m_audioEncCtx->sample_rate,           // 输出采样率
        AV_CH_LAYOUT_STEREO,                  // 输入声道布局（硬件采集的）
        AV_SAMPLE_FMT_S16,                    // 输入采样格式（s16）
        kAudioSampleRate,                     // 输入采样率
        0,                                    // log offset
        nullptr                               // log context
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

    // 分配编码用的 AVFrame
    m_audioFrame = av_frame_alloc();
    if (!m_audioFrame) {
        return -1;
    }
    // 配置编码帧参数
    m_audioFrame->format = m_audioEncCtx->sample_fmt;
    m_audioFrame->channel_layout = m_audioEncCtx->channel_layout;
    m_audioFrame->sample_rate = m_audioEncCtx->sample_rate;
    m_audioFrame->nb_samples = m_audioEncCtx->frame_size;  // 编码器要求的采样数（AAC为1024）
    ret = av_frame_get_buffer(m_audioFrame, 0);
    if (ret < 0) {
        std::cerr << "av_frame_get_buffer(audio) failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return -1;
    }

    return 0;
}

int Mp4Manager::writeAudioFrame(AVFrame *frame)
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

int Mp4Manager::writeAudioToMp4(const uint8_t *pcmData, int pcmSize, uint64_t captureTsUs)
{
    if (!m_outFmt || !m_audioEncCtx || !pcmData || pcmSize == 0) {
        return 0;
    }

    // 等待 MP4 header 写完
    if (!m_headerWritten) {
        return 0;
    }

    if (!m_audioPtsInited) {
        m_audioPtsInited = true;
        m_audioPtsBaseUs = captureTsUs;
    }
    // 每次都用输入时间戳更新 PTS 基准，映射到编码器 time_base（采样点）
    int64_t deltaUs = static_cast<int64_t>(captureTsUs - m_audioPtsBaseUs);

    // 确保buffer是可写的
    int ret = av_frame_make_writable(m_audioFrame);
    if (ret < 0) {
        std::cerr << "av_frame_make_writable(audio) failed: "
                  << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return -1;
    }

    const uint8_t *srcData[1] = { pcmData };
    ret = swr_convert(  // 音频格式转换 s16 -> fltp
        m_swrCtx,
        m_audioFrame->data,
        m_audioFrame->nb_samples,
        srcData,
        m_audioFrame->nb_samples
    );
    if (ret < 0) {
        std::cerr << "swr_convert failed: " << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return -1;
    }

    // 时间戳需要进行转换
    m_audioFrame->pts = av_rescale_q(deltaUs, AVRational{1, 1000000}, m_audioEncCtx->time_base);

    if (writeAudioFrame(m_audioFrame) < 0) {
        return -1;
    }
    return 0;
}

int Mp4Manager::addMp4Stream()
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
int Mp4Manager::addSpsPpsAndWriteHeader()
{
    if (!m_outFmt || m_sps.empty() || m_pps.empty()) {
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

    std::vector<uint8_t> avcc = buildAvccFromSpsPps(m_sps, m_pps);  // 将SPS和PPS封装为AVCC格式
    if (avcc.empty()) {
        std::cerr << "buildAvccFromSpsPps failed" << std::endl;
        return -1;
    }

    av_freep(&par->extradata);
    par->extradata = reinterpret_cast<uint8_t *>(av_malloc(avcc.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!par->extradata) {
        return -1;
    }
    memcpy(par->extradata, avcc.data(), avcc.size());
    memset(par->extradata + avcc.size(), 0, AV_INPUT_BUFFER_PADDING_SIZE);
    par->extradata_size = static_cast<int>(avcc.size());

    int ret = avformat_write_header(m_outFmt, nullptr); // 写MP4容器头
    if (ret < 0) {
        std::cerr << "avformat_write_header failed: "
                  << ffErrStr(ret) << " (ret=" << ret << ")" << std::endl;
        return -1;
    }
    m_headerWritten = true;

    return 0;
}

int Mp4Manager::writeVideoToMp4(const EncodedPacket *pstStream)
{
    if (!m_outFmt || !pstStream || !pstStream->data || pstStream->size == 0) {
        return 0;
    }

    uint8_t spsBuf[128] = {0};     // 存储sps帧的缓冲区
    uint8_t ppsBuf[128] = {0};     // 存储pps帧的缓冲区
    uint32_t spsLen = 0;
    uint32_t ppsLen = 0;

    const uint8_t *payload = pstStream->data;
    uint32_t payloadLen = pstStream->size;
    uint64_t pts = pstStream->pts;

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

                    if (nalType == 7 && m_sps.empty()) {          // SPS
                        spsLen = naluEnd - naluStart;             // SPS数据段长度
                        if (spsLen > sizeof(spsBuf)) spsLen = sizeof(spsBuf);  // 防止越界 实测SPS数据占27个字节 所以这里无影响
                        memcpy(spsBuf, payload + naluStart, spsLen);  // 将SPS数据拷贝进缓冲区spsBuf
                        m_sps.assign(spsBuf, spsBuf + spsLen);        // 写入上下文SPS变量

                    } else if (nalType == 8 && m_pps.empty()) {   // PPS
                        ppsLen = naluEnd - naluStart;              // PPS数据段长度
                        if (ppsLen > sizeof(ppsBuf)) ppsLen = sizeof(ppsBuf);  // 防止越界 实测PPS数据占5个字节 所以这里无影响
                        memcpy(ppsBuf, payload + naluStart, ppsLen);  // 将PPS数据拷贝进缓冲区ppsBuf
                        m_pps.assign(ppsBuf, ppsBuf + ppsLen);        // 写入上下文PPS变量

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
    if (!m_headerWritten && !m_sps.empty() && !m_pps.empty()) {
        if (addSpsPpsAndWriteHeader() < 0) {  // 写容器头
            return -1;
        }
    }

    if (m_videoStreamIndex < 0 || !m_headerWritten || !hasVideoSlice) {
        return 0;
    }

    if (!m_videoPtsInited) {
        m_videoPtsInited = true;
        m_videoPtsBaseUs = pts;
    }
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.flags = pstStream->keyframe ? AV_PKT_FLAG_KEY : 0;
    int64_t deltaUs = static_cast<int64_t>(pts - m_videoPtsBaseUs);

    if (deltaUs <= m_lastVideoPtsUs) {
        // PTS 未递增，跳过此帧避免时间戳回退
        return 0;
    }
    m_lastVideoPtsUs = deltaUs;
    pkt.pos = -1;
    std::vector<uint8_t> avccPacket;
    if (!annexbToAvcc(payload, payloadLen, avccPacket)) {
        return 0;
    }
    if (av_new_packet(&pkt, static_cast<int>(avccPacket.size())) < 0) {
        std::cerr << "av_new_packet failed, len=" << payloadLen << std::endl;
        return -1;
    }
    memcpy(pkt.data, avccPacket.data(), avccPacket.size());
    pkt.size = static_cast<int>(avccPacket.size());
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
    return 0;
}
