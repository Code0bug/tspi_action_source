#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "icm42688p.h"


#define DEV_CNT		     1			// 设备个数 	
#define DEV_NAME		"icm42688p"	// 设备名称	


struct icm42688p_dev {
	dev_t devid;				// 设备号 	 
	struct cdev cdev;			// cdev 	
	struct class *class;		// 类 		
	struct device *device;		// 设备 	 
	struct device_node	*nd; 	// 设备节点 
	int major;					// 主设备号 
	void *private_data;			// 私有数据 		
	signed short gyro_x_adc;	// 陀螺仪X轴原始值 	 
	signed short gyro_y_adc;	// 陀螺仪Y轴原始值		
	signed short gyro_z_adc;	// 陀螺仪Z轴原始值 		
	signed short accel_x_adc;	// 加速度计X轴原始值 	
	signed short accel_y_adc;	// 加速度计Y轴原始值	
	signed short accel_z_adc;	// 加速度计Z轴原始值 	
	signed short temp_adc;		// 温度原始值 	
};

static struct icm42688p_dev icm42688pdev;


/*
 * @description	: 从icm42688p读取多个寄存器数据
 * @param - dev:  icm42688p设备
 * @param - reg:  要读取的寄存器首地址
 * @param - val:  读取到的数据
 * @param - len:  要读取的数据长度
 * @return 		: 操作结果
 */
static int icm42688p_read_regs(struct icm42688p_dev *dev, u8 reg, void *buf, int len)
{
	int ret = -1;
	unsigned char txdata[1];
	unsigned char * rxdata;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)dev->private_data;
    
	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);	/* 申请内存 */
	if(!t) {
		return -ENOMEM;
	}

	rxdata = kzalloc(sizeof(char) * len, GFP_KERNEL);	/* 申请内存 */
	if(!rxdata) {
		goto out1;
	}

	/* 一共发送len+1个字节的数据，第一个字节为
	寄存器首地址，一共要读取len个字节长度的数据，*/
	txdata[0] = reg | 0x80;		/* 写数据的时候首寄存器地址bit8要置1 */			
	t->tx_buf = txdata;			/* 要发送的数据 */
    t->rx_buf = rxdata;			/* 要读取的数据 */
	t->len = len+1;				/* t->len=发送的长度+读取的长度 */
	spi_message_init(&m);		/* 初始化spi_message */
	spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
	ret = spi_sync(spi, &m);	/* 同步发送 */
	if(ret) {
		goto out2;
	}
	
    memcpy(buf , rxdata+1, len);  /* 只需要读取的数据 */

out2:
	kfree(rxdata);					/* 释放内存 */
out1:	
	kfree(t);						/* 释放内存 */
	
	return ret;
}

/*
 * @description	: 向icm42688p多个寄存器写入数据
 * @param - dev:  icm42688p设备
 * @param - reg:  要写入的寄存器首地址
 * @param - val:  要写入的数据缓冲区
 * @param - len:  要写入的数据长度
 * @return 	  :   操作结果
 */
static s32 icm42688p_write_regs(struct icm42688p_dev *dev, u8 reg, u8 *buf, u8 len)
{
	int ret = -1;
	unsigned char *txdata;
	struct spi_message m;
	struct spi_transfer *t;
	struct spi_device *spi = (struct spi_device *)dev->private_data;
	
	t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);	/* 申请内存 */
	if(!t) {
		return -ENOMEM;
	}
	
	txdata = kzalloc(sizeof(char)+len, GFP_KERNEL);
	if(!txdata) {
		goto out1;
	}
	
	/* 一共发送len+1个字节的数据，第一个字节为
	寄存器首地址，len为要写入的寄存器的集合，*/
	*txdata = reg & ~0x80;	/* 写数据的时候首寄存器地址bit8要清零 */
    memcpy(txdata+1, buf, len);	/* 把len个寄存器拷贝到txdata里，等待发送 */
	t->tx_buf = txdata;			/* 要发送的数据 */
	t->len = len+1;				/* t->len=发送的长度+读取的长度 */
	spi_message_init(&m);		/* 初始化spi_message */
	spi_message_add_tail(t, &m);/* 将spi_transfer添加到spi_message队列 */
	ret = spi_sync(spi, &m);	/* 同步发送 */
    if(ret) {
        goto out2;
    }
	
out2:
	kfree(txdata);				/* 释放内存 */
out1:
	kfree(t);					/* 释放内存 */
	return ret;
}

