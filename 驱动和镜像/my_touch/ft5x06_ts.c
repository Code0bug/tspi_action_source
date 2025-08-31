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

#define TD_STATUS 0x02  // 触摸数据寄存器地址
 
#if 0
#define MY_DEBUG(fmt,arg...);  printk("ft5x06_ts:%s %d "fmt"",__FUNCTION__,__LINE__,##arg);
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

struct ft5x06_ts_dev {
    int irq_pin, reset_pin;      // 中断和复位IO
    int irqnum;                  // 中断号
    int max_x;                   // 最大横坐标
    int max_y;                   // 最大纵坐标
    struct input_dev *input;     // input结构体
    struct i2c_client *client;   // I2C客户端
    struct touch_point pt;       // 触摸点数据
};
struct ft5x06_ts_dev ft5x06dev;



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

// 中断服务函数
static irqreturn_t ft5x06_ts_irq_handler(int irq, void *dev_id)
{
    s32 ret = -1;
    u8 i = 0;
    u8 point_data[31]; // 读取数据缓冲区
    u8 touch_num = 0;  // 触摸点数量
    int event_flag, touch_id, input_x, input_y; // 定义一些用于存储事件信息的变量
    struct touch_point pt; // 定义触摸点数据结构体变量
    struct ft5x06_ts_dev *dev = dev_id;
	struct i2c_client *client = (struct i2c_client *)dev->client;

    ret = i2c_read_regs(client, TD_STATUS, point_data, sizeof(point_data));
    if (ret < 0) {                        // 如果读取失败
        MY_DEBUG("I2C write end_cmd error!"); // 打印错误信息
    }

    touch_num = point_data[0] & 0x0f;
    MY_DEBUG("touch_num:%d", touch_num); // 打印触摸点数量

    for (i = 0; i < 5; i++) {
        // 获取触摸点数据
        memcpy(&pt, &point_data[1+6*i], sizeof(pt));

        /*
        解析触摸点的事件标志位
        00b: 按下
        01b: 抬起
        10b: 接触（长按）
        11b: 保留
        */
        event_flag = pt.xh >> 6;
        if(event_flag == 0x03) continue; // 如果事件标志位不是按下或抬起，则跳过此循环
        touch_id = pt.yh >> 4;    // 获取触摸点ID

        MY_DEBUG("i:%d touch_id:%d event_flag:%d",i, touch_id, event_flag); // 打印调试信息
        input_x = ((pt.xh & 0x0f) << 8) | pt.xl;
        input_y = ((pt.yh & 0x0f) << 8) | pt.yl;

        MY_DEBUG("i:%d,x:%d,y:%d", i, input_x, input_y); // 打印调试信息

        input_mt_slot(dev->input, touch_id);      // 设置输入设备的触摸槽位
        if (event_flag == 0) { // 如果是按下
            // 上报按下事件和坐标
            input_mt_report_slot_state(dev->input, MT_TOOL_FINGER, true); // 设置为按下状态
            input_report_abs(dev->input, ABS_MT_POSITION_X, 480-input_x); // 报告X坐标
            input_report_abs(dev->input, ABS_MT_POSITION_Y, input_y); // 报告Y坐标
        } else if (event_flag == 2) { // 如果是长按
            // 直接上报数据
            input_report_abs(dev->input, ABS_MT_POSITION_X, 480-input_x); // 报告X坐标
            input_report_abs(dev->input, ABS_MT_POSITION_Y, input_y); // 报告Y坐标
        } else if (event_flag == 1) { // 如果是触摸抬起
            // 上报事件
            input_mt_report_slot_state(dev->input, MT_TOOL_FINGER, false); // 设置为抬起状态
        }
    

    }

    // 报告输入设备的指针仿真信息
    input_mt_report_pointer_emulation(dev->input, true);
    // 同步输入事件
    input_sync(dev->input);

	return IRQ_HANDLED;
}



/*
 * @description		: i2c驱动的probe函数，当驱动与
 * 					  设备匹配以后此函数就会执行，进行设备初始化操作
 * @param - dev 	: i2c设备
 * @return 			: 0，成功;其他负值,失败
 */
