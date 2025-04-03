// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2022 Linaro Ltd
 * Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 */
#include <linux/bits.h>
#include <linux/bitfield.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/of_gpio.h>

const uint32_t gpio_base[] = {
	0xFDD60000, // gpio0
	0xFE740000, // gpio1
	0xFE750000, // gpio2
	0xFE760000, // gpio3
	0xFE770000, // gpio4
};
#define SWPORT_DR_L		0x0000
#define SWPORT_DR_H		0x0004

static uint32_t ws2812_pin = 0;
static volatile uint32_t *ws2812_gpio_port;
static volatile uint32_t ws2812_set_val = 0;
static volatile uint32_t ws2812_reset_val = 0;

DEFINE_SPINLOCK(lock);

// ws2812 reset
static inline void ws2812_rst(void)
{
	*ws2812_gpio_port = ws2812_reset_val;
	udelay(300);// RES low voltage time, Above 50µs
}

static inline void ws2812_code_0(void)
{
	// loop for delay about 300ns (250ns ~ 380ns)
	// 2 times for 110ns
	*ws2812_gpio_port = ws2812_set_val;
	*ws2812_gpio_port = ws2812_set_val;
	// loop for delay about 800ns (700ns~1us)
	// 15 times for 800ns
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
}

static inline void ws2812_code_1(void)
{
	// loop for delay about 700ns (650ns~950ns)
	// 13 times for 700ns
	*ws2812_gpio_port = ws2812_set_val;
	*ws2812_gpio_port = ws2812_set_val;
	*ws2812_gpio_port = ws2812_set_val;
	*ws2812_gpio_port = ws2812_set_val;
	*ws2812_gpio_port = ws2812_set_val;
	*ws2812_gpio_port = ws2812_set_val;
	*ws2812_gpio_port = ws2812_set_val;
	*ws2812_gpio_port = ws2812_set_val;
	*ws2812_gpio_port = ws2812_set_val;
	*ws2812_gpio_port = ws2812_set_val;
	*ws2812_gpio_port = ws2812_set_val;
	*ws2812_gpio_port = ws2812_set_val;
	*ws2812_gpio_port = ws2812_set_val;

	// loop for delay about 600ns (580ns~600ns)
	// 11 times for 590ns
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
	*ws2812_gpio_port = ws2812_reset_val;
}

static inline void ws2812_Write_24Bits(uint32_t grb)
{
	uint8_t i;
	for (i = 0; i < 24; i++)
	{
		if (!(grb & 0x800000))
		{
			ws2812_code_0();
		}
		else
		{
			ws2812_code_1();
		}
		grb <<= 1;
	}
}

static void ws2812_write_array(uint32_t *rgb, uint32_t cnt)
{
	uint32_t i = 0;
	unsigned long flags;

	for (i = 0; i < cnt; i++)
	{
		// rgb -> grb
		rgb[i] = (((rgb[i] >> 16) & 0xff) << 8) | (((rgb[i] >> 8) & 0xff) << 16) | ((rgb[i]) & 0xff);
	}

	spin_lock_irqsave(&lock, flags);

	ws2812_rst();

	for (i = 0; i < cnt; i++)
	{
		ws2812_Write_24Bits(rgb[i]);
	}

	spin_unlock_irqrestore(&lock, flags);
}

ssize_t ws2812_read(struct file *file, char __user *user, size_t bytesize, loff_t *this_loff_t)
{
	return 0;
}

ssize_t ws2812_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	uint32_t rgb[255];
	unsigned long ret = 0;

	if (count > 255 * 4) count = 255 * 4;
	ret = copy_from_user(&rgb[0], user_buf, count);
	if (ret < 0)
	{
		printk("copy_from_user fail!!!\n");
		return -1;
	}

	ws2812_write_array((uint32_t *)rgb, count / 4);

	return 0;
}

int ws2812_open(struct inode *inode, struct file *file)
{
	return 0;
}

int ws2812_close(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations ws2812_ops = {
	.owner = THIS_MODULE,
	.open = ws2812_open,
	.release = ws2812_close,
	.write = ws2812_write,
};

static struct miscdevice ws2812_misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ws2812-led",
	.fops = &ws2812_ops,
};

static int ws2812_probe(struct platform_device *pdev)
{
	int ret;
	enum of_gpio_flags flag;
	struct device_node *ws2812_gpio_node = pdev->dev.of_node;
	uint32_t rgb_cnt = 0;
	uint32_t rgb[255];
	uint8_t port = 0;
	uint8_t pin = 0;
	uint8_t offset = 0;
	uint32_t data_reg = 0;

	of_property_read_u32(ws2812_gpio_node, "rgb_cnt", &rgb_cnt);
	if (rgb_cnt > 255)
		rgb_cnt = 255;

	of_property_read_u32_array(ws2812_gpio_node, "rgb_value", rgb, rgb_cnt);
	ws2812_pin = of_get_named_gpio_flags(ws2812_gpio_node, "gpios", 0, &flag);
	if (!gpio_is_valid(ws2812_pin))
	{
		printk(KERN_ERR "ws2812: gpio: %d is invalid\n", ws2812_pin);
		return -ENODEV;
	}
	port = ws2812_pin >> 5;
	if (port >= ARRAY_SIZE(gpio_base))
	{
		printk(KERN_ERR "ws2812: port: %d is invalid\n", port);
		return -ENODEV;
	}

	if (gpio_request(ws2812_pin, "ws2812-gpio"))
	{
		printk(KERN_ERR "ws2812: gpio %d request failed!\n", ws2812_pin);
		gpio_free(ws2812_pin);
		return -ENODEV;
	}

	pin = ws2812_pin & 0x1F;
	if (pin >= 16)
	{
		offset = pin - 16;
		data_reg = SWPORT_DR_H;
	}
	else
	{
		offset = pin;
		data_reg = SWPORT_DR_L;
	}
	ws2812_gpio_port = ioremap(gpio_base[port] + data_reg, 4);
	// value | write_mask
	ws2812_set_val = (1 << offset) | ((1 << offset) << 16);
	ws2812_reset_val = (0 << offset) | ((1 << offset) << 16);

	gpio_direction_output(ws2812_pin, 0);

	ret = misc_register(&ws2812_misc_dev);
	msleep(50);

	ws2812_write_array(rgb, rgb_cnt);

	return 0;
}

static int ws2812_remove(struct platform_device *pdev)
{
	misc_deregister(&ws2812_misc_dev);
	gpio_free(ws2812_pin);

	return 0;
}

static const struct of_device_id ws2812_of_match[] = {
	{.compatible = "rgb-ws2812"},
	{/* sentinel */}};

MODULE_DEVICE_TABLE(of, ws2812_of_match);

static struct platform_driver ws2812_driver = {
	.probe		= ws2812_probe,
	.remove		= ws2812_remove,
	.driver		= {
		.name	= "ws2812_ctl",
		.of_match_table = ws2812_of_match,
	},
};

module_platform_driver(ws2812_driver);

MODULE_AUTHOR("MacLodge, Alan Ma <tech@biqu3d.com>");
MODULE_DESCRIPTION("WS2812 RGB driver for Rockchip RK3566");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ws2812_ctl");
