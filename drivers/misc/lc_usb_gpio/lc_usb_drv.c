/*
 *
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/async.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/io.h>

#include <linux/ioctl.h>
#include <linux/proc_fs.h>
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <linux/pm_wakeup.h>
#include <linux/pm_runtime.h>
//#include <linux/errno.h>
#include <linux/fwnode.h>
#include <linux/string.h>
#include <linux/kdev_t.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/genhd.h>
#include <linux/kallsyms.h>
#include <linux/mutex.h>
#include <linux/pm_runtime.h>
#include <linux/netdevice.h>
#include <linux/sched/signal.h>
#include <linux/sysfs.h>

#include <linux/kthread.h>
#include <linux/sched.h>


#define LONGCHEER_DEBUG
#define LONGCHEER_SUSPEND 0
#define LONGCHEER_RESUME 1


struct longcheer_gpio_pin {
	int usb_protect_en_pin;/*1 disable usb; 0 enable usb*/
};

struct longcheer_gpio_pin *longcheer_gpio_ctl;


#ifdef CONFIG_OF
static int longcheer_parse_dt(struct device *dev, struct longcheer_gpio_pin *data)
{
	u32 tmp;
	int ret;
	struct device_node *np = dev->of_node;

#ifdef LONGCHEER_DEBUG
	printk(KERN_CRIT"####%s####\n", __func__);
#endif

	data->usb_protect_en_pin = of_get_named_gpio_flags(np, "linux,usb-protect",0, &tmp);
	if (!gpio_is_valid(data->usb_protect_en_pin)) {
		printk(KERN_CRIT" usb_protect_en_pin is not valid\n");
		return -EINVAL;
	} else {
		printk(KERN_CRIT" usb_protect_en_pin is valid\n");
	}
	ret = gpio_request(data->usb_protect_en_pin,"fusb_protect_en_pin");
	if (ret) {
		printk(KERN_CRIT"%s: failed to usb_protect_en_pin %d,ret = %d\n",__func__,data->usb_protect_en_pin,ret);
		return -EINVAL;
	} else {
		printk(KERN_CRIT"%s: Succeed to usb_protect_en_pin %d,ret = %d\n",__func__,data->usb_protect_en_pin,ret);
	}	
	gpio_direction_output(data->usb_protect_en_pin, 0);

	return 0;
}
#endif

/*Added by xukai. 20191216. start*/
//static int read_usb_protect_gpio_status = 0;
static int usb_charging_status = 0;
/*Added by xukai. 20191216. end*/
static ssize_t lc_gpio_r_show(struct class *class, struct class_attribute *attr, char *data)
{
	printk(KERN_CRIT"####%s####\n", __func__);
	//return sprintf(buf, "lc_gpio_adc = %d\n", lc_gpio_adc);
	/*Added by xukai. 20191125. start*/
	//return sprintf(data, "%d\n", read_usb_protect_gpio_status);
	return sprintf(data, "%d\n", usb_charging_status);
	/*Added by xukai. 20191125. end*/
	//return 0;
}
static CLASS_ATTR_RO(lc_gpio_r);

static ssize_t lc_gpio_w_store(struct class *class, struct class_attribute *attr, const char *buf, size_t len)
{
	printk(KERN_CRIT"####%s####\nval=%s\n", __func__, buf);
	if(0 == strncmp(buf, "1", 1)){
		printk(KERN_CRIT"########disable usb charge!########\n");
		gpio_direction_output(longcheer_gpio_ctl->usb_protect_en_pin,1);
		/*Added by xukai. 20191125. start*/
		//read_usb_protect_gpio_status = 1;
		/*Added by xukai. 20191125. end*/
	} else {
		printk(KERN_CRIT"########enable usb charge!########\n");
		gpio_direction_output(longcheer_gpio_ctl->usb_protect_en_pin,0);
		/*Added by xukai. 20191125. start*/
		//read_usb_protect_gpio_status = 0;
		/*Added by xukai. 20191125. end*/
	}
	return len;
}
static CLASS_ATTR_WO(lc_gpio_w);

/* Add by xukai. 20191214. start */
void smb_charging_ctl(int val)
{
	switch(val){
		case 0:
			printk(KERN_CRIT"########disable usb charge!########\n");
			gpio_direction_output(longcheer_gpio_ctl->usb_protect_en_pin,1);
			usb_charging_status = 0;//Add by xukai. For status.
		break;
		case 1:
			printk(KERN_CRIT"########enable usb charge!########\n");
			gpio_direction_output(longcheer_gpio_ctl->usb_protect_en_pin,0);
			usb_charging_status = 1;//Add by xukai. For status.
		break;
		default:
			printk(KERN_CRIT "----%s, val is invalid----\n", val);
	}
	return;
}
/* Add by xukai. 20191214. end*/

