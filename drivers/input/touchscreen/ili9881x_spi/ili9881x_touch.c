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

/*gesture info mode*/
struct ili_demo_debug_info_id0 {
	u8 id;
	u8 app_sys_powr_state_e : 3;
	u8 app_sys_state_e : 3;
	u8 tp_state_e : 2;

	u8 touch_palm_state_e : 2;
	u8 app_an_statu_e : 3;
	u8 app_sys_check_bg_abnormal : 1;
	u8 g_b_wrong_bg: 1;
	u8 reserved0 : 1;

	u8 status_of_dynamic_th_e : 4;
	u8 reserved1 : 4;

	u32 algo_pt_status0 : 3;
	u32 algo_pt_status1 : 3;
	u32 algo_pt_status2 : 3;
	u32 algo_pt_status3 : 3;
	u32 algo_pt_status4 : 3;
	u32 algo_pt_status5 : 3;
	u32 algo_pt_status6 : 3;
	u32 algo_pt_status7 : 3;
	u32 algo_pt_status8 : 3;
	u32 algo_pt_status9 : 3;
	u32 reserved2 : 2;

	u16 hopping_flag : 1;
	u16 hopping_index : 5;
	u16 frequency_h : 2;
	u16 frequency_l : 8;

	u16 reserved3 : 16;
};

void ili_dump_data(void *data, int type, int len, int row_len, const char *name)
{
	int i, row = 31;
	u8 *p8 = NULL;
	s32 *p32 = NULL;
	s16 *p16 = NULL;

	if (!debug_en)
		return;

	if (row_len > 0)
		row = row_len;

	if (data == NULL) {
		ILI_ERR("The data going to dump is NULL\n");
		return;
	}

	pr_cont("\n\n");
	pr_cont("ILITEK: Dump %s data\n", name);
	pr_cont("ILITEK: ");

	if (type == 8)
		p8 = (u8 *) data;
	if (type == 32 || type == 10)
		p32 = (s32 *) data;
	if (type == 16)
		p16 = (s16 *) data;

	for (i = 0; i < len; i++) {
		if (type == 8)
			pr_cont(" %4x ", p8[i]);
		else if (type == 32)
			pr_cont(" %4x ", p32[i]);
		else if (type == 10)
			pr_cont(" %4d ", p32[i]);
		else if (type == 16)
			pr_cont(" %4d ", p16[i]);

		if ((i % row) == row - 1) {
			pr_cont("\n");
			pr_cont("ILITEK: ");
		}
	}
	pr_cont("\n\n");
}