static int ft5x06_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	

    u8 ret = 0;
    struct device_node *np = client->dev.of_node; // 获取设备节点

	printk("ft5x06_ts driver and device was matched!\r\n");

    ft5x06dev.client = client;

    // 获取复位引脚和中断引脚编号
    ft5x06dev.irq_pin = of_get_named_gpio(np, "touch-gpio", 0);
    ft5x06dev.reset_pin = of_get_named_gpio(np, "reset-gpio", 0);
    // 申请复位引脚
    ret = devm_gpio_request_one(&client->dev, ft5x06dev.reset_pin, GPIOF_OUT_INIT_HIGH, "ft5x06dev reset");
    if (ret != 0) {
        return ret;
    }
    // 申请中断引脚
    ret = devm_gpio_request_one(&client->dev, ft5x06dev.irq_pin, GPIOF_OUT_INIT_HIGH, "ft5x06dev irq");
    if (ret != 0) {
        return ret;
    }

    // 从设备树读取最大横坐标和纵坐标
    of_property_read_u32(np, "max-x", &ft5x06dev.max_x);
    of_property_read_u32(np, "max-y", &ft5x06dev.max_y);

    MY_DEBUG("max_x:%d  max-y:%d", ft5x06dev.max_x, ft5x06dev.max_y); 

    // 复位触摸屏设备
    gpio_direction_output(ft5x06dev.reset_pin, 0); // 设置复位管脚输出低电平
    msleep(20); // 等待20毫秒
    gpio_direction_output(ft5x06dev.irq_pin, 0); // 设置中断管脚输出低电平
    msleep(2); // 等待2毫秒
    gpio_direction_output(ft5x06dev.reset_pin, 1); // 设置复位管脚输出高电平
    msleep(6); // 等待6毫秒
    gpio_direction_output(ft5x06dev.irq_pin, 0); // 设置中断管脚输出低电平
    msleep(50); // 等待50毫秒

    // input设备注册 
	// 1.分配input设备结构体
	ft5x06dev.input = devm_input_allocate_device(&client->dev); // 分配一个输入设备
	if (!ft5x06dev.input) {
		ret = -ENOMEM;
		return ret;
	}
	// 2.配置input设备基本信息
	ft5x06dev.input->name = client->name;          // 设备名称
	ft5x06dev.input->id.bustype = BUS_I2C;         // 总线类型

	// 3.设置设备支持的事件类型
	__set_bit(EV_KEY, ft5x06dev.input->evbit);     // 设置按键事件
	__set_bit(EV_ABS, ft5x06dev.input->evbit);     // 设置绝对坐标事件（触摸位置）
	__set_bit(BTN_TOUCH, ft5x06dev.input->keybit); // 设置触摸按键（标记触摸状态）

	// 4.配置多点触摸坐标参数
	input_set_abs_params(ft5x06dev.input, ABS_MT_POSITION_X,0, ft5x06dev.max_x, 0, 0);
	input_set_abs_params(ft5x06dev.input, ABS_MT_POSITION_Y,0, ft5x06dev.max_y, 0, 0);	


	// 5.初始化多点触摸槽位   
	ret = input_mt_init_slots(ft5x06dev.input, 5, INPUT_MT_DIRECT);
	if (ret) {
        dev_err(&client->dev, "Input mt init error\n"); // 打印错误信息
		return ret;
	}

	// 6.注册输入设备
	ret = input_register_device(ft5x06dev.input);
	if (ret) {
        return ret;
    }
		
    // 申请中断
    ft5x06dev.irqnum = gpio_to_irq(ft5x06dev.irq_pin); // 将GPIO管脚转换为中断号
	ret = devm_request_threaded_irq(&client->dev, ft5x06dev.irqnum, NULL,
					ft5x06_ts_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					client->name, &ft5x06dev);
	if (ret) {
		dev_err(&client->dev, "Unable to request touchscreen IRQ.\n");
		return ret;
	}


	return 0;
}


/*
 * @description     : i2c驱动的remove函数，移除i2c驱动的时候此函数会执行
 * @param - client 	: i2c设备
 * @return          : 0，成功;其他负值,失败
 */
static int ft5x06_ts_remove(struct i2c_client *client)
{


	return 0;
}




// 设备树匹配列表
static const struct of_device_id ft5x06_ts_of_match[] = {
	{ .compatible = "ft5x06-touch" },
	{ }
};
MODULE_DEVICE_TABLE(of, ft5x06_ts_of_match);

// i2c驱动结构体
static struct i2c_driver ft5x06_ts_driver = {
	.driver		= {
		.owner = THIS_MODULE,
		.name	= "ft5x06_ts",				  // 驱动名称，用于和设备匹配 
		.of_match_table	= ft5x06_ts_of_match, // 设备树匹配表 		 
	},
	.probe		= ft5x06_ts_probe,
	.remove		= ft5x06_ts_remove,
};



/*
 * @description	: 驱动模块加载函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init ft5x06_ts_driver_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&ft5x06_ts_driver);
	return ret;
}

/*
 * @description	: 驱动模块卸载函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit ft5x06_ts_driver_exit(void)
{
	i2c_del_driver(&ft5x06_ts_driver);
}



module_init(ft5x06_ts_driver_init);
module_exit(ft5x06_ts_driver_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("lzp");