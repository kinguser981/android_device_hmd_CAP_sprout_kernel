/*
 * Copyright (C) 2011 ST-Ericsson SA.
 * Copyright (C) 2009 Motorola, Inc.
 *
 * License Terms: GNU General Public License v2
 *
 * Simple driver for National Semiconductor sgm Backlight driver chip
 */
	
#include "ktd3136.h"
#include "ti_reg_22.h"
#include "ti_reg_23.h"


void lm3697_reg_init(void){
	i2c_sgm_write(bkl_chip->client,0x10, 0x03);
	i2c_sgm_write(bkl_chip->client,0x13, 0x21); //smooth effect
	i2c_sgm_write(bkl_chip->client,0x16, 0x01); //linear enable
	i2c_sgm_write(bkl_chip->client,0x17, 0x19);
	i2c_sgm_write(bkl_chip->client,0x18, 0x19);
	i2c_sgm_write(bkl_chip->client,0x19, 0x03);
	i2c_sgm_write(bkl_chip->client,0x1A, 0x0C);
	i2c_sgm_write(bkl_chip->client,0x1C, 0x00); //disable PWM control
	i2c_sgm_write(bkl_chip->client,0x22, 0x07&1637);
	i2c_sgm_write(bkl_chip->client,0x23, 0xff&(1637>>3));
	i2c_sgm_write(bkl_chip->client,0x24, 0x02);
}

int lm3697_brightness_set(int brightness)
{
	int err;

	if (brightness > MAX_BRIGHTNESS)
		brightness = MAX_BRIGHTNESS;
	
	err = i2c_sgm_write(bkl_chip->client, 0x22, brightness&0x07);
	err = i2c_sgm_write(bkl_chip->client, 0x23, (brightness>>3)&0xFF);

	if(err < 0)
		pr_info("lm3697 set Backlight fail !\n");
	return err;

}

int lm3697_hbm_set(int hbm_mode)
{

	switch (hbm_mode) {
	case HBM_MODE_DEFAULT:
		i2c_sgm_write(bkl_chip->client,0x18, 0x10);
		break;
	case HBM_MODE_LEVEL1:
		i2c_sgm_write(bkl_chip->client,0x18, 0x13);
		break;
	case HBM_MODE_LEVEL2:
		i2c_sgm_write(bkl_chip->client,0x18, 0x16);
		break;
	case HBM_MODE_LEVEL3:
		i2c_sgm_write(bkl_chip->client,0x18, 0x19);
		break;
	default:
		pr_info("This isn't hbm mode\n");
		break;
	 }

	return  0;
}

const static uint8_t reg_list[] = {0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x16, 0x17,
                                   0x18, 0x19, 0x1A, 0x1C, 0x22, 0x23, 0x24, 0xFF};

Backlight_I2C_Control lcdbl_lm3697 = {
	.id             = 0x00,
	.ic             = "lm3697",
	.reg_list       = (uint8_t*)reg_list,
	.init           = lm3697_reg_init,
	.set_brightness = lm3697_brightness_set,
	.hbm_set        = lm3697_hbm_set
};

