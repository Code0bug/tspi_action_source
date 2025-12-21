#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"
#include "poll.h"
#include "sys/select.h"
#include "sys/time.h"
#include "linux/ioctl.h"
#include "signal.h"

#define AFS_2G  0x03
#define AFS_4G  0x02
#define AFS_8G  0x01
#define AFS_16G 0x00  // default

#define GFS_2000DPS   0x00 // default
#define GFS_1000DPS   0x01
#define GFS_500DPS    0x02
#define GFS_250DPS    0x03
#define GFS_125DPS    0x04
#define GFS_62_5DPS   0x05
#define GFS_31_25DPS  0x06
#define GFS_15_125DPS 0x07


static float accSensitivity   = 0.244f;   
static float gyroSensitivity  = 32.8f;    

float bsp_Icm42688GetAres(unsigned char Ascale)
{
    switch(Ascale)
    {
    // Possible accelerometer scales (and their register bit settings) are:
    // 2 Gs (11), 4 Gs (10), 8 Gs (01), and 16 Gs  (00).
    case AFS_2G:
        accSensitivity = 2000 / 32768.0f;
        break;
    case AFS_4G:
        accSensitivity = 4000 / 32768.0f;
        break;
    case AFS_8G:
        accSensitivity = 8000 / 32768.0f;
        break;
    case AFS_16G:
        accSensitivity = 16000 / 32768.0f;
        break;
    }

    return accSensitivity;
}

float bsp_Icm42688GetGres(unsigned char Gscale)
{
    switch(Gscale)
    {
    case GFS_15_125DPS:
        gyroSensitivity = 15.125f / 32768.0f;
        break;
    case GFS_31_25DPS:
        gyroSensitivity = 31.25f / 32768.0f;
        break;
    case GFS_62_5DPS:
        gyroSensitivity = 62.5f / 32768.0f;
        break;
    case GFS_125DPS:
        gyroSensitivity = 125.0f / 32768.0f;
        break;
    case GFS_250DPS:
        gyroSensitivity = 250.0f / 32768.0f;
        break;
    case GFS_500DPS:
        gyroSensitivity = 500.0f / 32768.0f;
        break;
    case GFS_1000DPS:
        gyroSensitivity = 1000.0f / 32768.0f;
        break;
    case GFS_2000DPS:
        gyroSensitivity = 2000.0f / 32768.0f;
        break;
    }
    return gyroSensitivity;
}


static int fd = 0;	// 文件描述符

/*
 * SIGIO信号处理函数
 * @param - signum 	: 信号值
 * @return 			: 无
 */
void sigio_signal_func(int signum) {
	int ret = 0;
	unsigned int count = 0;

    signed short databuf[7];
	signed short ax, ay, az, gx, gy, gz, tem;

    ret = read(fd, databuf, sizeof(databuf));
    if (ret == 0) {
        ax = databuf[0] * accSensitivity;
        ay = databuf[1] * accSensitivity;
        az = databuf[2] * accSensitivity;
        gx = databuf[3] * gyroSensitivity;
        gy = databuf[4] * gyroSensitivity;
        gz = databuf[5] * gyroSensitivity;
        tem = databuf[6] / 132.48 + 25;

        printf("ax: %d, ay: %d, az: %d, gx: %d, gy: %d, gz: %d, tem: %d\r\n", ax, ay, az, gx, gy, gz, tem);

    }

    printf("sigio_signal_func\n");
}


/*
 * @description		: main主程序
 * @param - argc 	: argv数组元素个数
 * @param - argv 	: 具体参数
 * @return 			: 0 成功;其他 失败
 */
int main(int argc, char *argv[])
{
	int flags = 0;
	char *filename;

	if (argc != 2) {
		printf("Error Usage!\r\n");
		return -1;
	}

	filename = argv[1];
	fd = open(filename, O_RDWR);   // 阻塞访问
	if (fd < 0) {
		printf("Can't open file %s\r\n", filename);
		return -1;
	}

    bsp_Icm42688GetAres(AFS_4G);
    bsp_Icm42688GetGres(GFS_1000DPS);


	// 设置信号SIGIO的处理函数
	signal(SIGIO, sigio_signal_func);

	fcntl(fd, F_SETOWN, getpid());		// 设置当前进程接收SIGIO信号 	
	flags = fcntl(fd, F_GETFL);			// 获取当前的进程状态 			
	fcntl(fd, F_SETFL, flags | FASYNC);	// 设置进程启用异步通知功能 

	while (1) {
		sleep(2);
	}

	close(fd);

	return 0;
}