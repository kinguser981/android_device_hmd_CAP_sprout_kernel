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

#define DTS_INT_GPIO	"touch,irq-gpio"
#define DTS_RESET_GPIO	"touch,reset-gpio"
#define DTS_OF_NAME	"tchip,ilitek"

void ili_tp_reset(void)
{
	ILI_INFO("edge delay = %d\n", ili9881x_ts->rst_edge_delay);

	/* Need accurate power sequence, do not change it to msleep */
	gpio_direction_output(ili9881x_ts->tp_rst, 1);
	mdelay(1);
	gpio_set_value(ili9881x_ts->tp_rst, 0);
	mdelay(5);
	gpio_set_value(ili9881x_ts->tp_rst, 1);
	mdelay(ili9881x_ts->rst_edge_delay);
}

void ili_input_register(void)
{
	ILI_INFO();

	ili9881x_ts->input = input_allocate_device();
	if (ERR_ALLOC_MEM(ili9881x_ts->input)) {
		ILI_ERR("Failed to allocate touch input device\n");
		input_free_device(ili9881x_ts->input);
		return;
	}

	ili9881x_ts->input->name = ili9881x_ts->hwif->name;
	ili9881x_ts->input->phys = ili9881x_ts->phys;
	ili9881x_ts->input->dev.parent = ili9881x_ts->dev;
	ili9881x_ts->input->id.bustype = ili9881x_ts->hwif->bus_type;

	/* set the supported event type for input device */
	set_bit(EV_ABS, ili9881x_ts->input->evbit);
	set_bit(EV_SYN, ili9881x_ts->input->evbit);
	set_bit(EV_KEY, ili9881x_ts->input->evbit);
	set_bit(BTN_TOUCH, ili9881x_ts->input->keybit);
	set_bit(BTN_TOOL_FINGER, ili9881x_ts->input->keybit);
	set_bit(INPUT_PROP_DIRECT, ili9881x_ts->input->propbit);

	input_set_abs_params(ili9881x_ts->input, ABS_MT_POSITION_X, TOUCH_SCREEN_X_MIN, ili9881x_ts->panel_wid - 1, 0, 0);
	input_set_abs_params(ili9881x_ts->input, ABS_MT_POSITION_Y, TOUCH_SCREEN_Y_MIN, ili9881x_ts->panel_hei - 1, 0, 0);
	input_set_abs_params(ili9881x_ts->input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ili9881x_ts->input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);

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
	input_set_capability(ili9881x_ts->input, EV_KEY, KEY_WAKEUP);
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

	__set_bit(KEY_WAKEUP, ili9881x_ts->input->keybit);
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

	/* register the input device to input sub-system */
	if (input_register_device(ili9881x_ts->input) < 0) {
		ILI_ERR("Failed to register touch input device\n");
		input_unregister_device(ili9881x_ts->input);
		input_free_device(ili9881x_ts->input);
	}
}

#if REGULATOR_POWER
/*******************************************************
Description:
	Novatek touchscreen driver get regulator function.

	with incell touchscreen and displayscreen regulator 
	must enable during device sleep
	enable regulator in probe. 
	if necessary, disable in suspend and enable in
	resume.

return:
	Executive outcomes. 0---succeed. negative---failed
*******************************************************/
static int32_t ilitek_plat_regulator_power_init(bool get)
{
	int32_t ret = 0;

	ILI_INFO("get/put regulator : %d \n", get);

	if (!get) {
		goto put_regulator;
	}

	ili9881x_ts->pwr_vdd = regulator_get(ili9881x_ts->dev, "touch_vddio");
	if (IS_ERR_OR_NULL(ili9881x_ts->pwr_vdd)) {
		ret = PTR_ERR(ili9881x_ts->pwr_vdd);
		ILI_ERR("Failed to get vdd regulator");
		goto put_regulator;
	} else {
        if (regulator_count_voltages(ili9881x_ts->pwr_vdd) > 0) {
            ret = regulator_set_voltage(ili9881x_ts->pwr_vdd, VDD_VOLTAGE, VDD_VOLTAGE);
            if (ret) {
                ILI_ERR("vddio regulator set_vtg failed,ret=%d", ret);
                goto put_regulator;
            }
        }
	}

	ili9881x_ts->pwr_lab = regulator_get(ili9881x_ts->dev, "touch_lab");
	if (IS_ERR_OR_NULL(ili9881x_ts->pwr_lab)) {
		ret = PTR_ERR(ili9881x_ts->pwr_lab);
		ILI_ERR("Failed to get lab regulator");
		goto put_regulator;
	}

	ili9881x_ts->pwr_ibb = regulator_get(ili9881x_ts->dev, "touch_ibb");
	if (IS_ERR_OR_NULL(ili9881x_ts->pwr_ibb)) {
		ret = PTR_ERR(ili9881x_ts->pwr_ibb);
		ILI_ERR("Failed to get ibb regulator");
		goto put_regulator;
	}

	return 0;

put_regulator:
	if (ili9881x_ts->pwr_vdd) {
		regulator_put(ili9881x_ts->pwr_vdd);
		ili9881x_ts->pwr_vdd = NULL;
	}

	if (ili9881x_ts->pwr_lab) {
		regulator_put(ili9881x_ts->pwr_lab);
		ili9881x_ts->pwr_lab = NULL;
	}

	if (ili9881x_ts->pwr_ibb) {
		regulator_put(ili9881x_ts->pwr_ibb);
		ili9881x_ts->pwr_ibb = NULL;
	}

	return ret;
}

