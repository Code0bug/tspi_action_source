#include "linux/stddef.h"
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/input/mt.h>
#include <linux/random.h>
#include <linux/backlight.h>

#if 1
#define MY_DEBUG(fmt,arg...);  printk("gp7101_bl:%s %d "fmt"",__FUNCTION__,__LINE__,##arg);
#else
#define MY_DEBUG(fmt,arg...)
#endif

// I2C 背光控制器寄存器定义 
#define BACKLIGHT_REG_CTRL_8  0x03
#define BACKLIGHT_REG_CTRL_16 0x02

// 设备结构体 
struct gp7101_bl_dev {

    struct i2c_client *client;

};

struct gp7101_bl_dev gp7101bldev;	


static s32 i2c_write_regs(struct i2c_client *client, u8 reg, u8 *buf, u8 len)
{
	u8 b[256];
	struct i2c_msg msg;

	b[0] = reg;					// 寄存器首地址 
	memcpy(&b[1],buf,len);		// 将要写入的数据拷贝到数组b里面 
		
	msg.addr = client->addr;	// 从机地址 
	msg.flags = 0;				// 标记为写数据 

	msg.buf = b;				// 要写入的数据缓冲区 
	msg.len = len + 1;			// 要写入的数据长度 

	return i2c_transfer(client->adapter, &msg, 1);
}

// 设置背光亮度
static int gp7101_backlight_set(struct backlight_device *bl) {

    struct gp7101_bl_dev *dev = bl_get_data(bl);  // 获取背光设备结构体
    struct i2c_client *client = dev->client;      // 获取I2C设备指针

    s32 ret;

    u8 addr[1] =  {BACKLIGHT_REG_CTRL_8};
    u8 buf[1] = {bl->props.brightness}  // 缓冲区，存储背光亮度

    MY_DEBUG("pwm:%d", bl->props.brightness);  

    ret = i2c_write_regs(client, addr[0], buf, sizeof(buf));

    while (ret < 0) { // 防止gp7101没有上电成功，发送失败时重新发送
        MY_DEBUG("i2c write failed : %d", ret); 
        msleep(50); 
        ret = i2c_write_regs(client, addr[0], buf, sizeof(buf)); // 重新发送
    }

    return 0;

}

// 背光设备操作结构体
static struct backlight_ops gp7101_backlight_ops = {
    .update_status = gp7101_backlight_set,
};


/*
 * @description		: i2c驱动的probe函数，当驱动与
 * 					  设备匹配以后此函数就会执行，进行设备初始化操作
 * @param - dev 	: i2c设备
 * @return 			: 0，成功;其他负值,失败
 */
static int  gp7101_bl_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	

    struct backlight_device *bl;                  // backlight_device结构用于表示背光设备
    struct backlight_properties props;            // 背光设备的属性
    struct device_node *np = client->dev.of_node; // 设备树中的节点

    printk("gp7101_bl driver and device was matched!\r\n");

    gp7101bldev.client = client;

    // 初始化背光属性结构
    memset(&props, 0, sizeof(props));
    props.type = BACKLIGHT_RAW; // 设置背光类型为原始类型

    of_property_read_u32(np, "max-brightness-levels", &props.max_brightness);  // 从设备树中读取最大亮度级别
    of_property_read_u32(np, "default-brightness-level", &props.brightness);  // 从设备树中读取默认亮度级别

    if (props.max_brightness > 255 || props.max_brightness < 0) {
        props.max_brightness = 255;
    }
    if (props.brightness > props.max_brightness || props.brightness < 0) {
        props.brightness = props.max_brightness;
    }

    // 注册背光设备
    bl = devm_backlight_device_register(&client->dev, "backlight", &client->dev, 
                                        &gp7101bldev, &gp7101_backlight_ops, &props);
    
    printk("max_brightness:%d  default_brightness:%d\r\n", props.max_brightness, props.brightness);

    // msleep(500); // 等待 gp7101 上电
    
    backlight_update_status(bl); // 更新背光设备的状态


	return 0;
}


/*
 * @description     : i2c驱动的remove函数，移除i2c驱动的时候此函数会执行
 * @param - client 	: i2c设备
 * @return          : 0，成功;其他负值,失败
 */
static int gp7101_bl_remove(struct i2c_client *client)
{
    MY_DEBUG("locat");
	return 0;
}


// 传统匹配方式ID列表 , 这个必须要有
static const struct i2c_device_id gp7101_bl_id[] = {
	{"gp7101-backlight", 0},  
	{}
};

// 设备树匹配列表
static const struct of_device_id gp7101_bl_of_match[] = {
	{ .compatible = "gp7101-backlight" },
	{ }
};

// i2c驱动结构体  
static struct i2c_driver gp7101_bl_driver = {
	.driver		= {
		.owner = THIS_MODULE,
		.name	= "gp7101-bl",				// 驱动名称，用于和设备匹配 
		.of_match_table	= gp7101_bl_of_match, // 设备树匹配表 		 
	},
	.probe		= gp7101_bl_probe,
	.remove		= gp7101_bl_remove,
	.id_table = gp7101_bl_id,
};



/*
 * @description	: 驱动模块加载函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init gp7101_bl_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&gp7101_bl_driver);
	return ret;
}

/*
 * @description	: 驱动模块卸载函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit gp7101_bl_exit(void)
{
	i2c_del_driver(&gp7101_bl_driver);
}



module_init(gp7101_bl_init);
module_exit(gp7101_bl_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lzp");
