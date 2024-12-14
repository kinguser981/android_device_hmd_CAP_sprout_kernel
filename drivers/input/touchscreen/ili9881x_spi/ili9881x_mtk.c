/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "ili9881x.h"
#include "tpd.h"

#define DTS_INT_GPIO	"touch,irq-gpio"
#define DTS_RESET_GPIO	"touch,reset-gpio"
#define DTS_OF_NAME	"mediatek,cap_touch"
#define MTK_RST_GPIO	GTP_RST_PORT
#define MTK_INT_GPIO	GTP_INT_PORT

extern struct tpd_device *tpd;

void ili_tp_reset(void)
{
	ILI_INFO("edge delay = %d\n", ili9881x_ts->rst_edge_delay);

	/* Need accurate power sequence, do not change it to msleep */
	tpd_gpio_output(ili9881x_ts->tp_rst, 1);
	mdelay(1);
	tpd_gpio_output(ili9881x_ts->tp_rst, 0);
	mdelay(5);
	tpd_gpio_output(ili9881x_ts->tp_rst, 1);
	mdelay(ili9881x_ts->rst_edge_delay);
}

void ili_input_register(void)
{
	int i;

	ili9881x_ts->input = tpd->dev;

	if (tpd_dts_data.use_tpd_button) {
		for (i = 0; i < tpd_dts_data.tpd_key_num; i++)
			input_set_capability(ili9881x_ts->input, EV_KEY, tpd_dts_data.tpd_key_local[i]);
	}

	/* set the supported event type for input device */
	set_bit(EV_ABS, ili9881x_ts->input->evbit);
	set_bit(EV_SYN, ili9881x_ts->input->evbit);
	set_bit(EV_KEY, ili9881x_ts->input->evbit);
	set_bit(BTN_TOUCH, ili9881x_ts->input->keybit);
	set_bit(BTN_TOOL_FINGER, ili9881x_ts->input->keybit);
	set_bit(INPUT_PROP_DIRECT, ili9881x_ts->input->propbit);

	if (MT_PRESSURE)
		input_set_abs_params(ili9881x_ts->input, ABS_MT_PRESSURE, 0, 255, 0, 0);

	if (MT_B_TYPE) {
#if KERNEL_VERSION(3, 7, 0) <= LINUX_VERSION_CODE
		input_mt_init_slots(ili9881x_ts->input, MAX_TOUCH_NUM, INPUT_MT_DIRECT);
#else
		input_mt_init_slots(ili9881x_ts->input, MAX_TOUCH_NUM);
#endif /* LINUX_VERSION_CODE */
	} else {
		input_set_abs_params(ili9881x_ts->input, ABS_MT_TRACKING_ID, 0, MAX_TOUCH_NUM, 0, 0);
	}

	/* Gesture keys register */
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_POWER);
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_GESTURE_UP);
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_GESTURE_DOWN);
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_GESTURE_LEFT);
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_GESTURE_RIGHT);
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_GESTURE_O);
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_GESTURE_E);
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_GESTURE_M);
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_GESTURE_W);
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_GESTURE_S);
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_GESTURE_V);
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_GESTURE_Z);
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_GESTURE_C);
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_GESTURE_F);

	__set_bit(KEY_GESTURE_POWER, ili9881x_ts->input->keybit);
	__set_bit(KEY_GESTURE_UP, ili9881x_ts->input->keybit);
	__set_bit(KEY_GESTURE_DOWN, ili9881x_ts->input->keybit);
	__set_bit(KEY_GESTURE_LEFT, ili9881x_ts->input->keybit);
	__set_bit(KEY_GESTURE_RIGHT, ili9881x_ts->input->keybit);
	__set_bit(KEY_GESTURE_O, ili9881x_ts->input->keybit);
	__set_bit(KEY_GESTURE_E, ili9881x_ts->input->keybit);
	__set_bit(KEY_GESTURE_M, ili9881x_ts->input->keybit);
	__set_bit(KEY_GESTURE_W, ili9881x_ts->input->keybit);
	__set_bit(KEY_GESTURE_S, ili9881x_ts->input->keybit);
	__set_bit(KEY_GESTURE_V, ili9881x_ts->input->keybit);
	__set_bit(KEY_GESTURE_Z, ili9881x_ts->input->keybit);
	__set_bit(KEY_GESTURE_C, ili9881x_ts->input->keybit);
	__set_bit(KEY_GESTURE_F, ili9881x_ts->input->keybit);
}