static struct attribute * lc_usb_gpio_class_attrs[] = {
	&class_attr_lc_gpio_r.attr,
	&class_attr_lc_gpio_w.attr,
	NULL,
};
ATTRIBUTE_GROUPS(lc_usb_gpio_class);


static struct class lc_usb_gpio_class = {
	.name	= "lc_usb_protect_ctl",
	.owner	= THIS_MODULE,
	.class_groups	= lc_usb_gpio_class_groups,	 
};

static int longcheer_probe(struct platform_device *dev)
{	
#ifdef USE_WILLIAM_CODE
	int ret_regu;
#endif
	int err = 0;


#ifdef LONGCHEER_DEBUG
	printk(KERN_CRIT"########GPIO %s########\n", __func__);
#endif
	longcheer_gpio_ctl = devm_kzalloc(&dev->dev, sizeof(struct longcheer_gpio_pin), GFP_KERNEL);
	if (longcheer_gpio_ctl == NULL) {
		err = -ENOMEM;
		dev_err(&dev->dev,"failed to allocate memory %d\n", err);
		goto exit;
	}

	dev_set_drvdata(&dev->dev, longcheer_gpio_ctl);

	if (dev->dev.of_node) 
	{
		err = longcheer_parse_dt(&dev->dev, longcheer_gpio_ctl);
		if (err < 0) {
			dev_err(&dev->dev, "Failed to parse device tree\n");
			goto exit;
		}
	} 	
	else 
	{
		dev_err(&dev->dev, "%s No valid platform data.\n", __func__);
		err = -ENODEV;
		goto exit;
	}

	err = class_register(&lc_usb_gpio_class);
	if (err < 0){
		printk("------%s------class register fail------\n", __func__);
		return err;
	}

	return 0;

exit:
	return err;
}

static int longcheer_driver_remove(struct platform_device *dev)
{
	gpio_free(longcheer_gpio_ctl->usb_protect_en_pin);

	return 0;
}


/***********************************add by william*****************************************/
#ifdef USE_WILLIAM_CODE
static int longcheer_runtime_suspend(struct device *dev)
{
#ifdef LONGCHEER_DEBUG
	printk(KERN_WARNING"########LONGCHEER %s########\n", __func__);
#endif
	return 0;
}

static int longcheer_runtime_resume(struct device *dev)
{
#ifdef LONGCHEER_DEBUG
	printk(KERN_WARNING"########LONGCHEER %s########\n", __func__);
#endif
	return 0;
}

static int longcheer_runtime_idle(struct device *dev)
{
#ifdef LONGCHEER_DEBUG
	printk(KERN_WARNING"########LONGCHEER %s########\n", __func__);
#endif
	return 0;
}

static int longcheer_suspend(struct device *dev)
{
#ifdef LONGCHEER_DEBUG
	printk(KERN_CRIT"########LONGCHEER %s########\n", __func__);
#endif

	//gpio_direction_output(longcheer_gpio_ctl->usb_protect_en_pin,0);

	return 0;
}

static int longcheer_resume(struct device *dev)
{
	int ret_regu;
#ifdef LONGCHEER_DEBUG
	printk(KERN_CRIT"########LONGCHEER %s########\n", __func__);
#endif
	//gpio_direction_output(longcheer_gpio_ctl->usb_protect_en_pin,1);

	return 0;
}

static const struct dev_pm_ops LONGCHEER_pm_ops =
{
	.runtime_suspend = longcheer_runtime_suspend,
	.runtime_resume  = longcheer_runtime_resume,
	.runtime_idle    = longcheer_runtime_idle,
	.suspend		=  longcheer_suspend,
	.resume			=  longcheer_resume,
};
#endif
/******************************************************************************************/


#ifdef CONFIG_OF
static struct of_device_id gpioctl_match_table[] = {
	{.compatible = "longcheer,gpio_ctl", },
	{ },
};
#endif

static struct platform_driver longcheer_gpio_driver = {
	.driver = {
		.name = "LONGCHEER_GPIO_DRV",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gpioctl_match_table),
#ifdef USE_WILLIAM_CODE
		.pm	= &LONGCHEER_pm_ops,
#endif
	},
	.probe = longcheer_probe,
	.remove = longcheer_driver_remove,
};

static int __init gpioctl_init(void)
{
	return platform_driver_register(&longcheer_gpio_driver);
}

static void __exit gpioctl_exit(void)
{
	platform_driver_unregister(&longcheer_gpio_driver);
}

module_init(gpioctl_init);
module_exit(gpioctl_exit);
MODULE_DESCRIPTION("longcheer ctl driver");
MODULE_AUTHOR("WILLIAM");
MODULE_LICENSE("GPL v2");