/*
 * @description	: 读取icm42688p指定寄存器值，读取一个寄存器
 * @param - dev:  icm42688p设备
 * @param - reg:  要读取的寄存器
 * @return 	  :   读取到的寄存器值
 */
static unsigned char icm42688p_read_onereg(struct icm42688p_dev *dev, u8 reg)
{
	u8 data = 0;
	icm42688p_read_regs(dev, reg, &data, 1);
	return data;
}

/*
 * @description	: 向icm42688p指定寄存器写入指定的值，写一个寄存器
 * @param - dev:  icm42688p设备
 * @param - reg:  要写的寄存器
 * @param - data: 要写入的值
 * @return   :    无
 */	

static void icm42688p_write_onereg(struct icm42688p_dev *dev, u8 reg, u8 value)
{
	u8 buf = value;
	icm42688p_write_regs(dev, reg, &buf, 1);
}



static int icm42688p_reginit(void)
{
    uint8_t reg_val = 0;
    // 读取 who am i 寄存器 
    reg_val = icm42688p_read_onereg(&icm42688pdev, ICM42688_WHO_AM_I);
	printk("icm42688p_reginit : reg_val:0x%02x\n",reg_val);

    icm42688p_write_onereg(&icm42688pdev, ICM42688_REG_BANK_SEL, 0); //设置bank 0区域寄存器
    icm42688p_write_onereg(&icm42688pdev, ICM42688_REG_BANK_SEL, 0x01); //软复位传感器
	mdelay(100);

	if (reg_val == ICM42688_ID) {
        icm42688p_write_onereg(&icm42688pdev, ICM42688_REG_BANK_SEL, 0x00);
        //reg_val = icm42688_read_reg(ICM42688_ACCEL_CONFIG0);//page74
        reg_val = (AFS_4G << 5);   // 量程 2g
        reg_val |= (AODR_100Hz);     // 输出速率 100HZ
        icm42688p_write_onereg(&icm42688pdev, ICM42688_ACCEL_CONFIG0, reg_val);

        icm42688p_write_onereg(&icm42688pdev, ICM42688_REG_BANK_SEL, 0x00);
        //reg_val = icm42688_read_reg(ICM42688_GYRO_CONFIG0);//page73
        reg_val = (GFS_1000DPS << 5);   // 量程 1000dps
        reg_val |= (GODR_100Hz);     // 输出速率 100HZ
        icm42688p_write_onereg(&icm42688pdev, ICM42688_GYRO_CONFIG0, reg_val);

        icm42688p_write_onereg(&icm42688pdev, ICM42688_REG_BANK_SEL, 0x00);
        reg_val = icm42688p_read_onereg(&icm42688pdev, ICM42688_PWR_MGMT0); 
        reg_val &= ~(1 << 5);
        reg_val |= ((3) << 2);
        reg_val |= (3);
        icm42688p_write_onereg(&icm42688pdev, ICM42688_PWR_MGMT0, reg_val);
        mdelay(20); 

        return 0;
	}


	return -1;

    
}

static void icm42688p_readdata(struct icm42688p_dev *dev) {

    uint8_t buffer[12] = {0};
	uint8_t buffer_tem[2] = {0};

    icm42688p_read_regs(dev, ICM42688_ACCEL_DATA_X1, buffer, 12);
	icm42688p_read_regs(dev, ICM42688_TEMP_DATA1, buffer_tem, 2);

    dev->temp_adc = (buffer_tem[0] << 8) | buffer_tem[1];

	dev->accel_x_adc = (buffer[0] << 8) | buffer[1];
	dev->accel_y_adc = (buffer[2] << 8) | buffer[3];
	dev->accel_z_adc = (buffer[4] << 8) | buffer[5];
	dev->gyro_x_adc = (buffer[6] << 8) | buffer[7];
	dev->gyro_y_adc = (buffer[8] << 8) | buffer[9];
	dev->gyro_z_adc = (buffer[10] << 8) | buffer[11];
}


/*
 * @description		: 打开设备
 * @param - inode 	: 传递给驱动的inode
 * @param - filp 	: 设备文件，file结构体有个叫做pr似有ate_data的成员变量
 * 					  一般在open的时候将private_data似有向设备结构体。
 * @return 			: 0 成功;其他 失败
 */
static int icm42688p_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &icm42688pdev; /* 设置私有数据 */
	return 0;
}

