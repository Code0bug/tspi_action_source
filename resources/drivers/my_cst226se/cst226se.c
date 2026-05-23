#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/i2c.h>
#include <asm/unaligned.h>
#include "cst226se.h"

 
#if 1
#define MY_DEBUG(fmt,arg...);  printk("cst226se_ts:%s %d "fmt"",__FUNCTION__,__LINE__,##arg);
#else
#define MY_DEBUG(fmt,arg...)
#endif

// 触摸点数据结构体
struct touch_point {
    u8 xh;   // 高 4 位是 event flag，低 4 位是高 4 位 x 
    u8 xl;   // 低 8 位 x 
    u8 yh;   // 高 4 位是 touch id，低 4 位是高 4 位 y 
    u8 yl;   // 低 8 位 y 
    u8 weight;
    u8 misc;
} __packed;

struct cst226se_ts_dev {
    int irq_pin, reset_pin;      // 中断和复位IO
    int irqnum;                  // 中断号
    int max_x;                   // 最大横坐标
    int max_y;                   // 最大纵坐标
    struct input_dev *input;     // input结构体
    struct i2c_client *client;   // I2C客户端
    struct touch_point pt;       // 触摸点数据
};
struct cst226se_ts_dev cst226sedev;



static int i2c_read_regs(struct i2c_client *client, u8 reg, void *val, int len)
{
	int ret;
	struct i2c_msg msg[2];

	// msg[0]为发送要读取的首地址 
	msg[0].addr = client->addr;			// 从机设备地址 
	msg[0].flags = 0;					// 标记为发送数据 
	msg[0].buf = &reg;					// 读取的首地址 
	msg[0].len = 1;						// reg长度

	// msg[1]读取数据 
	msg[1].addr = client->addr;			// 要写入的地址 
	msg[1].flags = I2C_M_RD;			// 标记为读取数据
	msg[1].buf = val;					// 读取数据缓冲区 
	msg[1].len = len;					// 要读取的数据长度

	ret = i2c_transfer(client->adapter, msg, 2);
	if(ret == 2) {
		ret = 0;
	} else {
		printk("i2c rd failed=%d reg=%06x len=%d\n",ret, reg, len);
		ret = -EREMOTEIO;
	}
	return ret;
}

// // 中断服务函数
// static irqreturn_t cst226se_ts_irq_handler(int irq, void *dev_id)
// {


// 	return IRQ_HANDLED;
// }



/*
 * @description		: i2c驱动的probe函数，当驱动与
 * 					  设备匹配以后此函数就会执行，进行设备初始化操作
 * @param - dev 	: i2c设备
 * @return 			: 0，成功;其他负值,失败
 */
static int cst226se_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	
    u8 ret = 0;
    char type_str[3]; // 存储芯片类型字符串
    u8 data[2];
    u16 chip_type;

    MY_DEBUG("cst226se driver and device was matched!\r\n");

    // struct device_node *np = client->dev.of_node;

    ret = i2c_read_regs(client, HYN_REG_CAP_CHIP_ID, data, 2);

    memcpy(type_str, data, 2);
    type_str[2] = 0; // 字符串结束符

    chip_type = (data[1] << 8) | data[0];

    MY_DEBUG("read chip type : 0x%04X\r\n", chip_type);



	return 0;
}


/*
 * @description     : i2c驱动的remove函数，移除i2c驱动的时候此函数会执行
 * @param - client 	: i2c设备
 * @return          : 0，成功;其他负值,失败
 */
static int cst226se_ts_remove(struct i2c_client *client)
{


	return 0;
}


// 设备树匹配列表
static const struct of_device_id cst226se_ts_of_match[] = {
	{ .compatible = "cst226se-touch" },
	{ }
};
MODULE_DEVICE_TABLE(of, cst226se_ts_of_match);

// i2c驱动结构体
static struct i2c_driver cst226se_ts_driver = {
	.driver		= {
		.owner = THIS_MODULE,
		.name	= "cst226se_ts",				  // 驱动名称，用于和设备匹配 
		.of_match_table	= cst226se_ts_of_match, // 设备树匹配表 		 
	},
	.probe		= cst226se_ts_probe,
	.remove		= cst226se_ts_remove,
};



/*
 * @description	: 驱动模块加载函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init cst226se_ts_driver_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&cst226se_ts_driver);
	return ret;
}

/*
 * @description	: 驱动模块卸载函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit cst226se_ts_driver_exit(void)
{
	i2c_del_driver(&cst226se_ts_driver);
}



module_init(cst226se_ts_driver_init);
module_exit(cst226se_ts_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lzp");