#if REGULATOR_POWER
void ili_plat_regulator_power_on(bool status)
{
	ILI_INFO("%s\n", status ? "POWER ON" : "POWER OFF");

	if (status) {
		if (ili9881x_ts->vdd) {
			if (regulator_enable(ili9881x_ts->vdd) < 0)
				ILI_ERR("regulator_enable VDD fail\n");
		}
		if (ili9881x_ts->vcc) {
			if (regulator_enable(ili9881x_ts->vcc) < 0)
				ILI_ERR("regulator_enable VCC fail\n");
		}
	} else {
		if (ili9881x_ts->vdd) {
			if (regulator_disable(ili9881x_ts->vdd) < 0)
				ILI_ERR("regulator_enable VDD fail\n");
		}
		if (ili9881x_ts->vcc) {
			if (regulator_disable(ili9881x_ts->vcc) < 0)
				ILI_ERR("regulator_enable VCC fail\n");
		}
	}
	atomic_set(&ili9881x_ts->ice_stat, DISABLE);
	mdelay(5);
}
#endif

#if REGULATOR_POWER
static void ilitek_plat_regulator_power_init(void)
{
	const char *vdd_name = "vdd";
	const char *vcc_name = "vcc";

	ili9881x_ts->vdd = regulator_get(tpd->tpd_dev, vdd_name);
	if (ERR_ALLOC_MEM(ili9881x_ts->vdd)) {
		ILI_ERR("regulator_get VDD fail\n");
		ili9881x_ts->vdd = NULL;
	}

	tpd->reg = ili9881x_ts->vdd;

	if (regulator_set_voltage(ili9881x_ts->vdd, VDD_VOLTAGE, VDD_VOLTAGE) < 0)
		ILI_ERR("Failed to set VDD %d\n", VDD_VOLTAGE);

	ili9881x_ts->vcc = regulator_get(ili9881x_ts->dev, vcc_name);
	if (ERR_ALLOC_MEM(ili9881x_ts->vcc)) {
		ILI_ERR("regulator_get VCC fail.\n");
		ili9881x_ts->vcc = NULL;
	}
	if (regulator_set_voltage(ili9881x_ts->vcc, VCC_VOLTAGE, VCC_VOLTAGE) < 0)
		ILI_ERR("Failed to set VCC %d\n", VCC_VOLTAGE);

	ili_plat_regulator_power_on(true);
}
#endif

static int ilitek_plat_gpio_register(void)
{
	int ret = 0;

	ili9881x_ts->tp_int = MTK_INT_GPIO;
	ili9881x_ts->tp_rst = MTK_RST_GPIO;

	ILI_INFO("TP INT: %d\n", ili9881x_ts->tp_int);
	ILI_INFO("TP RESET: %d\n", ili9881x_ts->tp_rst);

	if (!gpio_is_valid(ili9881x_ts->tp_int)) {
		ILI_ERR("Invalid INT gpio: %d\n", ili9881x_ts->tp_int);
		return -EBADR;
	}

	if (!gpio_is_valid(ili9881x_ts->tp_rst)) {
		ILI_ERR("Invalid RESET gpio: %d\n", ili9881x_ts->tp_rst);
		return -EBADR;
	}

	ret = gpio_request(ili9881x_ts->tp_int, "TP_INT");
	if (ret < 0) {
		ILI_ERR("Request IRQ GPIO failed, ret = %d\n", ret);
		gpio_free(ili9881x_ts->tp_int);
		ret = gpio_request(ili9881x_ts->tp_int, "TP_INT");
		if (ret < 0) {
			ILI_ERR("Retrying request INT GPIO still failed , ret = %d\n", ret);
			goto out;
		}
	}

	ret = gpio_request(ili9881x_ts->tp_rst, "TP_RESET");
	if (ret < 0) {
		ILI_ERR("Request RESET GPIO failed, ret = %d\n", ret);
		gpio_free(ili9881x_ts->tp_rst);
		ret = gpio_request(ili9881x_ts->tp_rst, "TP_RESET");
		if (ret < 0) {
			ILI_ERR("Retrying request RESET GPIO still failed , ret = %d\n", ret);
			goto out;
		}
	}

out:
	gpio_direction_input(ili9881x_ts->tp_int);
	return ret;
}