static void dma_clear_reg_setting(void)
{
	/* 1. interrupt t0/t1 enable flag */
	if (ili_ice_mode_bit_mask_write(INTR32_ADDR, INTR32_reg_t0_int_en | INTR32_reg_t1_int_en, (0 << 24)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR32_reg_t0_int_en | INTR32_reg_t1_int_en, INTR32_ADDR);

	/* 2. clear tdi_err_int_flag */
	if (ili_ice_mode_bit_mask_write(INTR2_ADDR, INTR2_tdi_err_int_flag_clear, (1 << 18)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR2_tdi_err_int_flag_clear, INTR2_ADDR);

	/* 3. clear dma channel 0 src1 info */
	if (ili_ice_mode_write(DMA49_reg_dma_ch0_src1_addr, 0x00000000, 4) < 0)
		ILI_ERR("Write 0x00000000 at %x failed\n", DMA49_reg_dma_ch0_src1_addr);
	if (ili_ice_mode_write(DMA50_reg_dma_ch0_src1_step_inc, 0x00, 1) < 0)
		ILI_ERR("Write 0x0 at %x failed\n", DMA50_reg_dma_ch0_src1_step_inc);
	if (ili_ice_mode_bit_mask_write(DMA50_ADDR, DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, BIT(31)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, DMA50_ADDR);

	/* 4. clear dma channel 0 trigger select */
	if (ili_ice_mode_bit_mask_write(DMA48_ADDR, DMA48_reg_dma_ch0_trigger_sel, (0 << 16)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA48_reg_dma_ch0_trigger_sel, DMA48_ADDR);
	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_flash_int_flag, (1 << 25)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR1_reg_flash_int_flag, INTR1_ADDR);

	/* 5. clear dma flash setting */
	ili_flash_clear_dma();
}

static void dma_trigger_reg_setting(u32 reg_dest_addr, u32 flash_start_addr, u32 copy_size)
{
	int retry = 30;
	u32 stat = 0;

	/* 1. set dma channel 0 clear */
	if (ili_ice_mode_bit_mask_write(DMA48_ADDR, DMA48_reg_dma_ch0_start_clear, (1 << 25)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA48_reg_dma_ch0_start_clear, DMA48_ADDR);

	/* 2. set dma channel 0 src1 info */
	if (ili_ice_mode_write(DMA49_reg_dma_ch0_src1_addr, 0x00041010, 4) < 0)
		ILI_ERR("Write 0x00041010 at %x failed\n", DMA49_reg_dma_ch0_src1_addr);
	if (ili_ice_mode_write(DMA50_reg_dma_ch0_src1_step_inc, 0x00, 1) < 0)
		ILI_ERR("Write 0x00 at %x failed\n", DMA50_reg_dma_ch0_src1_step_inc);
	if (ili_ice_mode_bit_mask_write(DMA50_ADDR, DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, BIT(31)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA50_reg_dma_ch0_src1_format | DMA50_reg_dma_ch0_src1_en, DMA50_ADDR);

	/* 3. set dma channel 0 src2 info */
	if (ili_ice_mode_bit_mask_write(DMA52_ADDR, DMA52_reg_dma_ch0_src2_en, (0 << 31)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA52_reg_dma_ch0_src2_en, DMA52_ADDR);

	/* 4. set dma channel 0 dest info */
	if (ili_ice_mode_write(DMA53_reg_dma_ch0_dest_addr, reg_dest_addr, 3) < 0)
		ILI_ERR("Write %x at %x failed\n", reg_dest_addr, DMA53_reg_dma_ch0_dest_addr);
	if (ili_ice_mode_write(DMA54_reg_dma_ch0_dest_step_inc, 0x01, 1) < 0)
		ILI_ERR("Write 0x01 at %x failed\n", DMA54_reg_dma_ch0_dest_step_inc);

	if (ili_ice_mode_write(DMA54_ADDR, DMA54_reg_dma_ch0_dest_format | DMA54_reg_dma_ch0_dest_en, BIT(31)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA54_reg_dma_ch0_dest_format | DMA54_reg_dma_ch0_dest_en, DMA54_ADDR);

	/* 5. set dma channel 0 trafer info */
	if (ili_ice_mode_write(DMA55_reg_dma_ch0_trafer_counts, copy_size, 4) < 0)
		ILI_ERR("Write %x at %x failed\n", copy_size, DMA55_reg_dma_ch0_trafer_counts);
	if (ili_ice_mode_bit_mask_write(DMA55_ADDR, DMA55_reg_dma_ch0_trafer_mode, (0 << 24)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA55_reg_dma_ch0_trafer_mode, DMA55_ADDR);

	/* 6. set dma channel 0 int info */
	if (ili_ice_mode_bit_mask_write(INTR33_ADDR, INTR33_reg_dma_ch0_int_en, (1 << 17)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR33_reg_dma_ch0_int_en, INTR33_ADDR);

	/* 7. set dma channel 0 trigger select */
	if (ili_ice_mode_bit_mask_write(DMA48_ADDR, DMA48_reg_dma_ch0_trigger_sel, (1 << 16)) < 0)
		ILI_ERR("Write %lu at %x failed\n", DMA48_reg_dma_ch0_trigger_sel, DMA48_ADDR);

	/* 8. set dma flash setting */
	ili_flash_dma_write(flash_start_addr, (flash_start_addr+copy_size), copy_size);

	/* 9. clear flash and dma ch0 int flag */
	if (ili_ice_mode_bit_mask_write(INTR1_ADDR, INTR1_reg_dma_ch1_int_flag | INTR1_reg_flash_int_flag, BIT(16) | BIT(25)) < 0)
		ILI_ERR("Write %lu at %x failed\n", INTR1_reg_dma_ch1_int_flag | INTR1_reg_flash_int_flag, INTR1_ADDR);
	if (ili_ice_mode_bit_mask_write(0x041013, BIT(0), 1) < 0) //patch
		ILI_ERR("Write %lu at %x failed\n", BIT(0), 0x041013);

	/* DMA Trigger */
	if (ili_ice_mode_write(FLASH4_reg_rcv_data, 0xFF, 1) < 0)
		ILI_ERR("Trigger DMA failed\n");

	/* waiting for fw reload code completed. */
	while (retry > 0) {
		if (ili_ice_mode_read(INTR1_ADDR, &stat, sizeof(u32)) < 0) {
			ILI_ERR("Read 0x%x error\n", INTR1_ADDR);
			retry--;
			continue;
		}

		ILI_DBG("fw dma stat = %x\n", stat);

		if ((stat & BIT(16)) == BIT(16))
			break;

		retry--;
		usleep_range(1000, 1000);
	}

	if (retry <= 0)
		ILI_ERR("DMA fail: Regsiter = 0x%x Flash = 0x%x, Size = %d\n",
			reg_dest_addr, flash_start_addr, copy_size);

	/* CS High */
	if (ili_ice_mode_write(FLASH0_reg_flash_csb, 0x1, 1) < 0)
		ILI_ERR("Pull CS High failed\n");
	/* waiting for CS status done */
	mdelay(10);
}

int ili_move_mp_code_flash(void)
{
	int ret = 0;
	u32 mp_text_size = 0, mp_andes_init_size = 0;
	u32 mp_flash_addr, mp_size, overlay_start_addr, overlay_end_addr;
	bool dma_trigger_enable = 0;
	u8 cmd[2] = {0};
	u8 data[16] = {0};

	cmd[0] = P5_X_MODE_CONTROL;
	cmd[1] = P5_X_FW_TEST_MODE;
	ret = ili9881x_ts->wrapper(cmd, 2, NULL, 0, ON);
	if (ret < 0)
		goto out;

	cmd[0] = P5_X_MP_TEST_MODE_INFO;
	ret = ili9881x_ts->wrapper(cmd, sizeof(u8), data, ili9881x_ts->protocol->mp_info_len, ON);
	ili_dump_data(data, 8, ili9881x_ts->protocol->mp_info_len, 0, "MP overlay info");
	if (ret < 0) {
		ILI_ERR("Failed to write info cmd\n");
		goto out;
	}

	dma_trigger_enable = 0;

	mp_flash_addr = data[3] + (data[2] << 8) + (data[1] << 16);
	mp_size = data[6] + (data[5] << 8) + (data[4] << 16);
	overlay_start_addr = data[9] + (data[8] << 8) + (data[7] << 16);
	overlay_end_addr = data[12] + (data[11] << 8) + (data[10] << 16);

	if (overlay_start_addr != 0x0 && overlay_end_addr != 0x0
		&& data[0] == P5_X_MP_TEST_MODE_INFO)
		dma_trigger_enable = 1;

	ILI_INFO("MP info Overlay: Enable = %d, addr = 0x%x ~ 0x%x, flash addr = 0x%x, mp size = 0x%x\n",
		dma_trigger_enable, overlay_start_addr,
		overlay_end_addr, mp_flash_addr, mp_size);

	/* Check if ic is ready switching test mode from demo mode */
	ili9881x_ts->actual_tp_mode = P5_X_FW_AP_MODE;
	ret = ili_ic_check_busy(50, 50); /* Set busy as 0x41 */
	if (ret < 0)
		goto out;

	ret = ili_ice_mode_ctrl(ENABLE, OFF);
	if (ret < 0)
		goto out;

	if (dma_trigger_enable) {
		mp_andes_init_size = overlay_start_addr;
		mp_text_size = (mp_size - overlay_end_addr) + 1;
		ILI_INFO("MP andes init size = %d , MP text size = %d\n", mp_andes_init_size, mp_text_size);

		dma_clear_reg_setting();

		ILI_INFO("[Move ANDES.INIT to DRAM]\n");
		dma_trigger_reg_setting(0, mp_flash_addr, mp_andes_init_size);	 /* DMA ANDES.INIT */

		dma_clear_reg_setting();

		ILI_INFO("[Move MP.TEXT to DRAM]\n");
		dma_trigger_reg_setting(overlay_end_addr, (mp_flash_addr + overlay_start_addr), mp_text_size);

		dma_clear_reg_setting();
	} else {
		/* DMA Trigger */
		if (ili_ice_mode_write(FLASH4_reg_rcv_data, 0xFF, 1) < 0)
			ILI_ERR("Trigger DMA failed\n");
		/* waiting for fw reload code completed. */
		mdelay(30);

		/* CS High */
		if (ili_ice_mode_write(FLASH0_reg_flash_csb, 0x1, 1) < 0)
			ILI_ERR("Pull CS High failed\n");
		/* waiting for CS status done */
		mdelay(10);
	}

	if (ili_reset_ctrl(TP_IC_CODE_RST) < 0)
		ILI_ERR("IC Code reset failed during moving mp code\n");

	ret = ili_ice_mode_ctrl(DISABLE, OFF);
	if (ret < 0)
		goto out;

	/* Check if ic is already in test mode */
	ili9881x_ts->actual_tp_mode = P5_X_FW_TEST_MODE; /* set busy as 0x51 */
	ret = ili_ic_check_busy(300, 50);
	if (ret < 0)
		ILI_ERR("Check cdc timeout failed after moved mp code\n");

out:
	return ret;
}

int ili_move_mp_code_iram(void)
{
	ILI_INFO("Download MP code to iram\n");
	return ili_fw_upgrade_handler(NULL);
}

int ili_proximity_near(int mode)
{
	int ret = 0;

	ili9881x_ts->prox_near = true;

	switch (mode) {
	case DDI_POWER_ON:
		/*
		 * If the power of VSP and VSN keeps alive when proximity near event
		 * occures, TP can just go to sleep in.
		 */
		ret = ili_ic_func_ctrl("sleep", SLEEP_IN);
		if (ret < 0)
			ILI_ERR("Write sleep in cmd failed\n");
		break;
	case DDI_POWER_OFF:
		ILI_INFO("DDI POWER OFF, do nothing\n");
		break;
	default:
		ILI_ERR("Unknown mode (%d)\n", mode);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int ili_proximity_far(int mode)
{
	int ret = 0;
	u8 cmd[2] = {0};

	if (!ili9881x_ts->prox_near) {
		ILI_INFO("No proximity near event, break\n");
		return 0;
	}

	switch (mode) {
	case WAKE_UP_GESTURE_RECOVERY:
		/*
		 * If the power of VSP and VSN has been shut down previsouly,
		 * TP should go through gesture recovery to get back.
		 */
		ili_gesture_recovery();
		break;
	case WAKE_UP_SWITCH_GESTURE_MODE:
		/*
		 * If the power of VSP and VSN keeps alive in the event of proximity near,
		 * TP can be just recovered by switching gesture mode to get back.
		 */
		cmd[0] = 0xF6;
		cmd[1] = 0x0A;

		ILI_INFO("write prepare gesture command 0xF6 0x0A\n");
		ret = ili9881x_ts->wrapper(cmd, 2, NULL, 0, ON);
		if (ret < 0) {
			ILI_INFO("write prepare gesture command error\n");
			break;
		}

		ret = ili_switch_tp_mode(P5_X_FW_GESTURE_MODE);
		if (ret < 0)
			ILI_ERR("Switch to gesture mode failed during proximity far\n");
		break;
	default:
		ILI_ERR("Unknown mode (%d)\n", mode);
		ret = -EINVAL;
		break;
	}

	ili9881x_ts->prox_near = false;

	return ret;
}

int ili_move_gesture_code_flash(int mode)
{
	int ret = 0;

	ILI_INFO("Switch to Gesture mode, lpwg cmd = %d\n",  ili9881x_ts->gesture_mode);
	ret = ili_set_tp_data_len(ili9881x_ts->gesture_mode);

	return ret;
}

int ili_move_gesture_code_iram(int mode)
{
	int i;
	int timeout = 10;
	u8 cmd[3] = {0}, data;

	if (ili9881x_ts->gesture_load_code == false) {
		ILI_INFO("Switch to Gesture mode, lpwg cmd = %d, no need to load gesture code by driver\n",  mode);
		if (ili_set_tp_data_len(mode) < 0)
			ILI_ERR("Failed to set tp data length\n");
		return 0;
	} else {
		ILI_INFO("Load gesture code by driver\n");
	}

	if (ili_ic_func_ctrl("lpwg", 0x3) < 0)
		ILI_ERR("write gesture flag failed\n");

	ILI_INFO("Switch to Gesture mode, lpwg cmd = %d\n",  mode);
	if (ili_set_tp_data_len(mode) < 0)
		ILI_ERR("Failed to set tp data length\n");

	cmd[0] = 0x1;
	cmd[1] = 0xA;
	cmd[2] = 0x5;

	for (i = 0; i < timeout; i++) {
		if (ili9881x_ts->wrapper(cmd, sizeof(cmd), &data, sizeof(data), ON) < 0)
			ILI_ERR("read gesture ready byte error\n");

		ILI_DBG("gesture ready byte = 0x%x\n", data);
		if (data == 0x91) {
			ILI_INFO("Gesture check fw ready\n");
			break;
		}
	}

	if (i >= timeout) {
		ILI_ERR("Gesture is not ready (0x%x), try to run its recovery\n", cmd[0]);
		ili_gesture_recovery();
		return 0;
	}

	if (ili_fw_upgrade_handler(NULL) < 0)
		ILI_ERR("FW upgrade failed during moving code\n");

	/* FW star run gestrue code cmd */
	cmd[0] = 0x1;
	cmd[1] = 0xA;
	cmd[2] = 0x6;
	if (ili9881x_ts->wrapper(cmd, sizeof(cmd), NULL, 0, ON) < 0)
		ILI_ERR("write 0x1,0xA,0x6 error");

	return 0;
}

u8 ili_calc_packet_checksum(u8 *packet, int len)
{
	int i;
	s32 sum = 0;

	for (i = 0; i < len; i++)
		sum += packet[i];

	return (u8) ((-sum) & 0xFF);
}

int ili_touch_esd_gesture_flash(void)
{
	int ret = 0, retry = 100;
	u32 answer = 0;

	if (ili_ice_mode_ctrl(ENABLE, OFF) < 0)
		ILI_ERR("Enable ice mode failed during gesture recovery\n");

	ILI_INFO("ESD Gesture PWD Addr = 0x%x, Answer = 0x%x\n",
		I2C_ESD_GESTURE_PWD_ADDR, I2C_ESD_GESTURE_RUN);

	/* write a special password to inform FW go back into gesture mode */
	if (ili_ice_mode_write(I2C_ESD_GESTURE_PWD_ADDR, ESD_GESTURE_PWD, 4) < 0)
		ILI_ERR("write password failed\n");

	/* HW reset gives effect to FW receives password successed */
	ili9881x_ts->actual_tp_mode = P5_X_FW_AP_MODE;
	if (ili_reset_ctrl(ili9881x_ts->reset) < 0)
		ILI_ERR("TP Reset failed during gesture recovery\n");

	if (ili_ice_mode_ctrl(ENABLE, ON) < 0)
		ILI_ERR("Enable ice mode failed during gesture recovery\n");

	/* polling another specific register to see if gesutre is enabled properly */
	do {
		if (ili_ice_mode_read(I2C_ESD_GESTURE_PWD_ADDR, &answer, sizeof(u32)) < 0)
			ILI_ERR("Read gesture answer error\n");
		if (answer != I2C_ESD_GESTURE_RUN)
			ILI_INFO("answer = 0x%x != (0x%x)\n", answer, I2C_ESD_GESTURE_RUN);
		mdelay(1);
		retry--;
	} while (answer != I2C_ESD_GESTURE_RUN && retry > 0);

	if (retry <= 0) {
		ILI_ERR("Enter gesture failed\n");
		ret = -1;
	} else {
		ILI_INFO("Enter gesture successfully\n");
	}

	if (ili_ice_mode_ctrl(DISABLE, ON) < 0)
		ILI_ERR("Disable ice mode failed during gesture recovery\n");

	ili9881x_ts->gesture_move_code(ili9881x_ts->gesture_mode);
	return ret;
}

int ili_touch_esd_gesture_iram(void)
{
	int ret = 0, retry = 100;
	u32 answer = 0;
	u32 esd_ges_pwd_addr = 0x0;
	u8 cmd[3] = {0};
	ili9881x_ts->skip_wake = true;

	if (ili_ice_mode_ctrl(ENABLE, OFF) < 0)
		ILI_ERR("Enable ice mode failed during gesture recovery\n");

	if (ili9881x_ts->chip->core_ver >= CORE_VER_1420)
		esd_ges_pwd_addr = I2C_ESD_GESTURE_PWD_ADDR;
	else
		esd_ges_pwd_addr = SPI_ESD_GESTURE_PWD_ADDR;

	ILI_INFO("ESD Gesture PWD Addr = 0x%x, Answer = 0x%x\n",
		esd_ges_pwd_addr, SPI_ESD_GESTURE_RUN);

	/* write a special password to inform FW go back into gesture mode */
	if (ili_ice_mode_write(esd_ges_pwd_addr, ESD_GESTURE_PWD, 4) < 0)
		ILI_ERR("write password failed\n");

	/* Host download gives effect to FW receives password successed */
	ili9881x_ts->actual_tp_mode = P5_X_FW_AP_MODE;
	if (ili_fw_upgrade_handler(NULL) < 0)
		ILI_ERR("FW upgrade failed during gesture recovery\n");

	/* Wait for fw running code finished. */
	if (ili9881x_ts->info_from_hex || (ili9881x_ts->chip->core_ver >= CORE_VER_1410))
		msleep(50);

	if (ili_ice_mode_ctrl(ENABLE, ON) < 0)
		ILI_ERR("Enable ice mode failed during gesture recovery\n");

	/* polling another specific register to see if gesutre is enabled properly */
	do {
		if (ili_ice_mode_read(esd_ges_pwd_addr, &answer, sizeof(u32)) < 0)
			ILI_ERR("Read gesture answer error\n");

		if (answer != SPI_ESD_GESTURE_RUN)
			ILI_INFO("answer = 0x%x != (0x%x)\n", answer, SPI_ESD_GESTURE_RUN);
		mdelay(1);
	} while (answer != SPI_ESD_GESTURE_RUN && --retry > 0);

	if (retry <= 0) {
		ILI_ERR("Enter gesture failed\n");
		ret = -1;
	} else {
		ILI_INFO("Enter gesture successfully\n");
	}

	if (ili_ice_mode_ctrl(DISABLE, ON) < 0)
		ILI_ERR("Disable ice mode failed during gesture recovery\n");

	ili9881x_ts->actual_tp_mode = P5_X_FW_GESTURE_MODE;
	ili9881x_ts->tp_data_len = P5_X_GESTURE_INFO_LENGTH;
	ili9881x_ts->tp_data_format = ili9881x_ts->gesture_mode;
	if (ili9881x_ts->gesture_load_code == false) {
		//ILI_INFO("Switch to Gesture mode, lpwg cmd = %d,\n no need to load gesture code by driver\n",  ili9881x_ts->gesture_mode);
		//if (ili_set_tp_data_len(ili9881x_ts->gesture_mode) < 0)
		//	ILI_ERR("Failed to set tp data length\n");
		return 0;
	} else {
		ILI_INFO("Load gesture code by driver\n");
	}
	
	if (ili_fw_upgrade_handler(NULL) < 0)
		ILI_ERR("FW upgrade failed during gesture recovery\n");

	/* FW star run gestrue code cmd */
	cmd[0] = 0x1;
	cmd[1] = 0xA;
	cmd[2] = 0x6;
	if (ili9881x_ts->wrapper(cmd, sizeof(cmd), NULL, 0, ON) < 0)
		ILI_ERR("write 0x1,0xA,0x6 error");
	ili9881x_ts->skip_wake = false;
	return ret;
}

void ili_demo_debug_info_id0(u8 *buf, size_t len)
{
	struct ili_demo_debug_info_id0 id0;

	ipio_memcpy(&id0, buf, sizeof(id0), len);
	ILI_INFO("id0 len = %d,strucy len = %ld", (int)len, sizeof(id0));

	ILI_INFO("id = %d\n", id0.id);
	ILI_INFO("app_sys_powr_state_e = %d\n", id0.app_sys_powr_state_e);
	ILI_INFO("app_sys_state_e = %d\n", id0.app_sys_state_e);
	ILI_INFO("tp_state_e = %d\n", id0.tp_state_e);
	ILI_INFO("touch_palm_state_e = %d\n", id0.touch_palm_state_e);
	ILI_INFO("app_an_statu_e = %d\n", id0.app_an_statu_e);
	ILI_INFO("app_sys_check_bg_abnormal = %d\n", id0.app_sys_check_bg_abnormal);
	ILI_INFO("g_b_wrong_bg = %d\n", id0.g_b_wrong_bg);
	ILI_INFO("status_of_dynamic_th_e = %d\n", id0.status_of_dynamic_th_e);
	ILI_INFO("algo_pt_status0 = %d\n", id0.algo_pt_status0);
	ILI_INFO("algo_pt_status1 = %d\n", id0.algo_pt_status1);
	ILI_INFO("algo_pt_status2 = %d\n", id0.algo_pt_status2);
	ILI_INFO("algo_pt_status3 = %d\n", id0.algo_pt_status3);
	ILI_INFO("algo_pt_status4 = %d\n", id0.algo_pt_status4);
	ILI_INFO("algo_pt_status5 = %d\n", id0.algo_pt_status5);
	ILI_INFO("algo_pt_status6 = %d\n", id0.algo_pt_status6);
	ILI_INFO("algo_pt_status7 = %d\n", id0.algo_pt_status7);
	ILI_INFO("algo_pt_status8 = %d\n", id0.algo_pt_status8);
	ILI_INFO("algo_pt_status9 = %d\n", id0.algo_pt_status9);
	ILI_INFO("hopping_flag = %d\n", id0.hopping_flag);
	ILI_INFO("hopping_index = %d\n", id0.hopping_index);
	ILI_INFO("frequency = %d\n", (id0.frequency_h << 8 | id0.frequency_l));
}

void ili_demo_debug_info_mode(u8 *buf, size_t len)
{
	u8 *info_ptr;
	u8 info_id, info_len;

	ili_report_ap_mode(buf, P5_X_DEMO_MODE_PACKET_LEN);
	info_ptr = buf + P5_X_DEMO_MODE_PACKET_LEN;
	info_len = info_ptr[0];
	info_id = info_ptr[1];

	ILI_INFO("info len = %d ,id = %d\n", info_len, info_id);

	ili9881x_ts->demo_debug_info[info_id](&info_ptr[1] , info_len);
}

static void ilitek_tddi_touch_send_debug_data(u8 *buf, int len)
{
	int index;

	mutex_lock(&ili9881x_ts->debug_mutex);

	if (!ili9881x_ts->netlink && !ili9881x_ts->dnp)
		goto out;

	/* Send data to netlink */
	if (ili9881x_ts->netlink) {
		ili_netlink_reply_msg(buf, len);
		goto out;
	}

	/* Sending data to apk via the node of debug_message node */
	if (ili9881x_ts->dnp) {
		index = ili9881x_ts->dbf;
		if (!ili9881x_ts->dbl[ili9881x_ts->dbf].mark) {
			ili9881x_ts->dbf = ((ili9881x_ts->dbf + 1) % TR_BUF_LIST_SIZE);
		} else {
			if (ili9881x_ts->dbf == 0)
				index = TR_BUF_LIST_SIZE -1;
			else
				index = ili9881x_ts->dbf -1;
		}
		if (ili9881x_ts->dbl[index].data == NULL) {
			ILI_ERR("BUFFER %d error\n", index);
			goto out;
		}
		ipio_memcpy(ili9881x_ts->dbl[index].data, buf, len, 2048);
		ili9881x_ts->dbl[index].mark = true;
		wake_up(&(ili9881x_ts->inq));
		goto out;
	}

out:
	mutex_unlock(&ili9881x_ts->debug_mutex);
}

void ili_touch_press(u16 x, u16 y, u16 pressure, u16 id)
{
	ILI_DBG("Touch Press: id = %d, x = %d, y = %d, p = %d\n", id, x, y, pressure);

	if (MT_B_TYPE) {
		input_mt_slot(ili9881x_ts->input, id);
		input_mt_report_slot_state(ili9881x_ts->input, MT_TOOL_FINGER, true);
		input_report_abs(ili9881x_ts->input, ABS_MT_POSITION_X, x);
		input_report_abs(ili9881x_ts->input, ABS_MT_POSITION_Y, y);
		if (MT_PRESSURE)
			input_report_abs(ili9881x_ts->input, ABS_MT_PRESSURE, pressure);
	} else {
		input_report_key(ili9881x_ts->input, BTN_TOUCH, 1);
		input_report_abs(ili9881x_ts->input, ABS_MT_TRACKING_ID, id);
		input_report_abs(ili9881x_ts->input, ABS_MT_TOUCH_MAJOR, 1);
		input_report_abs(ili9881x_ts->input, ABS_MT_WIDTH_MAJOR, 1);
		input_report_abs(ili9881x_ts->input, ABS_MT_POSITION_X, x);
		input_report_abs(ili9881x_ts->input, ABS_MT_POSITION_Y, y);
		if (MT_PRESSURE)
			input_report_abs(ili9881x_ts->input, ABS_MT_PRESSURE, pressure);

		input_mt_sync(ili9881x_ts->input);
	}
}

void ili_touch_release(u16 x, u16 y, u16 id)
{
	ILI_DBG("Touch Release: id = %d, x = %d, y = %d\n", id, x, y);

	if (MT_B_TYPE) {
		input_mt_slot(ili9881x_ts->input, id);
		input_mt_report_slot_state(ili9881x_ts->input, MT_TOOL_FINGER, false);
	} else {
		input_report_key(ili9881x_ts->input, BTN_TOUCH, 0);
		input_mt_sync(ili9881x_ts->input);
	}
}

void ili_touch_release_all_point(void)
{
	int i;

	if (MT_B_TYPE) {
		for (i = 0 ; i < MAX_TOUCH_NUM; i++)
			ili_touch_release(0, 0, i);

		input_report_key(ili9881x_ts->input, BTN_TOUCH, 0);
		input_report_key(ili9881x_ts->input, BTN_TOOL_FINGER, 0);
	} else {
		ili_touch_release(0, 0, 0);
	}
	input_sync(ili9881x_ts->input);
}

static struct ilitek_touch_info touch_info[MAX_TOUCH_NUM];

void ili_report_ap_mode(u8 *buf, int len)
{
	int i = 0;
	u32 xop = 0, yop = 0;

	memset(touch_info, 0x0, sizeof(touch_info));

	ili9881x_ts->finger = 0;

	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		if ((buf[(4 * i) + 1] == 0xFF) && (buf[(4 * i) + 2] == 0xFF)
			&& (buf[(4 * i) + 3] == 0xFF)) {
			if (MT_B_TYPE)
				ili9881x_ts->curt_touch[i] = 0;
			continue;
		}

		xop = (((buf[(4 * i) + 1] & 0xF0) << 4) | (buf[(4 * i) + 2]));
		yop = (((buf[(4 * i) + 1] & 0x0F) << 8) | (buf[(4 * i) + 3]));

		if (ili9881x_ts->trans_xy) {
			touch_info[ili9881x_ts->finger].x = xop;
			touch_info[ili9881x_ts->finger].y = yop;
		} else {
			touch_info[ili9881x_ts->finger].x = xop * ili9881x_ts->panel_wid / TPD_WIDTH;
			touch_info[ili9881x_ts->finger].y = yop * ili9881x_ts->panel_hei / TPD_HEIGHT;
		}

		touch_info[ili9881x_ts->finger].id = i;

		if (MT_PRESSURE)
			touch_info[ili9881x_ts->finger].pressure = buf[(4 * i) + 4];
		else
			touch_info[ili9881x_ts->finger].pressure = 1;

		ILI_DBG("original x = %d, y = %d\n", xop, yop);
		ili9881x_ts->finger++;
		if (MT_B_TYPE)
			ili9881x_ts->curt_touch[i] = 1;
	}

	ILI_DBG("figner number = %d, LastTouch = %d\n", ili9881x_ts->finger, ili9881x_ts->last_touch);

	if (ili9881x_ts->finger) {
		if (MT_B_TYPE) {
			for (i = 0; i < ili9881x_ts->finger; i++) {
				input_report_key(ili9881x_ts->input, BTN_TOUCH, 1);
				ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
				input_report_key(ili9881x_ts->input, BTN_TOOL_FINGER, 1);
			}
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				if (ili9881x_ts->curt_touch[i] == 0 && ili9881x_ts->prev_touch[i] == 1)
					ili_touch_release(0, 0, i);
				ili9881x_ts->prev_touch[i] = ili9881x_ts->curt_touch[i];
			}
		} else {
			for (i = 0; i < ili9881x_ts->finger; i++)
				ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
		}
		input_sync(ili9881x_ts->input);
		ili9881x_ts->last_touch = ili9881x_ts->finger;
	} else {
		if (ili9881x_ts->last_touch) {
			if (MT_B_TYPE) {
				for (i = 0; i < MAX_TOUCH_NUM; i++) {
					if (ili9881x_ts->curt_touch[i] == 0 && ili9881x_ts->prev_touch[i] == 1)
						ili_touch_release(0, 0, i);
					ili9881x_ts->prev_touch[i] = ili9881x_ts->curt_touch[i];
				}
				input_report_key(ili9881x_ts->input, BTN_TOUCH, 0);
				input_report_key(ili9881x_ts->input, BTN_TOOL_FINGER, 0);
			} else {
				ili_touch_release(0, 0, 0);
			}
			input_sync(ili9881x_ts->input);
			ili9881x_ts->last_touch = 0;
		}
	}
	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_report_debug_mode(u8 *buf, int len)
{
	int i = 0;
	u32 xop = 0, yop = 0;
	static u8 p[MAX_TOUCH_NUM];

	memset(touch_info, 0x0, sizeof(touch_info));

	ili9881x_ts->finger = 0;

	for (i = 0; i < MAX_TOUCH_NUM; i++) {
		if ((buf[(3 * i) + 5] == 0xFF) && (buf[(3 * i) + 6] == 0xFF)
			&& (buf[(3 * i) + 7] == 0xFF)) {
			if (MT_B_TYPE)
				ili9881x_ts->curt_touch[i] = 0;
			continue;
		}

		xop = (((buf[(3 * i) + 5] & 0xF0) << 4) | (buf[(3 * i) + 6]));
		yop = (((buf[(3 * i) + 5] & 0x0F) << 8) | (buf[(3 * i) + 7]));

		if (ili9881x_ts->trans_xy) {
			touch_info[ili9881x_ts->finger].x = xop;
			touch_info[ili9881x_ts->finger].y = yop;
		} else {
			touch_info[ili9881x_ts->finger].x = xop * ili9881x_ts->panel_wid / TPD_WIDTH;
			touch_info[ili9881x_ts->finger].y = yop * ili9881x_ts->panel_hei / TPD_HEIGHT;
		}

		touch_info[ili9881x_ts->finger].id = i;

		if (MT_PRESSURE) {
			/*
			 * Since there's no pressure data in debug mode, we make fake values
			 * for android system if pressure needs to be reported.
			 */
			if (p[ili9881x_ts->finger] == 1)
				touch_info[ili9881x_ts->finger].pressure = p[ili9881x_ts->finger] = 2;
			else
				touch_info[ili9881x_ts->finger].pressure = p[ili9881x_ts->finger] = 1;
		} else {
			touch_info[ili9881x_ts->finger].pressure = 1;
		}

		ILI_DBG("original x = %d, y = %d\n", xop, yop);
		ili9881x_ts->finger++;
		if (MT_B_TYPE)
			ili9881x_ts->curt_touch[i] = 1;
	}

	ILI_DBG("figner number = %d, LastTouch = %d\n", ili9881x_ts->finger, ili9881x_ts->last_touch);

	if (ili9881x_ts->finger) {
		if (MT_B_TYPE) {
			for (i = 0; i < ili9881x_ts->finger; i++) {
				input_report_key(ili9881x_ts->input, BTN_TOUCH, 1);
				ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
				input_report_key(ili9881x_ts->input, BTN_TOOL_FINGER, 1);
			}
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				if (ili9881x_ts->curt_touch[i] == 0 && ili9881x_ts->prev_touch[i] == 1)
					ili_touch_release(0, 0, i);
				ili9881x_ts->prev_touch[i] = ili9881x_ts->curt_touch[i];
			}
		} else {
			for (i = 0; i < ili9881x_ts->finger; i++)
				ili_touch_press(touch_info[i].x, touch_info[i].y, touch_info[i].pressure, touch_info[i].id);
		}
		input_sync(ili9881x_ts->input);
		ili9881x_ts->last_touch = ili9881x_ts->finger;
	} else {
		if (ili9881x_ts->last_touch) {
			if (MT_B_TYPE) {
				for (i = 0; i < MAX_TOUCH_NUM; i++) {
					if (ili9881x_ts->curt_touch[i] == 0 && ili9881x_ts->prev_touch[i] == 1)
						ili_touch_release(0, 0, i);
					ili9881x_ts->prev_touch[i] = ili9881x_ts->curt_touch[i];
				}
				input_report_key(ili9881x_ts->input, BTN_TOUCH, 0);
				input_report_key(ili9881x_ts->input, BTN_TOOL_FINGER, 0);
			} else {
				ili_touch_release(0, 0, 0);
			}
			input_sync(ili9881x_ts->input);
			ili9881x_ts->last_touch = 0;
		}
	}
	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_report_gesture_mode(u8 *buf, int len)
{
	int i, lu_x = 0, lu_y = 0, rd_x = 0, rd_y = 0, score = 0;
	u8 ges[P5_X_GESTURE_INFO_LENGTH] = {0};
	struct gesture_coordinate *gc = ili9881x_ts->gcoord;
	struct input_dev *input = ili9881x_ts->input;
	bool transfer = ili9881x_ts->trans_xy;

	for (i = 0; i < len; i++)
		ges[i] = buf[i];

	memset(gc, 0x0, sizeof(struct gesture_coordinate));

	gc->code = ges[1];
	score = ges[36];
	ILI_INFO("gesture code = 0x%x, score = %d\n", gc->code, score);

	/* Parsing gesture coordinate */
	gc->pos_start.x = ((ges[4] & 0xF0) << 4) | ges[5];
	gc->pos_start.y = ((ges[4] & 0x0F) << 8) | ges[6];
	gc->pos_end.x   = ((ges[7] & 0xF0) << 4) | ges[8];
	gc->pos_end.y   = ((ges[7] & 0x0F) << 8) | ges[9];
	gc->pos_1st.x   = ((ges[16] & 0xF0) << 4) | ges[17];
	gc->pos_1st.y   = ((ges[16] & 0x0F) << 8) | ges[18];
	gc->pos_2nd.x   = ((ges[19] & 0xF0) << 4) | ges[20];
	gc->pos_2nd.y   = ((ges[19] & 0x0F) << 8) | ges[21];
	gc->pos_3rd.x   = ((ges[22] & 0xF0) << 4) | ges[23];
	gc->pos_3rd.y   = ((ges[22] & 0x0F) << 8) | ges[24];
	gc->pos_4th.x   = ((ges[25] & 0xF0) << 4) | ges[26];
	gc->pos_4th.y   = ((ges[25] & 0x0F) << 8) | ges[27];

	switch (gc->code) {
	case GESTURE_DOUBLECLICK:
		ILI_INFO("Double Click key event\n");
		input_report_key(input, KEY_WAKEUP, 1);
		input_sync(input);
		input_report_key(input, KEY_WAKEUP, 0);
		input_sync(input);
		gc->type  = GESTURE_DOUBLECLICK;
		gc->clockwise = 1;
		gc->pos_end.x = gc->pos_start.x;
		gc->pos_end.y = gc->pos_start.y;
		break;
	case GESTURE_LEFT:
		gc->type  = GESTURE_LEFT;
		gc->clockwise = 1;
		break;
	case GESTURE_RIGHT:
		gc->type  = GESTURE_RIGHT;
		gc->clockwise = 1;
		break;
	case GESTURE_UP:
		gc->type  = GESTURE_UP;
		gc->clockwise = 1;
		break;
	case GESTURE_DOWN:
		gc->type  = GESTURE_DOWN;
		gc->clockwise = 1;
		break;
	case GESTURE_O:
		gc->type  = GESTURE_O;
		gc->clockwise = (ges[34] > 1) ? 0 : ges[34];

		lu_x = (((ges[28] & 0xF0) << 4) | (ges[29]));
		lu_y = (((ges[28] & 0x0F) << 8) | (ges[30]));
		rd_x = (((ges[31] & 0xF0) << 4) | (ges[32]));
		rd_y = (((ges[31] & 0x0F) << 8) | (ges[33]));

		gc->pos_1st.x = ((rd_x + lu_x) / 2);
		gc->pos_1st.y = lu_y;
		gc->pos_2nd.x = lu_x;
		gc->pos_2nd.y = ((rd_y + lu_y) / 2);
		gc->pos_3rd.x = ((rd_x + lu_x) / 2);
		gc->pos_3rd.y = rd_y;
		gc->pos_4th.x = rd_x;
		gc->pos_4th.y = ((rd_y + lu_y) / 2);
		break;
	case GESTURE_W:
		gc->type  = GESTURE_W;
		gc->clockwise = 1;
		break;
	case GESTURE_M:
		gc->type  = GESTURE_M;
		gc->clockwise = 1;
		break;
	case GESTURE_V:
		gc->type  = GESTURE_V;
		gc->clockwise = 1;
		break;
	case GESTURE_C:
		gc->type  = GESTURE_C;
		gc->clockwise = 1;
		break;
	case GESTURE_E:
		gc->type  = GESTURE_E;
		gc->clockwise = 1;
		break;
	case GESTURE_S:
		gc->type  = GESTURE_S;
		gc->clockwise = 1;
		break;
	case GESTURE_Z:
		gc->type  = GESTURE_Z;
		gc->clockwise = 1;
		break;
	case GESTURE_TWOLINE_DOWN:
		gc->type  = GESTURE_TWOLINE_DOWN;
		gc->clockwise = 1;
		gc->pos_1st.x  = (((ges[10] & 0xF0) << 4) | (ges[11]));
		gc->pos_1st.y  = (((ges[10] & 0x0F) << 8) | (ges[12]));
		gc->pos_2nd.x  = (((ges[13] & 0xF0) << 4) | (ges[14]));
		gc->pos_2nd.y  = (((ges[13] & 0x0F) << 8) | (ges[15]));
		break;
	default:
		ILI_ERR("Unknown gesture code\n");
		break;
	}

	if (!transfer) {
		gc->pos_start.x	= gc->pos_start.x * ili9881x_ts->panel_wid / TPD_WIDTH;
		gc->pos_start.y = gc->pos_start.y * ili9881x_ts->panel_hei / TPD_HEIGHT;
		gc->pos_end.x   = gc->pos_end.x * ili9881x_ts->panel_wid / TPD_WIDTH;
		gc->pos_end.y   = gc->pos_end.y * ili9881x_ts->panel_hei / TPD_HEIGHT;
		gc->pos_1st.x   = gc->pos_1st.x * ili9881x_ts->panel_wid / TPD_WIDTH;
		gc->pos_1st.y   = gc->pos_1st.y * ili9881x_ts->panel_hei / TPD_HEIGHT;
		gc->pos_2nd.x   = gc->pos_2nd.x * ili9881x_ts->panel_wid / TPD_WIDTH;
		gc->pos_2nd.y   = gc->pos_2nd.y * ili9881x_ts->panel_hei / TPD_HEIGHT;
		gc->pos_3rd.x   = gc->pos_3rd.x * ili9881x_ts->panel_wid / TPD_WIDTH;
		gc->pos_3rd.y   = gc->pos_3rd.y * ili9881x_ts->panel_hei / TPD_HEIGHT;
		gc->pos_4th.x   = gc->pos_4th.x * ili9881x_ts->panel_wid / TPD_WIDTH;
		gc->pos_4th.y   = gc->pos_4th.y * ili9881x_ts->panel_hei / TPD_HEIGHT;
	}

	ILI_INFO("Transfer = %d, Type = %d, clockwise = %d\n", transfer, gc->type, gc->clockwise);
	ILI_INFO("Gesture Points: (%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)(%d, %d)\n",
			gc->pos_start.x, gc->pos_start.y,
			gc->pos_end.x, gc->pos_end.y,
			gc->pos_1st.x, gc->pos_1st.y,
			gc->pos_2nd.x ,gc->pos_2nd.y,
			gc->pos_3rd.x, gc->pos_3rd.y,
			gc->pos_4th.x, gc->pos_4th.y);

	ilitek_tddi_touch_send_debug_data(buf, len);
}

void ili_report_i2cuart_mode(u8 *buf, int len)
{
	int type = buf[3] & 0x0F;
	int need_read_len = 0, one_data_bytes = 0;
	int actual_len = len - 5;
	int uart_len;
	u8 *uart_buf = NULL, *total_buf = NULL;

	ILI_DBG("data[3] = %x, type = %x, actual_len = %d\n",
					buf[3], type, actual_len);

	need_read_len = buf[1] * buf[2];

	if (type == 0 || type == 1 || type == 6) {
		one_data_bytes = 1;
	} else if (type == 2 || type == 3) {
		one_data_bytes = 2;
	} else if (type == 4 || type == 5) {
		one_data_bytes = 4;
	}

	need_read_len =  need_read_len * one_data_bytes + 1;
	ILI_DBG("need_read_len = %d  one_data_bytes = %d\n", need_read_len, one_data_bytes);

	if (need_read_len < actual_len) {
		ilitek_tddi_touch_send_debug_data(buf, len);
		goto out;
	}

	uart_len = need_read_len - actual_len;
	ILI_DBG("uart len = %d\n", uart_len);

	uart_buf = kcalloc(uart_len, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(uart_buf)) {
		ILI_ERR("Failed to allocate uart_buf memory %ld\n", PTR_ERR(uart_buf));
		goto out;
	}

	if (ili9881x_ts->wrapper(NULL, 0, uart_buf, uart_len, OFF) < 0) {
		ILI_ERR("i2cuart read data failed\n");
		goto out;
	}

	total_buf = kcalloc(len + uart_len, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(total_buf)) {
		ILI_ERR("Failed to allocate total_buf memory %ld\n", PTR_ERR(total_buf));
		goto out;
	}

	memcpy(total_buf, buf, len);
	memcpy(total_buf + len, uart_buf, uart_len);
	ilitek_tddi_touch_send_debug_data(total_buf, len + uart_len);

out:
	ipio_kfree((void **)&uart_buf);
	ipio_kfree((void **)&total_buf);
	return;
}
