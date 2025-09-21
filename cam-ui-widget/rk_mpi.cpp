#include "rk_mpi.h"

int vdec_init(int width, int height) {
    // 解码相关配置
    RK_S32 s32Ret = RK_SUCCESS;
    VDEC_CHN_ATTR_S stAttr;
    VDEC_CHN_PARAM_S stVdecParam;
    MB_POOL_CONFIG_S stMbPoolCfg;
    VDEC_PIC_BUF_ATTR_S stVdecPicBufAttr;
    MB_PIC_CAL_S stMbPicCalResult;
    VDEC_MOD_PARAM_S stModParam;

    memset(&stAttr, 0, sizeof(VDEC_CHN_ATTR_S));
    memset(&stVdecParam, 0, sizeof(VDEC_CHN_PARAM_S));
    memset(&stModParam, 0, sizeof(VDEC_MOD_PARAM_S));


    stModParam.enVdecMBSource = MB_SOURCE_USER;     // 内存池：用户池
    s32Ret = RK_MPI_VDEC_SetModParam(&stModParam);  // 设置解码通道参数
    if (s32Ret != RK_SUCCESS) {
        printf("SetModParam failed!\n");
        return s32Ret;
    }

    stAttr.enMode = VIDEO_MODE_FRAME;  // 码流发送方式：流式/帧式
    stAttr.enType = RK_VIDEO_ID_AVC;   // 码流格式
    stAttr.u32PicWidth = width;        // 视频宽度
    stAttr.u32PicHeight = height;      // 视频高度
    stAttr.u32FrameBufCnt = 8;
    stAttr.u32StreamBufCnt = 8;

    s32Ret = RK_MPI_VDEC_CreateChn(0, &stAttr); // 创建视频解码通道
    if (s32Ret != RK_SUCCESS) {
        printf("create vdec failed! \n");
        return s32Ret;
    }

    stVdecParam.stVdecVideoParam.enCompressMode = COMPRESS_MODE_NONE;
    s32Ret = RK_MPI_VDEC_SetChnParam(0, &stVdecParam); // 设置通道参数
    if (s32Ret != RK_SUCCESS) {
        printf("set chn param failed \n");
        return s32Ret;
    }

    RK_S32 vdec_fd = RK_MPI_VDEC_GetFd(0); // 获取视频解码通道的设备文件句柄，用于查询解码通道数据有产生解码图像
    if (vdec_fd <= 0) {
        printf("get fd chn failed \n");
        return s32Ret;
    }

    s32Ret = RK_MPI_VDEC_StartRecvStream(0); // 解码器开始接收用户发送的码流
    if (s32Ret != RK_SUCCESS) {
        printf("start recv chn failed \n");
        return s32Ret;
    }

    return 0;

}


int vi_dev_init() {
	printf("%s\n", __func__);
	int ret = 0;
	int devId = 0;
	int pipeId = devId;

	VI_DEV_ATTR_S stDevAttr;
	VI_DEV_BIND_PIPE_S stBindPipe;
	memset(&stDevAttr, 0, sizeof(stDevAttr));
	memset(&stBindPipe, 0, sizeof(stBindPipe));
	// 0. get dev config status
	ret = RK_MPI_VI_GetDevAttr(devId, &stDevAttr);
	if (ret == RK_ERR_VI_NOT_CONFIG) {
		// 0-1.config dev
		ret = RK_MPI_VI_SetDevAttr(devId, &stDevAttr);
		if (ret != RK_SUCCESS) {
			printf("RK_MPI_VI_SetDevAttr %x\n", ret);
			return -1;
		}
	} else {
		printf("RK_MPI_VI_SetDevAttr already\n");
	}
	// 1.get dev enable status
	ret = RK_MPI_VI_GetDevIsEnable(devId);
	if (ret != RK_SUCCESS) {
		// 1-2.enable dev
		ret = RK_MPI_VI_EnableDev(devId);
		if (ret != RK_SUCCESS) {
			printf("RK_MPI_VI_EnableDev %x\n", ret);
			return -1;
		}
		// 1-3.bind dev/pipe
		stBindPipe.u32Num = 1;
		stBindPipe.PipeId[0] = pipeId;
		ret = RK_MPI_VI_SetDevBindPipe(devId, &stBindPipe);
		if (ret != RK_SUCCESS) {
			printf("RK_MPI_VI_SetDevBindPipe %x\n", ret);
			return -1;
		}
	} else {
		printf("RK_MPI_VI_EnableDev already\n");
	}

	return 0;
}

int vi_chn_init(int channelId, int width, int height) {
	int ret;
    int buf_cnt = 6;
	// VI init
	VI_CHN_ATTR_S vi_chn_attr;
	memset(&vi_chn_attr, 0, sizeof(vi_chn_attr));
    vi_chn_attr.stIspOpt.u32BufCount = buf_cnt;
	vi_chn_attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF; // VI_V4L2_MEMORY_TYPE_MMAP;
	vi_chn_attr.stSize.u32Width = width;
	vi_chn_attr.stSize.u32Height = height;
	vi_chn_attr.enPixelFormat = RK_FMT_YUV420SP;
	vi_chn_attr.enCompressMode = COMPRESS_MODE_NONE; // COMPRESS_AFBC_16x16;
    vi_chn_attr.u32Depth = 2; //0, get fail, 1 - u32BufCount, can get, if bind to other device, must be < u32BufCount
	ret = RK_MPI_VI_SetChnAttr(0, channelId, &vi_chn_attr);
	ret |= RK_MPI_VI_EnableChn(0, channelId);
	if (ret) {
		printf("ERROR: create VI error! ret=%d\n", ret);
		return ret;
	}

	return ret;
}

