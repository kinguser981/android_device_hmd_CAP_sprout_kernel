/*
 * Copyright (C) 2011 ST-Ericsson SA.
 * Copyright (C) 2009 Motorola, Inc.
 *
 * License Terms: GNU General Public License v2
 *
 * Simple driver for National Semiconductor sgm Backlight driver chip
 *
 * Author: Shreshtha Kumar SAHU <shreshthakumar.sahu@stericsson.com>
 * based on leds-sgm.c by Dan Murphy <D.Murphy@motorola.com>
 */

#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/module.h>

#include "ktd3136.h"

#define SGM_NAME "lcdbl_i2c"

struct ktd3137_chip *bkl_chip;
static Backlight_I2C_Control *control = NULL;

int ktd3137_read_reg(struct i2c_client *client, int reg, u8 *val)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	*val = ret;

	return ret;
}

int i2c_sgm_write(struct i2c_client *client, uint8_t command, uint8_t data)
{
	int retry/*, loop_i*/;
	uint8_t buf[1 + 1];
	uint8_t toRetry = 5;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1 + 1,
			.buf = buf,
		}
	};

	buf[0] = command;
	buf[1] = data;
	
	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		//msleep(20);
	}

	if (retry == toRetry) {
		printk("%s: i2c_write_block retry over %d\n",
			__func__, toRetry);
		return -EIO;
	}
	return 0;

}

int dsi_panel_update_ext_backlight(int brightness)
{
	if(control){
		pr_info("[brightness]%s brightness = %d\n", __func__, brightness);
		control->set_brightness(brightness);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(dsi_panel_update_ext_backlight);


int dsi_panel_update_full_scale_cur(int hbm_mode)
{
	if(control){
		pr_info("%s hbm mode = %d\n", __func__, hbm_mode);
		control->hbm_set(hbm_mode);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(dsi_panel_update_full_scale_cur);

static ssize_t sgm_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", control?control->ic:"NULL");
}

static ssize_t sgm_brightness_store(struct device *dev, struct device_attribute
				   *attr, const char *buf, size_t size)
{

	int len;
	char buf_bri[6];
	int  brightness = 0;

	len = (int)strlen(buf);
	if (len > 5)
		return -1;
	memcpy(buf_bri, buf, len);
	buf_bri[len] = '\0';
	sscanf(buf_bri, "%d", &brightness);

	dsi_panel_update_ext_backlight(brightness);
   
	return size;
}
static DEVICE_ATTR(chip, 0644, sgm_brightness_show, sgm_brightness_store);


static ssize_t sgm_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;
	uint8_t val;
	uint8_t *reg_list = NULL; 
	char *offset = buf;

	if(control){reg_list = control->reg_list;}

	for(i = 0; reg_list[i] != 0xFF; i++){
		val = 0;
		if(ktd3137_read_reg(bkl_chip->client, reg_list[i], &val) < 0){
			sprintf(offset, "%02x --\n", reg_list[i]);
		}else{
			sprintf(offset, "%02x %02x\n", reg_list[i], val);
		}
		offset = offset+strlen(offset);
	}

	
	return strlen(buf)+1;
}

static ssize_t sgm_reg_store(struct device *dev, struct device_attribute
				   *attr, const char *buf, size_t size)
{
	uint8_t reg, val;

	sscanf(buf, "%x %x", &reg, &val);
	pr_info("write_reg, reg: %02x, val:%02x", reg, val);
	i2c_sgm_write(bkl_chip->client, reg, val);
   
	return size;
}
				   
static DEVICE_ATTR(reg, 0644, sgm_reg_show, sgm_reg_store);


static struct attribute *sgm_attrs[] = {
	&dev_attr_chip.attr,
	&dev_attr_reg.attr,
	NULL
};
static struct attribute_group sgm_attribute_group = {
	.attrs = sgm_attrs
};

extern Backlight_I2C_Control lcdbl_lm3697;
extern Backlight_I2C_Control lcdbl_ktd3137;
extern Backlight_I2C_Control lcdbl_aw99703;
static int sgm_detect_ic(void)
{
	int ret = 0;
	u8 rbuf = 2;

	ret = ktd3137_read_reg(bkl_chip->client,0x00, &rbuf);
	pr_info("%s: 0x00 reg = 0x%x, ret = %d\n",__func__, rbuf, ret);
	if(ret<0){
		return ret;
	}

	do{
#ifdef CONFIG_BACKLIGHT_KTD3136
		if(rbuf == lcdbl_ktd3137.id){
			pr_info("%s: i2c_lcdbl is KTD3137\n");
			control = &lcdbl_ktd3137;
			break;
		}
#endif

#ifdef CONFIG_BACKLIGHT_AW99703
		if(rbuf == lcdbl_aw99703.id){
			pr_info("%s: i2c_lcdbl is AW99703\n");
			control = &lcdbl_aw99703;
			break;
		}
#endif
		
#ifdef CONFIG_BACKLIGHT_LM3697
		pr_info("%s: i2c_lcdbl default LM3697\n");
		control = &lcdbl_lm3697;
#endif
	}while(0);

	return 0;
}

static int sgm_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	int err = 0;

	struct ktd3137_bl_pdata *pdata = dev_get_drvdata(&client->dev);
	struct ktd3137_chip *chip;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C_FUNC_I2C not supported\n");
		return -EIO;
	}

	client->addr = 0x36;
	pr_err("probe start! 1 \n");
	chip = devm_kzalloc(&client->dev, sizeof(struct ktd3137_chip),
				GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;
	 
	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata){
		pr_err("probe start! 2 \n");
		return -1;// -ENOMEM;
	}
	pdata->linear_backlight = 1;
	pdata->default_brightness = 1637;
	pdata->max_brightness = 0x7ff;

	chip->client = client;
	chip->pdata = pdata;
	chip->dev = &client->dev;

	bkl_chip = chip;
	i2c_set_clientdata(client, chip);
	err = sysfs_create_group(&client->dev.kobj, &sgm_attribute_group);
	
	if(sgm_detect_ic()){
		pr_err("probe err = %d\n", err);
		return -1;
	}

	if(control){control->init();}

	return 0;
}

static const struct i2c_device_id sgm_id[] = {
	{SGM_NAME, 0},
	{}
};

static const struct of_device_id lm3697_bl_of_match[] = {
	{ .compatible = "lct,lcdbl_i2c", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sgm_id);

static struct i2c_driver sgm_i2c_driver = {
	.probe = sgm_probe,
	.id_table = sgm_id,
	.driver = {
		.name = SGM_NAME,
		.owner = THIS_MODULE,
		.of_match_table = lm3697_bl_of_match,
	},
};

module_i2c_driver(sgm_i2c_driver);

MODULE_DESCRIPTION("Back Light driver for sgm");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Shreshtha Kumar SAHU <shreshthakumar.sahu@stericsson.com>");