/*
 * @description		: 从设备读取数据 
 * @param - filp 	: 要打开的设备文件(文件描述符)
 * @param - buf 	: 返回给用户空间的数据缓冲区
 * @param - cnt 	: 要读取的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t icm42688p_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
	signed short data[7];
	long err = 0;
	struct icm42688p_dev *dev = (struct icm42688p_dev *)filp->private_data;

	icm42688p_readdata(dev);
	data[0] = dev->accel_x_adc;
	data[1] = dev->accel_y_adc;
	data[2] = dev->accel_z_adc;
	data[3] = dev->gyro_x_adc;
	data[4] = dev->gyro_y_adc;
	data[5] = dev->gyro_z_adc;
	data[6] = dev->temp_adc;
	err = copy_to_user(buf, data, sizeof(data));
	return 0;
}

/*
 * @description		: 关闭/释放设备
 * @param - filp 	: 要关闭的设备文件(文件描述符)
 * @return 			: 0 成功;其他 失败
 */
static int icm42688p_release(struct inode *inode, struct file *filp)
{
	return 0;
}


// 操作函数
static const struct file_operations icm42688p_ops = { 
	.owner = THIS_MODULE,
	.open = icm42688p_open,
	.read = icm42688p_read,
	.release = icm42688p_release,
};

/*
* @description     : spi驱动的probe函数，当驱动与
*                    设备匹配以后此函数就会执行
* @param - spi     : spi设备
* 
*/	
static int icm42688p_probe(struct spi_device *spi)
{

    printk("icm42688p driver and device was matched!\r\n");

	// 注册设备
	// 1.创建设备号 
	if (icm42688pdev.major) {		//  定义了设备号 
		icm42688pdev.devid = MKDEV(icm42688pdev.major, 0);
		register_chrdev_region(icm42688pdev.devid, DEV_CNT, DEV_NAME);
	} else {			    // 没有定义设备号 
		alloc_chrdev_region(&icm42688pdev.devid, 0, DEV_CNT, DEV_NAME);	// 申请设备号 
		icm42688pdev.major = MAJOR(icm42688pdev.devid);	// 获取分配号的主设备号 
	}
	// 2.注册设备     
	cdev_init(&icm42688pdev.cdev, &icm42688p_ops);
	cdev_add(&icm42688pdev.cdev, icm42688pdev.devid, DEV_CNT);
	// 3.创建类      
	icm42688pdev.class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(icm42688pdev.class)) {
		return PTR_ERR(icm42688pdev.class);
	}
	// 4.创建设备 
	icm42688pdev.device = device_create(icm42688pdev.class, NULL, icm42688pdev.devid, NULL, DEV_NAME);
	if (IS_ERR(icm42688pdev.device)) {
		return PTR_ERR(icm42688pdev.device);
	}

    // 初始化spi_device
    spi->mode = SPI_MODE_0;  // MODE0，CPOL=0，CPHA=0
    spi_setup(spi);
    icm42688pdev.private_data = spi;
    if ( icm42688p_reginit() ) {
		 printk("icm42688p init failed!\r\n");
	}

	return 0;
}

/*
 * @description     : spi驱动的remove函数，移除spi驱动的时候此函数会执行
 * @param - spi 	: spi设备
 * @return          : 0，成功;其他负值,失败
 */
static int icm42688p_remove(struct spi_device *spi)
{
	// 删除设备 
	cdev_del(&icm42688pdev.cdev);
	unregister_chrdev_region(icm42688pdev.devid, DEV_CNT);

	// 注销掉类和设备 
	device_destroy(icm42688pdev.class, icm42688pdev.devid);
	class_destroy(icm42688pdev.class);

	return 0;
}


// 传统匹配方式ID列表 
static const struct spi_device_id icm42688p_id[] = {
	{"rockchip,icm42688p", 0},  
	{}
};

// 设备树匹配列表 
static const struct of_device_id icm42688p_of_match[] = {
	{ .compatible = "rockchip,icm42688p" },
	{ /* Sentinel */ }
};

// SPI驱动结构体 
static struct spi_driver icm42688p_driver = {
	.probe = icm42688p_probe,
	.remove = icm42688p_remove,
	.driver = {
			.owner = THIS_MODULE,
		   	.name = "icm42688p",
		   	.of_match_table = icm42688p_of_match, 
		   },
	.id_table = icm42688p_id,
};
		   
/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init icm42688p_init(void)
{
	return spi_register_driver(&icm42688p_driver);
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit icm42688p_exit(void)
{
	spi_unregister_driver(&icm42688p_driver);
}

module_init(icm42688p_init);
module_exit(icm42688p_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lzp");