int venc_init(int chnId, int width, int height, RK_CODEC_ID_E enType) {
	printf("%s\n",__func__);
	VENC_RECV_PIC_PARAM_S stRecvParam;
	VENC_CHN_ATTR_S stAttr;
	memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));

	if (enType == RK_VIDEO_ID_AVC) {
		stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
		stAttr.stRcAttr.stH264Cbr.u32BitRate = 10 * 1024;
		stAttr.stRcAttr.stH264Cbr.u32Gop = 1;
	} else if (enType == RK_VIDEO_ID_HEVC) {
		stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
		stAttr.stRcAttr.stH265Cbr.u32BitRate = 10 * 1024;
		stAttr.stRcAttr.stH265Cbr.u32Gop = 60;
	} else if (enType == RK_VIDEO_ID_MJPEG) {
		stAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
		stAttr.stRcAttr.stMjpegCbr.u32BitRate = 10 * 1024;
	}

	stAttr.stVencAttr.enType = enType;
	stAttr.stVencAttr.enPixelFormat = RK_FMT_RGB888;
	if (enType == RK_VIDEO_ID_AVC)
		stAttr.stVencAttr.u32Profile = H264E_PROFILE_HIGH;
	stAttr.stVencAttr.u32PicWidth = width;
	stAttr.stVencAttr.u32PicHeight = height;
	stAttr.stVencAttr.u32VirWidth = width;
	stAttr.stVencAttr.u32VirHeight = height;
	stAttr.stVencAttr.u32StreamBufCnt = 2;
	stAttr.stVencAttr.u32BufSize = width * height * 3 / 2;
	stAttr.stVencAttr.enMirror = MIRROR_NONE;

	RK_MPI_VENC_CreateChn(chnId, &stAttr);

	memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
	stRecvParam.s32RecvPicNum = -1;
	RK_MPI_VENC_StartRecvFrame(chnId, &stRecvParam);

	return 0;
}

int vpss_init(int chnId, int width, int height)
{
    RK_S32 s32Ret = RK_SUCCESS;
    VPSS_GRP VpssGrp = 0;           // VPSS组组号
    VPSS_CHN VpssChn[VPSS_MAX_CHN_NUM] = { VPSS_CHN0, VPSS_CHN1, VPSS_CHN2, VPSS_CHN3 }; // VPSS通道号
    VPSS_GRP_ATTR_S stGrpVpssAttr;  // VPSS组属性
    VPSS_CHN_ATTR_S stVpssChnAttr;  // VPSS通道属性
    MPP_CHN_S stViChn, stVpssChn;   // 模块设备通道结构体

    stGrpVpssAttr.u32MaxW = 1920;  // 输入图像的最大宽度
    stGrpVpssAttr.u32MaxH = 1080;  // 输入图像的最大高度
    stGrpVpssAttr.enPixelFormat = RK_FMT_YUV420SP;           // 输入图像像素格式
    stGrpVpssAttr.stFrameRate.s32SrcFrameRate = -1;          // 帧率控制
    stGrpVpssAttr.stFrameRate.s32DstFrameRate = -1;
    stGrpVpssAttr.enCompressMode = COMPRESS_MODE_NONE;       // 输入图像压缩模式
    s32Ret = RK_MPI_VPSS_CreateGrp(VpssGrp, &stGrpVpssAttr); // 创建一个VPSS组
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }

    stVpssChnAttr.enChnMode = VPSS_CHN_MODE_USER;
    stVpssChnAttr.enDynamicRange = DYNAMIC_RANGE_SDR8; // 目标图像动态范围
    stVpssChnAttr.enPixelFormat = RK_FMT_BGR888;       // 目标图像像素格式
    stVpssChnAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssChnAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssChnAttr.u32Depth = 2;
    stVpssChnAttr.u32Width = width;                    // 目标图像宽度
    stVpssChnAttr.u32Height = height;                  // 目标图像高度
    stVpssChnAttr.enCompressMode = COMPRESS_MODE_NONE; // 目标图像压缩模式
    s32Ret = RK_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn[chnId], &stVpssChnAttr); // 设置VPSS通道属性
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }

    s32Ret = RK_MPI_VPSS_EnableChn(VpssGrp, VpssChn[chnId]);  // 使能通道
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }

    s32Ret = RK_MPI_VPSS_EnableBackupFrame(VpssGrp);      // 使能 Backup 帧
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }

    s32Ret = RK_MPI_VPSS_StartGrp(VpssGrp);               // 启用VPSS组
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }
    // bind vi to vpss
    stViChn.enModId    = RK_ID_VI;
    stViChn.s32DevId   = 0;
    stViChn.s32ChnId   = 1;
    stVpssChn.enModId = RK_ID_VPSS;
    stVpssChn.s32DevId = 0;
    stVpssChn.s32ChnId = 0;

    RK_LOGD("vi to vpss ch %d vpss group %d", stVpssChn.s32ChnId , stVpssChn.s32DevId);
    s32Ret = RK_MPI_SYS_Bind(&stViChn, &stVpssChn);
    if (s32Ret != RK_SUCCESS) {
        RK_LOGE("vi and vpss bind error ");
        return s32Ret;
    }

    return 0;
}


