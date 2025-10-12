#ifndef ROCKITMPI_H
#define ROCKITMPI_H

#include <stdio.h>
#include <sys/poll.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>

#include "rk_defines.h"
#include "rk_debug.h"
#include "rk_mpi_vi.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_venc.h"
#include "rk_mpi_vdec.h"
#include "rk_mpi_vpss.h"
#include "rk_mpi_vo.h"
#include "rk_mpi_rgn.h"
#include "rk_common.h"
#include "rk_comm_rgn.h"
#include "rk_comm_vi.h"
#include "rk_comm_vo.h"
#include "rk_mpi_cal.h"
#include "rk_mpi_mmz.h"

#include "im2d.h"
#include "rga.h"
#include "RgaUtils.h"



void viInit();
int vencInit();
int vdecInit();
//int getFrame(int chn, unsigned char* dat);
int encodeToFile();
int decodeSend();
int decodeGet();


int vi_dev_init1();
int vi_chn_init(int channelId, int width, int height);



#endif // ROCKITMPI_H