/*******************************************************
Description:
	Novatek touchscreen driver enable regulator function.

return:
	Executive outcomes. 0---succeed. negative---failed
*******************************************************/
void ili_plat_regulator_power_on(bool en)
{
	static bool status = false;
	int32_t ret = 0;

	if (status == en) {
		ILI_INFO("Already %s touch regulator", en?"enable":"disable");
		return;
	}
	status = en;
	ILI_INFO("%s touch regulator", en?"enable":"disable");

	if (!en) {
		goto disable_ibb_regulator;
	}

	if (ili9881x_ts->pwr_vdd) {
		ret = regulator_enable(ili9881x_ts->pwr_vdd);
		if (ret < 0) {
			ILI_ERR("Failed to enable vdd regulator");
			goto exit;
		}
	}

	if (ili9881x_ts->pwr_lab) {
		ret = regulator_enable(ili9881x_ts->pwr_lab);
		if (ret < 0) {
			ILI_ERR("Failed to enable lab regulator");
			goto disable_vdd_regulator;
		}
	}

	if (ili9881x_ts->pwr_ibb) {
		ret = regulator_enable(ili9881x_ts->pwr_ibb);
		if (ret < 0) {
			ILI_ERR("Failed to enable ibb regulator");
			goto disable_lab_regulator;
		}
	}

	return;

disable_ibb_regulator:
	if (ili9881x_ts->pwr_ibb)
		regulator_disable(ili9881x_ts->pwr_ibb);

disable_lab_regulator:
	if (ili9881x_ts->pwr_lab)
		regulator_disable(ili9881x_ts->pwr_lab);

disable_vdd_regulator:
	if (ili9881x_ts->pwr_vdd)
		regulator_disable(ili9881x_ts->pwr_vdd);

exit:
	return;
}
#endif

