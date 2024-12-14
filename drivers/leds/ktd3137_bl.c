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

#include "ktd3136.h"
#include "ktd_reg_04.h"
#include "ktd_reg_05.h"

static int ktd3137_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int ktd3137_masked_write(struct i2c_client *client,
					int reg, u8 mask, u8 val)
{
	int rc;
	u8 temp = 0;

	rc = ktd3137_read_reg(client, reg, &temp);
	if (rc < 0) {
		dev_err(&client->dev, "failed to read reg\n");
	} else {
		temp &= ~mask;
		temp |= val & mask;
		rc = ktd3137_write_reg(client, reg, temp);
		if (rc < 0)
			dev_err(&client->dev, "failed to write masked data\n");
	}

	return rc;
}

void kt3136_reg_init(void){
	i2c_sgm_write(bkl_chip->client,0x06, 0x1B);
	i2c_sgm_write(bkl_chip->client,0x02, 0xc9);
}

int ktd3137_brightness_set(int brightness)
{
	struct ktd3137_bl_pdata *pdata = bkl_chip->pdata;

	if (brightness > pdata->max_brightness)
		brightness = pdata->max_brightness;

	if((pdata->prev_bl_current == 0)&&(brightness != 0)){
		if (pdata->linear_backlight == 1) {
			ktd3137_masked_write(bkl_chip->client, REG_CONTROL, 0x02, 0x02);// set linear mode
		}else{
			ktd3137_masked_write(bkl_chip->client, REG_CONTROL, 0x02, 0x00);// set exponetial mode
		}
	}
	
	if (brightness == 0) {
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0x98);
	} else {
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0xC9);

	if (pdata->using_lsb) {
		ktd3137_masked_write(bkl_chip->client, REG_RATIO_LSB,
						0x07, brightness);
		ktd3137_masked_write(bkl_chip->client, REG_RATIO_MSB,
						0xff, brightness>>3);
	} else {
		ktd3137_masked_write(bkl_chip->client, REG_RATIO_LSB, 0x07,
			ktd3137_brightness_table_reg4[brightness]);
		ktd3137_masked_write(bkl_chip->client, REG_RATIO_MSB, 0xff,
			ktd3137_brightness_table_reg5[brightness]);
		}
	}

	pdata->prev_bl_current = brightness;

	return 0;
}

int ktd_hbm_set(int hbm_mode)
{
	switch (hbm_mode) {
	case HBM_MODE_DEFAULT:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0x81);
		i2c_sgm_write(bkl_chip->client,0x8, 0x3);
		break;
	case HBM_MODE_LEVEL1:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0x99);
		i2c_sgm_write(bkl_chip->client,0x8, 0x3);
		break;
	case HBM_MODE_LEVEL2:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0xB1);
		i2c_sgm_write(bkl_chip->client,0x8, 0x3);
		break;
	case HBM_MODE_LEVEL3:
		ktd3137_write_reg(bkl_chip->client, REG_MODE, 0xC9);
		i2c_sgm_write(bkl_chip->client,0x8, 0x3);
		break;
	default:
		pr_info("This isn't hbm mode\n");
		break;
	 }

	return 0;
}

const static uint8_t reg_list[] = {0xFF};

Backlight_I2C_Control lcdbl_ktd3137 = {
	.id             = 0x18,
	.ic             = "ktd3137",
	.reg_list       = (uint8_t*)reg_list,
	.init           = kt3136_reg_init,
	.set_brightness = ktd3137_brightness_set,
	.hbm_set        = ktd_hbm_set
};