void ili_irq_disable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&ili9881x_ts->irq_spin, flag);

	if (atomic_read(&ili9881x_ts->irq_stat) == DISABLE)
		goto out;

	if (!ili9881x_ts->irq_num) {
		ILI_ERR("gpio_to_irq (%d) is incorrect\n", ili9881x_ts->irq_num);
		goto out;
	}

	disable_irq_nosync(ili9881x_ts->irq_num);
	atomic_set(&ili9881x_ts->irq_stat, DISABLE);
	ILI_DBG("Disable irq success\n");

out:
	spin_unlock_irqrestore(&ili9881x_ts->irq_spin, flag);
}

void ili_irq_enable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&ili9881x_ts->irq_spin, flag);

	if (atomic_read(&ili9881x_ts->irq_stat) == ENABLE)
		goto out;

	if (!ili9881x_ts->irq_num) {
		ILI_ERR("gpio_to_irq (%d) is incorrect\n", ili9881x_ts->irq_num);
		goto out;
	}

	enable_irq(ili9881x_ts->irq_num);
	atomic_set(&ili9881x_ts->irq_stat, ENABLE);
	ILI_DBG("Enable irq success\n");

out:
	spin_unlock_irqrestore(&ili9881x_ts->irq_spin, flag);
}

static irqreturn_t ilitek_plat_isr_top_half(int irq, void *dev_id)
{
	if (irq != ili9881x_ts->irq_num) {
		ILI_ERR("Incorrect irq number (%d)\n", irq);
		return IRQ_NONE;
	}

	if (atomic_read(&ili9881x_ts->cmd_int_check) == ENABLE) {
		atomic_set(&ili9881x_ts->cmd_int_check, DISABLE);
		ILI_DBG("interrupt for cmd, ignore\n");
		wake_up(&(ili9881x_ts->inq));
		return IRQ_HANDLED;
	}

	if (ili9881x_ts->prox_near) {
		ILI_INFO("Proximity event, ignore interrupt!\n");
		return IRQ_HANDLED;
	}

	ILI_DBG("report: %d, rst: %d, fw: %d, switch: %d, mp: %d, sleep: %d, esd: %d\n",
			ili9881x_ts->report,
			atomic_read(&ili9881x_ts->tp_reset),
			atomic_read(&ili9881x_ts->fw_stat),
			atomic_read(&ili9881x_ts->tp_sw_mode),
			atomic_read(&ili9881x_ts->mp_stat),
			atomic_read(&ili9881x_ts->tp_sleep),
			atomic_read(&ili9881x_ts->esd_stat));

	if (!ili9881x_ts->report || atomic_read(&ili9881x_ts->tp_reset) ||
		atomic_read(&ili9881x_ts->fw_stat) || atomic_read(&ili9881x_ts->tp_sw_mode) ||
		atomic_read(&ili9881x_ts->mp_stat) || atomic_read(&ili9881x_ts->tp_sleep) ||
		atomic_read(&ili9881x_ts->esd_stat)) {
			ILI_DBG("ignore interrupt !\n");
			return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t ilitek_plat_isr_bottom_half(int irq, void *dev_id)
{
	if (mutex_is_locked(&ili9881x_ts->touch_mutex)) {
		ILI_DBG("touch is locked, ignore\n");
		return IRQ_HANDLED;
	}
	mutex_lock(&ili9881x_ts->touch_mutex);
	ili_report_handler();
	mutex_unlock(&ili9881x_ts->touch_mutex);
	return IRQ_HANDLED;
}

void ili_irq_unregister(void)
{
	devm_free_irq(ili9881x_ts->dev, ili9881x_ts->irq_num, NULL);
}

int ili_irq_register(int type)
{
	int ret = 0;
	static bool get_irq_pin;
	struct device_node *node;

	atomic_set(&ili9881x_ts->irq_stat, DISABLE);

	if (get_irq_pin == false) {
		node = of_find_matching_node(NULL, touch_of_match);
		if (node)
			ili9881x_ts->irq_num = irq_of_parse_and_map(node, 0);

		ILI_INFO("ili9881x_ts->irq_num = %d\n", ili9881x_ts->irq_num);
		get_irq_pin = true;
	}

	ret = devm_request_threaded_irq(ili9881x_ts->dev, ili9881x_ts->irq_num,
				ilitek_plat_isr_top_half,
				ilitek_plat_isr_bottom_half,
				type | IRQF_ONESHOT, "ilitek", NULL);

	if (type == IRQF_TRIGGER_FALLING)
		ILI_INFO("IRQ TYPE = IRQF_TRIGGER_FALLING\n");
	if (type == IRQF_TRIGGER_RISING)
		ILI_INFO("IRQ TYPE = IRQF_TRIGGER_RISING\n");

	if (ret != 0)
		ILI_ERR("Failed to register irq handler, irq = %d, ret = %d\n", ili9881x_ts->irq_num, ret);

	atomic_set(&ili9881x_ts->irq_stat, ENABLE);

	return ret;
}

static void tpd_resume(struct device *h)
{
	if (ili_sleep_handler(TP_RESUME) < 0)
		ILI_ERR("TP resume failed\n");
}

static void tpd_suspend(struct device *h)
{
	if (ili_sleep_handler(TP_SUSPEND) < 0)
		ILI_ERR("TP suspend failed\n");
}

static int ilitek_plat_probe(void)
{
	ILI_INFO("platform probe\n");

#if REGULATOR_POWER
	ilitek_plat_regulator_power_init();
#endif

	if (ilitek_plat_gpio_register() < 0)
		ILI_ERR("Register gpio failed\n");

	ili_irq_register(ili9881x_ts->irq_tirgger_type);

	if (ili_tddi_init() < 0) {
		ILI_ERR("platform probe failed\n");
		ili_irq_unregister();
		return -ENODEV;
	}
	tpd_load_status = 1;
	return 0;
}

static int ilitek_plat_remove(void)
{
	ILI_INFO();
	ili_dev_remove();
	return 0;
}

static const struct of_device_id tp_match_table[] = {
	{.compatible = DTS_OF_NAME},
	{},
};

static struct ilitek_hwif_info hwif = {
	.bus_type = TDDI_INTERFACE,
	.plat_type = TP_PLAT_MTK,
	.owner = THIS_MODULE,
	.name = TDDI_DEV_ID,
	.of_match_table = of_match_ptr(tp_match_table),
	.plat_probe = ilitek_plat_probe,
	.plat_remove = ilitek_plat_remove,
};

static int tpd_local_init(void)
{
	ILI_INFO("TPD init device driver\n");

	if (ili_dev_init(&hwif) < 0) {
		ILI_ERR("Failed to register i2c/spi bus driver\n");
		return -ENODEV;
	}
	if (tpd_load_status == 0) {
		ILI_ERR("Add error touch panel driver\n");
		return -1;
	}
	if (tpd_dts_data.use_tpd_button) {
		tpd_button_setting(tpd_dts_data.tpd_key_num, tpd_dts_data.tpd_key_local,
				   tpd_dts_data.tpd_key_dim_local);
	}
	tpd_type_cap = 1;
	return 0;
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = TDDI_DEV_ID,
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
};

static int __init ilitek_plat_dev_init(void)
{
	int ret = 0;

	ILI_INFO("ILITEK TP driver init for MTK\n");
	tpd_get_dts_info();
	ret = tpd_driver_add(&tpd_device_driver);
	if (ret < 0) {
		ILI_ERR("ILITEK add TP driver failed\n");
		tpd_driver_remove(&tpd_device_driver);
		return -ENODEV;
	}
	return 0;
}

static void __exit ilitek_plat_dev_exit(void)
{
	ILI_INFO("ilitek driver has been removed\n");
	tpd_driver_remove(&tpd_device_driver);
}

module_init(ilitek_plat_dev_init);
module_exit(ilitek_plat_dev_exit);
MODULE_AUTHOR("ILITEK");
MODULE_LICENSE("GPL");