static int ilitek_plat_gpio_register(void)
{
	int ret = 0;
	u32 flag;
	struct device_node *dev_node = ili9881x_ts->dev->of_node;

	ili9881x_ts->tp_int = of_get_named_gpio_flags(dev_node, DTS_INT_GPIO, 0, &flag);
	ili9881x_ts->tp_rst = of_get_named_gpio_flags(dev_node, DTS_RESET_GPIO, 0, &flag);

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

	if (ili9881x_ts->ignore_first_irq) {
		ILI_INFO("ignore first interrupt!\n");
		ili9881x_ts->ignore_first_irq = false;
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

	atomic_set(&ili9881x_ts->irq_stat, DISABLE);

	if (get_irq_pin == false) {
		ili9881x_ts->irq_num  = gpio_to_irq(ili9881x_ts->tp_int);
		get_irq_pin = true;
	}

	ILI_INFO("ili9881x_ts->irq_num = %d\n", ili9881x_ts->irq_num);

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

#if defined(CONFIG_FB) || defined(CONFIG_DRM_MSM)
static int ilitek_plat_notifier_fb(struct notifier_block *self, unsigned long event, void *data)
{
	int *blank;
	struct fb_event *evdata = data;

	ILI_INFO("Notifier's event = %ld\n", event);

	/*
	 *	FB_EVENT_BLANK(0x09): A hardware display blank change occurred.
	 *	FB_EARLY_EVENT_BLANK(0x10): A hardware display blank early change occurred.
	 */
	if (evdata && evdata->data) {
		blank = evdata->data;
		switch (*blank) {
#ifdef CONFIG_DRM_MSM
		case MSM_DRM_BLANK_POWERDOWN:
#else
		case FB_BLANK_POWERDOWN:
#endif
#if CONFIG_PLAT_SPRD
		case DRM_MODE_DPMS_OFF:
#endif /* CONFIG_PLAT_SPRD */
			if (TP_SUSPEND_PRIO) {
#ifdef CONFIG_DRM_MSM
				if (event != MSM_DRM_EARLY_EVENT_BLANK)
#else
				if (event != FB_EARLY_EVENT_BLANK)
#endif
					return NOTIFY_DONE;
			} else {
#ifdef CONFIG_DRM_MSM
				if (event != MSM_DRM_EVENT_BLANK)
#else
				if (event != FB_EVENT_BLANK)
#endif
					return NOTIFY_DONE;
			}
			if (ili_sleep_handler(TP_SUSPEND) < 0)
				ILI_ERR("TP suspend failed\n");
			break;
#ifdef CONFIG_DRM_MSM
		case MSM_DRM_BLANK_UNBLANK:
		//case MSM_DRM_BLANK_NORMAL:
#else
		case FB_BLANK_UNBLANK:
		case FB_BLANK_NORMAL:
#endif

#if CONFIG_PLAT_SPRD
		case DRM_MODE_DPMS_ON:
#endif /* CONFIG_PLAT_SPRD */

#ifdef CONFIG_DRM_MSM
			if (event == MSM_DRM_EVENT_BLANK)
#else
			if (event == FB_EVENT_BLANK)
#endif
			{
				if (ili_sleep_handler(TP_RESUME) < 0)
					ILI_ERR("TP resume failed\n");

			}
			break;
		default:
			ILI_ERR("Unknown event, blank = %d\n", *blank);
			break;
		}
	}
	return NOTIFY_OK;
}
#else
static void ilitek_plat_early_suspend(struct early_suspend *h)
{
	if (ili_sleep_handler(TP_SUSPEND) < 0)
		ILI_ERR("TP suspend failed\n");
}

static void ilitek_plat_late_resume(struct early_suspend *h)
{
	if (ili_sleep_handler(TP_RESUME) < 0)
		ILI_ERR("TP resume failed\n");
}
#endif

static void ilitek_plat_sleep_init(void)
{
#if defined(CONFIG_FB) || defined(CONFIG_DRM_MSM)
	ILI_INFO("Init notifier_fb struct\n");
	ili9881x_ts->notifier_fb.notifier_call = ilitek_plat_notifier_fb;
#if defined(CONFIG_DRM_MSM)
		if (msm_drm_register_client(&ili9881x_ts->notifier_fb)) {
			ILI_ERR("msm_drm_register_client Unable to register fb_notifier\n");
		}
#else
#if CONFIG_PLAT_SPRD
	if (adf_register_client(&ili9881x_ts->notifier_fb))
		ILI_ERR("Unable to register notifier_fb\n");
#else
	if (fb_register_client(&ili9881x_ts->notifier_fb))
		ILI_ERR("Unable to register notifier_fb\n");
#endif /* CONFIG_PLAT_SPRD */
#endif /* CONFIG_DRM_MSM */
#else
	ILI_INFO("Init eqarly_suspend struct\n");
	ili9881x_ts->early_suspend.suspend = ilitek_plat_early_suspend;
	ili9881x_ts->early_suspend.resume = ilitek_plat_late_resume;
	ili9881x_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	register_early_suspend(&ili9881x_ts->early_suspend);
#endif
}

static int ilitek_plat_probe(void)
{
	ILI_INFO("platform probe\n");

	if (ilitek_plat_gpio_register() < 0)
		ILI_ERR("Register gpio failed\n");

	if (ili_tddi_init() < 0) {
		ILI_ERR("platform probe failed\n");
		return -ENODEV;
	}

	ili_irq_register(ili9881x_ts->irq_tirgger_type);
	ilitek_plat_sleep_init();
	
#if REGULATOR_POWER
	ilitek_plat_regulator_power_init(true);
#endif

	return 0;
}

static int ilitek_plat_remove(void)
{
	ILI_INFO("remove plat dev\n");
	ili_dev_remove();
	return 0;
}

static const struct of_device_id tp_match_table[] = {
	{.compatible = DTS_OF_NAME},
	{},
};

static struct ilitek_hwif_info hwif = {
	.bus_type = TDDI_INTERFACE,
	.plat_type = TP_PLAT_QCOM,
	.owner = THIS_MODULE,
	.name = TDDI_DEV_ID,
	.of_match_table = of_match_ptr(tp_match_table),
	.plat_probe = ilitek_plat_probe,
	.plat_remove = ilitek_plat_remove,
};

int ilitek_plat_dev_init(void)
{
	ILI_INFO("ILITEK TP driver init for QCOM\n");
	if (ili_dev_init(&hwif) < 0) {
		ILI_ERR("Failed to register i2c/spi bus driver\n");
		return -ENODEV;
	}
	return 0;
}

void ilitek_plat_dev_exit(void)
{
	ILI_INFO("remove plat dev\n");
	ili_dev_remove();
}
