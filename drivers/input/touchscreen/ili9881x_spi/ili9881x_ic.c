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

#define PROTOCL_VER_NUM		8
static struct ilitek_protocol_info protocol_info[PROTOCL_VER_NUM] = {
	/* length -> fw, protocol, tp, key, panel, core, func, window, cdc, mp_info */
	[0] = {PROTOCOL_VER_500, 4, 4, 14, 30, 5, 5, 2, 8, 3, 8},
	[1] = {PROTOCOL_VER_510, 4, 3, 14, 30, 5, 5, 3, 8, 3, 8},
	[2] = {PROTOCOL_VER_520, 4, 4, 14, 30, 5, 5, 3, 8, 3, 8},
	[3] = {PROTOCOL_VER_530, 9, 4, 14, 30, 5, 5, 3, 8, 3, 8},
	[4] = {PROTOCOL_VER_540, 9, 4, 14, 30, 5, 5, 3, 8, 15, 8},
	[5] = {PROTOCOL_VER_550, 9, 4, 14, 30, 5, 5, 3, 8, 15, 14},
	[6] = {PROTOCOL_VER_560, 9, 4, 14, 30, 5, 5, 3, 8, 15, 14},
	[7] = {PROTOCOL_VER_570, 9, 4, 14, 30, 5, 5, 3, 8, 15, 14},
};

#define FUNC_CTRL_NUM	19
static struct ilitek_ic_func_ctrl func_ctrl[FUNC_CTRL_NUM] = {
	/* cmd[3] = cmd, func, ctrl */
	[0] = {"sense", {0x1, 0x1, 0x0}, 3},
	[1] = {"sleep", {0x1, 0x2, 0x0}, 3},
	[2] = {"glove", {0x1, 0x6, 0x0}, 3},
	[3] = {"stylus", {0x1, 0x7, 0x0}, 3},
	[4] = {"tp_scan_mode", {0x1, 0x8, 0x0}, 3},
	[5] = {"lpwg", {0x1, 0xA, 0x0}, 3},
	[6] = {"gesture", {0x1, 0xB, 0x3F}, 3},
	[7] = {"phone_cover", {0x1, 0xC, 0x0}, 3},
	[8] = {"finger_sense", {0x1, 0xF, 0x0}, 3},
	[9] = {"phone_cover_window", {0xE, 0x0, 0x0}, 3},
	[10] = {"proximity", {0x1, 0x10, 0x0}, 3},
	[11] = {"plug", {0x1, 0x11, 0x0}, 3},
	[12] = {"edge_palm", {0x1, 0x12, 0x0}, 3},
	[13] = {"lock_point", {0x1, 0x13, 0x0}, 3},
	[14] = {"active", {0x1, 0x14, 0x0}, 3},
	[15] = {"idle", {0x1, 0x19, 0x0}, 3},
	[16] = {"gesture_demo_en", {0x1, 0x16, 0x0}, 3},
	[17] = {"tp_recore", {0x1, 0x18, 0x0}, 3},
	[18] = {"knock_en", {0x1, 0xA, 0x8, 0x03, 0x0, 0x0}, 6},
};

#define CHIP_SUP_NUM	4
static u32 ic_sup_list[CHIP_SUP_NUM] = {
	[0] = ILI9881_CHIP,
	[1] = ILI7807_CHIP,
	[2] = ILI9881N_AA,
	[3] = ILI9881O_AA
};

static int ilitek_tddi_ic_check_support(u32 pid, u16 id)
{
	int i = 0;

	for (i = 0; i < CHIP_SUP_NUM; i++) {
		if ((pid == ic_sup_list[i]) || (id == ic_sup_list[i]))
			break;
	}

	if (i >= CHIP_SUP_NUM) {
		ILI_INFO("ERROR, ILITEK CHIP(0x%x) Not found !!\n", pid);
		return -1;
	}

	ILI_INFO("ILITEK CHIP ILI9881x found.\n");

	ili9881x_ts->chip->pid = pid;

	ili9881x_ts->chip->reset_key = 0x00019881;
	ili9881x_ts->chip->wtd_key = 0x9881;
	ili9881x_ts->chip->dma_reset = ENABLE;
	ili9881x_ts->chip->no_bk_shift = RAWDATA_NO_BK_SHIFT;
	ili9881x_ts->chip->max_count = 0x1FFFF;
	return 0;
}

int ili_ice_mode_bit_mask_write(u32 addr, u32 mask, u32 value)
{
	int ret = 0;
	u32 data = 0;

	if (ili_ice_mode_read(addr, &data, sizeof(u32)) < 0) {
		ILI_ERR("Read data error\n");
		return -1;
	}

	data &= (~mask);
	data |= (value & mask);

	ILI_DBG("mask value data = %x\n", data);

	ret = ili_ice_mode_write(addr, data, sizeof(u32));
	if (ret < 0)
		ILI_ERR("Failed to re-write data in ICE mode, ret = %d\n", ret);

	return ret;
}

int ili_ice_mode_write(u32 addr, u32 data, int len)
{
	int ret = 0, i;
	u8 txbuf[64] = {0};

	if (!atomic_read(&ili9881x_ts->ice_stat)) {
		ILI_ERR("ice mode not enabled\n");
		return -1;
	}

	txbuf[0] = 0x25;
	txbuf[1] = (char)((addr & 0x000000FF) >> 0);
	txbuf[2] = (char)((addr & 0x0000FF00) >> 8);
	txbuf[3] = (char)((addr & 0x00FF0000) >> 16);

	for (i = 0; i < len; i++)
		txbuf[i + 4] = (char)(data >> (8 * i));

	ret = ili9881x_ts->wrapper(txbuf, len + 4, NULL, 0, OFF);
	if (ret < 0)
		ILI_ERR("Failed to write data in ice mode, ret = %d\n", ret);

	return ret;
}

int ili_ice_mode_read(u32 addr, u32 *data, int len)
{
	int ret = 0;
	u8 *rxbuf = NULL;
	u8 txbuf[4] = {0};

	if (!atomic_read(&ili9881x_ts->ice_stat)) {
		ILI_ERR("ice mode not enabled\n");
		return -1;
	}

	txbuf[0] = 0x25;
	txbuf[1] = (char)((addr & 0x000000FF) >> 0);
	txbuf[2] = (char)((addr & 0x0000FF00) >> 8);
	txbuf[3] = (char)((addr & 0x00FF0000) >> 16);

	ret = ili9881x_ts->wrapper(txbuf, sizeof(txbuf), NULL, 0, OFF);
	if (ret < 0)
		goto out;

	rxbuf = kcalloc(len, sizeof(u8), GFP_KERNEL);
	if (ERR_ALLOC_MEM(rxbuf)) {
		ILI_ERR("Failed to allocate rxbuf, %ld\n", PTR_ERR(rxbuf));
		ret = -ENOMEM;
		goto out;
	}

	ret = ili9881x_ts->wrapper(NULL, 0, rxbuf, len, OFF);
	if (ret < 0)
		goto out;

	if (len == sizeof(u8))
		*data = rxbuf[0];
	else
		*data = (rxbuf[0] | rxbuf[1] << 8 | rxbuf[2] << 16 | rxbuf[3] << 24);

out:
	if (ret < 0)
		ILI_ERR("Failed to read data in ice mode, ret = %d\n", ret);

	ipio_kfree((void **)&rxbuf);
	return ret;
}

int ili_ice_mode_ctrl(bool enable, bool mcu)
{
	int ret = 0, retry = 3;
	u8 cmd_open[4] = {0x25, 0x62, 0x10, 0x18};
	u8 cmd_close[4] = {0x1B, 0x62, 0x10, 0x18};
	u32 pid;

	ILI_INFO("%s ICE mode, mcu on = %d\n", (enable ? "Enable" : "Disable"), mcu);

	if (enable) {
		if (atomic_read(&ili9881x_ts->ice_stat)) {
			ILI_INFO("ice mode already enabled\n");
			return 0;
		}

		if (mcu)
			cmd_open[0] = 0x1F;

		atomic_set(&ili9881x_ts->ice_stat, ENABLE);

		do {
			if (ili9881x_ts->wrapper(cmd_open, sizeof(cmd_open), NULL, 0, OFF) < 0)
				ILI_ERR("write ice mode cmd error\n");

			if (ili9881x_ts->spi_speed != NULL && ili9881x_ts->chip->spi_speed_ctrl)
				ili9881x_ts->spi_speed(ON);

			/* Read chip id to ensure that ice mode is enabled successfully */
			if (ili_ice_mode_read(ili9881x_ts->chip->pid_addr, &pid, sizeof(u32)) < 0)
				ILI_ERR("Read pid error\n");

			if (ilitek_tddi_ic_check_support(pid, pid >> 16) == 0)
				break;
		} while (--retry > 0);

		if (retry <= 0) {
			ILI_ERR("Enter to ICE Mode failed !!\n");
			atomic_set(&ili9881x_ts->ice_stat, DISABLE);
			ret = -1;
			goto out;
		}

		/* Patch to resolve the issue of i2c nack after exit to ice mode */
		if (ili_ice_mode_write(0x47002, 0x00, 1) < 0)
			ILI_ERR("Write 0x0 at 0x47002 failed\n");
	} else {
		if (!atomic_read(&ili9881x_ts->ice_stat)) {
			ILI_INFO("ice mode already disabled\n");
			return 0;
		}

		ret = ili9881x_ts->wrapper(cmd_close, sizeof(cmd_close), NULL, 0, OFF);
		if (ret < 0)
			ILI_ERR("Exit to ICE Mode failed !!\n");

		atomic_set(&ili9881x_ts->ice_stat, DISABLE);
	}
out:
	return ret;
}

int ili_ic_watch_dog_ctrl(bool write, bool enable)
{
	int timeout = 50, ret = 0;

	if (!atomic_read(&ili9881x_ts->ice_stat)) {
		ILI_ERR("ice mode wasn't enabled\n");
		return -1;
	}

	if (ili9881x_ts->chip->wdt_addr <= 0 || ili9881x_ts->chip->id <= 0) {
		ILI_ERR("WDT/CHIP ID is invalid\n");
		return -EINVAL;
	}

	/* FW will automatiacally disable WDT in I2C */
	if (ili9881x_ts->wtd_ctrl == OFF) {
		ILI_INFO("WDT ctrl is off, do nothing\n");
		return 0;
	}

	if (!write) {
		if (ili_ice_mode_read(ili9881x_ts->chip->wdt_addr, &ret, sizeof(u8)) < 0) {
			ILI_ERR("Read wdt error\n");
			return -1;
		}
		ILI_INFO("Read WDT: %s\n", (ret ? "ON" : "OFF"));
		return ret;
	}

	ILI_INFO("%s WDT, key = %x\n", (enable ? "Enable" : "Disable"), ili9881x_ts->chip->wtd_key);

	if (enable) {
		if (ili_ice_mode_write(ili9881x_ts->chip->wdt_addr, 1, 1) < 0)
			ILI_ERR("Wrie WDT key failed\n");
	} else {
		/* need delay 300us to wait fw relaod code after stop mcu. */
		udelay(300);
		if (ili_ice_mode_write(ili9881x_ts->chip->wdt_addr, (ili9881x_ts->chip->wtd_key & 0xff), 1) < 0)
			ILI_ERR("Write WDT key failed\n");
		if (ili_ice_mode_write(ili9881x_ts->chip->wdt_addr, (ili9881x_ts->chip->wtd_key >> 8), 1) < 0)
			ILI_ERR("Write WDT key failed\n");
	}

	while (timeout > 0) {
		udelay(40);
		if (ili_ice_mode_read(TDDI_WDT_ACTIVE_ADDR, &ret, sizeof(u8)) < 0)
			ILI_ERR("Read wdt active error\n");

		ILI_DBG("ret = %x\n", ret);
		if (enable) {
			if (ret == TDDI_WDT_ON)
				break;
		} else {
			if (ret == TDDI_WDT_OFF)
				break;

			/* If WDT can't be disabled, try to command and wait to see */
			if (ili_ice_mode_write(ili9881x_ts->chip->wdt_addr, 0x00, 1) < 0)
				ILI_ERR("Write 0x0 at %x\n", ili9881x_ts->chip->wdt_addr);
			if (ili_ice_mode_write(ili9881x_ts->chip->wdt_addr, 0x98, 1) < 0)
				ILI_ERR("Write 0x98 at %x\n", ili9881x_ts->chip->wdt_addr);
		}
		timeout--;
	}

	if (timeout <= 0) {
		ILI_ERR("WDT turn on/off timeout !, ret = %x\n", ret);
		ili_ic_get_pc_counter();
		return -EINVAL;
	}

	if (enable) {
		ILI_INFO("WDT turn on succeed\n");
	} else {
		ILI_INFO("WDT turn off succeed\n");
		if (ili_ice_mode_write(ili9881x_ts->chip->wdt_addr, 0, 1) < 0)
			ILI_ERR("Write turn off cmd failed\n");
	}
	return 0;
}

int ili_ic_func_ctrl(const char *name, int ctrl)
{
	int i = 0, ret;

	for (i = 0; i < FUNC_CTRL_NUM; i++) {
		if (ipio_strcmp(name, func_ctrl[i].name) == 0) {
			if (strlen(name) != strlen(func_ctrl[i].name))
				continue;
			break;
		}
	}

	if (i >= FUNC_CTRL_NUM) {
		ILI_ERR("Not found function ctrl, %s\n", name);
		ret = -1;
		goto out;
	}

	if (ili9881x_ts->protocol->ver == PROTOCOL_VER_500) {
		ILI_ERR("Non support function ctrl with protocol v5.0\n");
		ret = -1;
		goto out;
	}

	if (ili9881x_ts->protocol->ver >= PROTOCOL_VER_560) {
		if (ipio_strcmp(func_ctrl[i].name, "gesture") == 0 ||
			ipio_strcmp(func_ctrl[i].name, "phone_cover_window") == 0) {
			ILI_INFO("Non support %s function ctrl\n", func_ctrl[i].name);
			ret = -1;
			goto out;
		}
	}

	func_ctrl[i].cmd[2] = ctrl;

	ILI_INFO("func = %s, len = %d, cmd = 0x%x, 0%x, 0x%x\n", func_ctrl[i].name, func_ctrl[i].len,
		func_ctrl[i].cmd[0], func_ctrl[i].cmd[1], func_ctrl[i].cmd[2]);

	ret = ili9881x_ts->wrapper(func_ctrl[i].cmd, func_ctrl[i].len, NULL, 0, ON);
	if (ret < 0)
		ILI_ERR("Write TP function failed\n");

out:
	return ret;
}

int ili_ic_code_reset(void)
{
	int ret;
	bool ice = atomic_read(&ili9881x_ts->ice_stat);

	if (!ice)
		if (ili_ice_mode_ctrl(ENABLE, OFF) < 0)
			ILI_ERR("Enable ice mode failed before code reset\n");

	ret = ili_ice_mode_write(0x40040, 0xAE, 1);
	if (ret < 0)
		ILI_ERR("ic code reset failed\n");

	if (!ice)
		if (ili_ice_mode_ctrl(DISABLE, OFF) < 0)
			ILI_ERR("Enable ice mode failed after code reset\n");
	return ret;
}

int ili_ic_whole_reset(void)
{
	int ret = 0;
	bool ice = atomic_read(&ili9881x_ts->ice_stat);

	if (!ice)
		if (ili_ice_mode_ctrl(ENABLE, OFF) < 0)
			ILI_ERR("Enable ice mode failed before chip reset\n");

	ILI_INFO("ic whole reset key = 0x%x, edge_delay = %d\n",
		ili9881x_ts->chip->reset_key, ili9881x_ts->rst_edge_delay);

	ret = ili_ice_mode_write(ili9881x_ts->chip->reset_addr, ili9881x_ts->chip->reset_key, sizeof(u32));
	if (ret < 0) {
		ILI_ERR("ic whole reset failed\n");
		goto out;
	}

	/* Need accurate power sequence, do not change it to msleep */
	mdelay(ili9881x_ts->rst_edge_delay);

out:
	if (!ice)
		if (ili_ice_mode_ctrl(DISABLE, OFF) < 0)
			ILI_ERR("Enable ice mode failed after chip reset\n");

	return ret;
}

static void ilitek_tddi_ic_wr_pack(int packet)
{
	int retry = 100;
	u32 reg_data = 0;

	while (retry--) {
		if (ili_ice_mode_read(0x73010, &reg_data, sizeof(u8)) < 0)
			ILI_ERR("Read 0x73010 error\n");

		if ((reg_data & 0x02) == 0) {
			ILI_INFO("check ok 0x73010 read 0x%X retry = %d\n", reg_data, retry);
			break;
		}
		mdelay(10);
	}

	if (retry <= 0)
		ILI_INFO("check 0x73010 error read 0x%X\n", reg_data);

	if (ili_ice_mode_write(0x73000, packet, 4) < 0)
		ILI_ERR("Write %x at 0x73000\n", packet);
}

static u32 ilitek_tddi_ic_rd_pack(int packet)
{
	int retry = 100;
	u32 reg_data = 0;

	ilitek_tddi_ic_wr_pack(packet);

	while (retry--) {
		if (ili_ice_mode_read(0x4800A, &reg_data, sizeof(u8)) < 0)
			ILI_ERR("Read 0x4800A error\n");

		if ((reg_data & 0x02) == 0x02) {
			ILI_INFO("check  ok 0x4800A read 0x%X retry = %d\n", reg_data, retry);
			break;
		}
		mdelay(10);
	}
	if (retry <= 0)
		ILI_INFO("check 0x4800A error read 0x%X\n", reg_data);

	if (ili_ice_mode_write(0x4800A, 0x02, 1) < 0)
		ILI_ERR("Write 0x2 at 0x4800A\n");

	if (ili_ice_mode_read(0x73016, &reg_data, sizeof(u8)) < 0)
		ILI_ERR("Read 0x73016 error\n");

	return reg_data;
}

void ili_ic_set_ddi_reg_onepage(u8 page, u8 reg, u8 data)
{
	int wdt;
	u32 setpage = 0x1FFFFF00 | page;
	u32 setreg = 0x1F000100 | (reg << 16) | data;
	bool ice = atomic_read(&ili9881x_ts->ice_stat);

	ILI_INFO("setpage =  0x%X setreg = 0x%X\n", setpage, setreg);

	if (!ice)
		if (ili_ice_mode_ctrl(ENABLE, OFF) < 0)
			ILI_ERR("Enable ice mode failed before writing ddi reg\n");

	wdt = ili_ic_watch_dog_ctrl(ILI_READ, DISABLE);
	if (wdt)
		if (ili_ic_watch_dog_ctrl(ILI_WRITE, DISABLE) < 0)
			ILI_ERR("Disable WDT failed before writing ddi reg\n");

	/*TDI_WR_KEY*/
	ilitek_tddi_ic_wr_pack(0x1FFF9527);
	/*Switch to Page*/
	ilitek_tddi_ic_wr_pack(setpage);
	/* Page*/
	ilitek_tddi_ic_wr_pack(setreg);
	/*TDI_WR_KEY OFF*/
	ilitek_tddi_ic_wr_pack(0x1FFF9500);

	if (wdt)
		if (ili_ic_watch_dog_ctrl(ILI_WRITE, ENABLE) < 0)
			ILI_ERR("Enable WDT failed after writing ddi reg\n");

	if (!ice)
		if (ili_ice_mode_ctrl(DISABLE, OFF) < 0)
			ILI_ERR("Disable ice mode failed after writing ddi reg\n");
}

void ili_ic_get_ddi_reg_onepage(u8 page, u8 reg, u8 *data)
{
	int wdt;
	u32 setpage = 0x1FFFFF00 | page;
	u32 setreg = 0x2F000100 | (reg << 16);
	bool ice = atomic_read(&ili9881x_ts->ice_stat);

	ILI_INFO("setpage = 0x%X setreg = 0x%X\n", setpage, setreg);

	if (!ice)
		if (ili_ice_mode_ctrl(ENABLE, OFF) < 0)
			ILI_ERR("Enable ice mode failed before reading ddi reg\n");

	wdt = ili_ic_watch_dog_ctrl(ILI_READ, DISABLE);
	if (wdt)
		if (ili_ic_watch_dog_ctrl(ILI_WRITE, DISABLE) < 0)
			ILI_ERR("Disable WDT failed before reading ddi reg\n");

	/*TDI_WR_KEY*/
	ilitek_tddi_ic_wr_pack(0x1FFF9527);
	/*Set Read Page reg*/
	ilitek_tddi_ic_wr_pack(setpage);

	/*TDI_RD_KEY*/
	ilitek_tddi_ic_wr_pack(0x1FFF9487);
	/*( *( __IO uint8 *)	(0x4800A) ) =0x2*/
	if (ili_ice_mode_write(0x4800A, 0x02, 1) < 0)
		ILI_ERR("Write 0x2 at 0x4800A\n");

	*data = ilitek_tddi_ic_rd_pack(setreg);
	ILI_INFO("check page = 0x%X, reg = 0x%X, read 0x%X\n", page, reg, *data);

	/*TDI_RD_KEY OFF*/
	ilitek_tddi_ic_wr_pack(0x1FFF9400);
	/*TDI_WR_KEY OFF*/
	ilitek_tddi_ic_wr_pack(0x1FFF9500);

	if (wdt)
		if (ili_ic_watch_dog_ctrl(ILI_WRITE, ENABLE) < 0)
			ILI_ERR("Enable WDT failed after reading ddi reg\n");

	if (!ice)
		if (ili_ice_mode_ctrl(DISABLE, OFF) < 0)
			ILI_ERR("Disable ice mode failed after reading ddi reg\n");
}

void ili_ic_check_otp_prog_mode(void)
{
	int retry = 5;
	u32 prog_mode, prog_done;

	if (!ili9881x_ts->do_otp_check)
		return;

	if (ili_ice_mode_ctrl(ENABLE, OFF) < 0) {
		ILI_ERR("enter ice mode failed in otp\n");
		return;
	}

	if (ili_ic_watch_dog_ctrl(ILI_WRITE, DISABLE) < 0) {
		ILI_ERR("disable WDT failed in otp\n");
		return;
	}

	do {
		if (ili_ice_mode_write(0x43008, 0x80, 1) < 0)
			ILI_ERR("Write 0x80 at 0x43008 failed\n");

		if (ili_ice_mode_write(0x43030, 0x0, 1) < 0)
			ILI_ERR("Write 0x0 at 0x43030 failed\n");

		if (ili_ice_mode_write(0x4300C, 0x4, 1) < 0)
			ILI_ERR("Write 0x4 at 0x4300C failed\n");

		/* Need accurate power sequence, do not change it to msleep */
		mdelay(1);

		if (ili_ice_mode_write(0x4300C, 0x4, 1) < 0)
			ILI_ERR("Write 0x4 at 0x4300C\n");

		if (ili_ice_mode_read(0x43030, &prog_done, sizeof(u8)) < 0)
			ILI_ERR("Read prog_done error\n");

		if (ili_ice_mode_read(0x43008, &prog_mode, sizeof(u8)) < 0)
			ILI_ERR("Read prog_mode error\n");

		ILI_INFO("otp prog_mode = 0x%x, prog_done = 0x%x\n", prog_mode, prog_done);
		if (prog_done == 0x0 && prog_mode == 0x80)
			break;
	} while (--retry > 0);

	if (retry <= 0)
		ILI_ERR("OTP Program mode error!\n");
}

void ili_ic_spi_speed_ctrl(bool enable)
{
	ILI_INFO("%s spi speed up\n", (enable ? "Enable" : "Disable"));

	if (enable) {
		if (ili_ice_mode_write(0x063820, 0x00000101, 4) < 0)
			ILI_ERR("Write 0x00000101 at 0x063820 failed\n");

		if (ili_ice_mode_write(0x042c34, 0x00000008, 4) < 0)
			ILI_ERR("Write 0x00000008 at 0x042c34 failed\n");

		if (ili_ice_mode_write(0x063820, 0x00000000, 4) < 0)
			ILI_ERR("Write 0x00000000 at 0x063820 failed\n");
	} else {
		if (ili_ice_mode_write(0x063820, 0x00000101, 4) < 0)
			ILI_ERR("Write 0x00000101 at 0x063820 failed\n");

		if (ili_ice_mode_write(0x042c34, 0x00000000, 4) < 0)
			ILI_ERR("Write 0x00000000 at 0x042c34 failed\n");

		if (ili_ice_mode_write(0x063820, 0x00000000, 4) < 0)
			ILI_ERR("Write 0x00000000 at 0x063820 failed\n");
	}
}

void ili_ic_get_pc_counter(void)
{
	bool ice = atomic_read(&ili9881x_ts->ice_stat);
	u32 pc = 0, pc_addr = ili9881x_ts->chip->pc_counter_addr;
	u32 latch = 0, latch_addr = ili9881x_ts->chip->pc_latch_addr;

	if (!ice)
		if (ili_ice_mode_ctrl(ENABLE, OFF) < 0)
			ILI_ERR("Enable ice mode failed while reading pc counter\n");

	if (ili_ice_mode_read(ili9881x_ts->chip->pc_counter_addr, &pc, sizeof(u32)) < 0)
		ILI_ERR("Read pc conter error\n");

	if (ili_ice_mode_read(ili9881x_ts->chip->pc_latch_addr, &latch, sizeof(u32)) < 0)
		ILI_ERR("Read pc latch error\n");

	ili9881x_ts->fw_pc = pc;
	ili9881x_ts->fw_latch = latch;
	ILI_ERR("read pc (addr: 0x%x) = 0x%x\n", pc_addr, ili9881x_ts->fw_pc);
	ILI_ERR("read latch (addr: 0x%x) = 0x%x\n", latch_addr, ili9881x_ts->fw_latch);

	if (!ice)
		if (ili_ice_mode_ctrl(DISABLE, OFF) < 0)
			ILI_ERR("Disable ice mode failed while reading pc counter\n");
}


void ili_ic_get_pc_counter_forwdt(void)
{
	bool ice = atomic_read(&ili9881x_ts->ice_stat);
	u32 pc = 0, pc_addr = ili9881x_ts->chip->pc_counter_addr;
	u32 latch = 0, latch_addr = ili9881x_ts->chip->pc_latch_addr;

	if (!ice)
		if (ili_ice_mode_ctrl(ENABLE, OFF) < 0)
			ILI_ERR("Enable ice mode failed while reading pc counter\n");

	if (ili_ice_mode_read(ili9881x_ts->chip->pc_counter_addr, &pc, sizeof(u32)) < 0)
		ILI_ERR("Read pc conter error\n");

	if (ili_ice_mode_read(ili9881x_ts->chip->pc_latch_addr, &latch, sizeof(u32)) < 0)
		ILI_ERR("Read pc latch error\n");

	ili9881x_ts->fw_pc = pc;
	ili9881x_ts->fw_latch = latch;
	ILI_ERR("read pc (addr: 0x%x) = 0x%x\n", pc_addr, ili9881x_ts->fw_pc);
	ILI_ERR("read latch (addr: 0x%x) = 0x%x\n", latch_addr, ili9881x_ts->fw_latch);

	atomic_set(&ili9881x_ts->ice_stat, DISABLE);
}

int ili_ic_check_int_stat(void)
{
	/* From FW request, timeout should at least be 3 sec */
	if (!wait_event_interruptible_timeout(ili9881x_ts->inq, !atomic_read(&ili9881x_ts->cmd_int_check), msecs_to_jiffies(3000))) {
		ILI_ERR("Error! cmd int isn't received\n");
		atomic_set(&ili9881x_ts->cmd_int_check, DISABLE);
		return -1;
	}

	ILI_DBG("cmd int is active\n");
	return 0;
}

int ili_ic_check_busy(int count, int delay)
{
	u8 cmd = P5_X_CDC_BUSY_STATE;
	u8 busy = 0, rby = 0;

	if (ili9881x_ts->actual_tp_mode == P5_X_FW_AP_MODE)
		rby = 0x41;
	else if (ili9881x_ts->actual_tp_mode == P5_X_FW_TEST_MODE)
		rby = 0x51;
	else {
		ILI_ERR("Unknown TP mode (0x%x)\n", ili9881x_ts->actual_tp_mode);
		return -EINVAL;
	}

	ILI_INFO("read byte = %x, delay = %d\n", rby, delay);

	do {
		if (ili9881x_ts->wrapper(&cmd, sizeof(cmd), &busy, sizeof(u8), ON) < 0)
			ILI_ERR("Read check busy failed\n");

		ILI_DBG("busy = 0x%x\n", busy);

		if (busy == rby) {
			ILI_INFO("Check busy free\n");
			return 0;
		}

		mdelay(delay);
	} while (--count > 0);

	ILI_ERR("Check busy (0x%x) timeout !\n", busy);
	ili_ic_get_pc_counter();
	return -1;
}

int ili_ic_get_project_id(u8 *pdata, int size)
{
	int i;
	u32 tmp;
	bool ice = atomic_read(&ili9881x_ts->ice_stat);

	if (!pdata) {
		ILI_ERR("pdata is null\n");
		return -ENOMEM;
	}

	ILI_INFO("Read size = %d\n", size);

	if (!ice)
		if (ili_ice_mode_ctrl(ENABLE, OFF) < 0)
			ILI_ERR("Enable ice mode failed while reading project id\n");

	if (ili_ice_mode_write(0x041000, 0x0, 1) < 0)
		ILI_ERR("Pull cs low failed\n");
	if (ili_ice_mode_write(0x041004, 0x66aa55, 3) < 0)
		ILI_ERR("Write key failed\n");

	if (ili_ice_mode_write(0x041008, 0x03, 1) < 0)
		ILI_ERR("Write 0x03 at 0x041008\n");

	if (ili_ice_mode_write(0x041008, (RSV_BK_ST_ADDR & 0xFF0000) >> 16, 1) < 0)
		ILI_ERR("Write address failed\n");
	if (ili_ice_mode_write(0x041008, (RSV_BK_ST_ADDR & 0x00FF00) >> 8, 1) < 0)
		ILI_ERR("Write address failed\n");
	if (ili_ice_mode_write(0x041008, (RSV_BK_ST_ADDR & 0x0000FF), 1) < 0)
		ILI_ERR("Write address failed\n");

	for (i = 0; i < size; i++) {
		if (ili_ice_mode_write(0x041008, 0xFF, 1) < 0)
			ILI_ERR("Write dummy failed\n");
		if (ili_ice_mode_read(0x41010, &tmp, sizeof(u8)) < 0)
			ILI_ERR("Read project id error\n");
		pdata[i] = tmp;
		ILI_INFO("project_id[%d] = 0x%x\n", i, pdata[i]);
	}

	ili_flash_clear_dma();

	if (ili_ice_mode_write(0x041000, 0x1, 1) < 0)
		ILI_ERR("Pull cs high\n");

	if (!ice)
		if (ili_ice_mode_ctrl(DISABLE, OFF) < 0)
			ILI_ERR("Disable ice mode failed while reading project id\n");

	return 0;
}

int ili_ic_get_core_ver(void)
{
	int ret = 0;
	u8 cmd = P5_X_GET_CORE_VERSION;
	u8 buf[10] = {0};

	if (ili9881x_ts->info_from_hex) {
		buf[1] = ili9881x_ts->fw_info[68];
		buf[2] = ili9881x_ts->fw_info[69];
		buf[3] = ili9881x_ts->fw_info[70];
		goto out;
	}

	ret = ili9881x_ts->wrapper(&cmd, sizeof(cmd), buf, ili9881x_ts->protocol->core_ver_len, ON);
	if (ret < 0) {
		ILI_ERR("i2c/spi read core ver err\n");
		goto out;
	}

	if (buf[0] != cmd) {
		ILI_ERR("Invalid core ver\n");
		ret = -1;
	}

out:
	ILI_INFO("Core version = %d.%d.%d\n", buf[1], buf[2], buf[3]);
	ili9881x_ts->chip->core_ver = buf[1] << 16 | buf[2] << 8 | buf[3];
	return ret;
}

void ili_fw_uart_ctrl(u8 ctrl)
{
	u8 cmd[4] = {0};

	if (ctrl > 1 || ctrl < 0) {
		ILI_INFO("Unknown cmd, ignore\n");
		return;
	}

	ILI_INFO("%s UART mode\n", ctrl ? "Enable" : "Disable");

	cmd[0] = P5_X_I2C_UART;
	cmd[1] = 0x3;
	cmd[2] = 0;
	cmd[3] = ctrl;

	if (ili9881x_ts->wrapper(cmd, sizeof(cmd), NULL, 0, ON) < 0) {
		ILI_INFO("Write fw uart cmd failed\n");
		return;
	}

	ili9881x_ts->fw_uart_en = ctrl ? ENABLE : DISABLE;
}

int ili_ic_get_fw_ver(void)
{
	int ret = 0;
	u8 cmd = P5_X_GET_FW_VERSION;
	u8 buf[10] = {0};

	if (ili9881x_ts->info_from_hex) {
		buf[1] = ili9881x_ts->fw_info[48];
		buf[2] = ili9881x_ts->fw_info[49];
		buf[3] = ili9881x_ts->fw_info[50];
		buf[4] = ili9881x_ts->fw_info[51];
		goto out;
	}

	ret = ili9881x_ts->wrapper(&cmd, sizeof(cmd), buf, ili9881x_ts->protocol->fw_ver_len, ON);
	if (ret < 0) {
		ILI_ERR("i2c/spi read firmware ver err\n");
		goto out;
	}

	if (buf[0] != cmd) {
		ILI_ERR("Invalid firmware ver\n");
		ret = -1;
	}

out:
	ILI_INFO("Firmware version = %d.%d.%d.%d\n", buf[1], buf[2], buf[3], buf[4]);
	ili9881x_ts->chip->fw_ver = buf[1] << 24 | buf[2] << 16 | buf[3] << 8 | buf[4];

#ifdef CONFIG_TOUCHSCREEN_PROCFILE
	{
		uint8_t tp_info[64] = {0};

		sprintf(tp_info, 
				"[Vendor]InnoLux,[FW]0x%02x,[IC]ili%xx\n",
				buf[3],
				ili9881x_ts->chip->id);

		ILI_ERR("%s", tp_info);
		update_lct_tp_info(tp_info, NULL);
	}
#endif

	return ret;
}

int ili_ic_get_panel_info(void)
{
	int ret = 0;
	u8 cmd = P5_X_GET_PANEL_INFORMATION;
	u8 buf[10] = {0};
	u8 len = ili9881x_ts->protocol->panel_info_len;

	if (ili9881x_ts->info_from_hex && (ili9881x_ts->chip->core_ver >= CORE_VER_1410)) {
		buf[1] = ili9881x_ts->fw_info[16];
		buf[2] = ili9881x_ts->fw_info[17];
		buf[3] = ili9881x_ts->fw_info[18];
		buf[4] = ili9881x_ts->fw_info[19];
		ili9881x_ts->panel_wid = buf[2] << 8 | buf[1];
		ili9881x_ts->panel_hei = buf[4] << 8 | buf[3];
		goto out;
	}

	len = (ili9881x_ts->chip->core_ver >= CORE_VER_1430) ? 6 : len;

	ret = ili9881x_ts->wrapper(&cmd, sizeof(cmd), buf, len, ON);
	if (ret < 0)
		ILI_ERR("Read panel info error\n");

	if (buf[0] != cmd) {
		ILI_INFO("Invalid panel info, use default resolution\n");
		ili9881x_ts->panel_wid = TOUCH_SCREEN_X_MAX;
		ili9881x_ts->panel_hei = TOUCH_SCREEN_Y_MAX;
		ili9881x_ts->trans_xy = OFF;
	} else {
		ili9881x_ts->panel_wid = buf[1] << 8 | buf[2];
		ili9881x_ts->panel_hei = buf[3] << 8 | buf[4];
		ili9881x_ts->trans_xy = (ili9881x_ts->chip->core_ver >= CORE_VER_1430) ? buf[5] : OFF;
		ILI_INFO("Transfer touch coordinate = %s\n", ili9881x_ts->trans_xy ? "ON" : "OFF");
	}

out:
	ILI_INFO("Panel info: width = %d, height = %d\n", ili9881x_ts->panel_wid, ili9881x_ts->panel_hei);
	return ret;
}

int ili_ic_get_tp_info(void)
{
	int ret = 0;
	u8 cmd = P5_X_GET_TP_INFORMATION;
	u8 buf[20] = {0};

	if (ili9881x_ts->info_from_hex  && (ili9881x_ts->chip->core_ver >= CORE_VER_1410)) {
		buf[1] = ili9881x_ts->fw_info[5];
		buf[2] = ili9881x_ts->fw_info[7];
		buf[3] = ili9881x_ts->fw_info[8];
		buf[4] = ili9881x_ts->fw_info[9];
		buf[5] = ili9881x_ts->fw_info[10];
		buf[6] = ili9881x_ts->fw_info[11];
		buf[7] = ili9881x_ts->fw_info[12];
		buf[8] = ili9881x_ts->fw_info[14];
		buf[11] = buf[7];
		buf[12] = buf[8];
		goto out;
	}

	ret = ili9881x_ts->wrapper(&cmd, sizeof(cmd), buf, ili9881x_ts->protocol->tp_info_len, ON);
	if (ret < 0) {
		ILI_ERR("Read tp info error\n");
		goto out;
	}

	if (buf[0] != cmd) {
		ILI_ERR("Invalid tp info\n");
		ret = -1;
		goto out;
	}

out:
	ili9881x_ts->min_x = buf[1];
	ili9881x_ts->min_y = buf[2];
	ili9881x_ts->max_x = buf[4] << 8 | buf[3];
	ili9881x_ts->max_y = buf[6] << 8 | buf[5];
	ili9881x_ts->xch_num = buf[7];
	ili9881x_ts->ych_num = buf[8];
	ili9881x_ts->stx = buf[11];
	ili9881x_ts->srx = buf[12];

	ILI_INFO("TP Info: min_x = %d, min_y = %d, max_x = %d, max_y = %d\n", ili9881x_ts->min_x, ili9881x_ts->min_y, ili9881x_ts->max_x, ili9881x_ts->max_y);
	ILI_INFO("TP Info: xch = %d, ych = %d, stx = %d, srx = %d\n", ili9881x_ts->xch_num, ili9881x_ts->ych_num, ili9881x_ts->stx, ili9881x_ts->srx);
	return ret;
}

static void ilitek_tddi_ic_check_protocol_ver(u32 pver)
{
	int i = 0;

	if (ili9881x_ts->protocol->ver == pver) {
		ILI_INFO("same procotol version, do nothing\n");
		return;
	}

	for (i = 0; i < PROTOCL_VER_NUM - 1; i++) {
		if (protocol_info[i].ver == pver) {
			ili9881x_ts->protocol = &protocol_info[i];
			ILI_INFO("update protocol version = %x\n", ili9881x_ts->protocol->ver);
			return;
		}
	}

	ILI_INFO("Not found a correct protocol version in list, use newest version\n");
	ili9881x_ts->protocol = &protocol_info[PROTOCL_VER_NUM - 1];
}

int ili_ic_get_protocl_ver(void)
{
	int ret = 0;
	u8 cmd = P5_X_GET_PROTOCOL_VERSION;
	u8 buf[10] = {0};
	u32 ver;

	if (ili9881x_ts->info_from_hex) {
		buf[1] = ili9881x_ts->fw_info[72];
		buf[2] = ili9881x_ts->fw_info[73];
		buf[3] = ili9881x_ts->fw_info[74];
		goto out;
	}

	ret = ili9881x_ts->wrapper(&cmd, sizeof(cmd), buf, ili9881x_ts->protocol->pro_ver_len, ON);
	if (ret < 0) {
		ILI_ERR("Read protocol version error\n");
		goto out;
	}

	if (buf[0] != cmd) {
		ILI_ERR("Invalid protocol ver\n");
		ret = -1;
		goto out;
	}

out:
	ver = buf[1] << 16 | buf[2] << 8 | buf[3];

	ilitek_tddi_ic_check_protocol_ver(ver);

	ILI_INFO("Protocol version = %d.%d.%d\n", ili9881x_ts->protocol->ver >> 16,
		(ili9881x_ts->protocol->ver >> 8) & 0xFF, ili9881x_ts->protocol->ver & 0xFF);
	return ret;
}

int ili_ic_get_info(void)
{
	int ret = 0;
	u32 pid = ili9881x_ts->chip->pid;

	if (!atomic_read(&ili9881x_ts->ice_stat)) {
		ILI_ERR("ice mode doesn't enable\n");
		return -1;
	}

	if (!pid) {
		if (ili_ice_mode_read(ili9881x_ts->chip->pid_addr, &ili9881x_ts->chip->pid, sizeof(u32)) < 0)
			ILI_ERR("Read chip pid error\n");
	}
	if (ili_ice_mode_read(ili9881x_ts->chip->otp_addr, &ili9881x_ts->chip->otp_id, sizeof(u32)) < 0)
		ILI_ERR("Read otp id error\n");
	if (ili_ice_mode_read(ili9881x_ts->chip->ana_addr, &ili9881x_ts->chip->ana_id, sizeof(u32)) < 0)
		ILI_ERR("Read ana id error\n");

	ili9881x_ts->chip->id = pid >> 16;
	ili9881x_ts->chip->type = (pid & 0x0000FF00) >> 8;
	ili9881x_ts->chip->ver = pid & 0xFF;
	ili9881x_ts->chip->otp_id &= 0xFF;
	ili9881x_ts->chip->ana_id &= 0xFF;

	ILI_INFO("CHIP: PID = %x, ID = %x, TYPE = %x, VER = %x, OTP = %x, ANA = %x\n",
		ili9881x_ts->chip->pid,
		ili9881x_ts->chip->id,
		ili9881x_ts->chip->type,
		ili9881x_ts->chip->ver,
		ili9881x_ts->chip->otp_id,
		ili9881x_ts->chip->ana_id);

	ret = ilitek_tddi_ic_check_support(ili9881x_ts->chip->pid, ili9881x_ts->chip->id);
	return ret;
}

static struct ilitek_ic_info chip;

void ili_ic_init(void)
{
	chip.pid_addr =		   	TDDI_PID_ADDR;
	chip.wdt_addr =		   	TDDI_WDT_ADDR;
	chip.pc_counter_addr = 		TDDI_PC_COUNTER_ADDR;
	chip.pc_latch_addr =		TDDI_PC_LATCH_ADDR;
	chip.otp_addr =		   	TDDI_OTP_ID_ADDR;
	chip.ana_addr =		   	TDDI_ANA_ID_ADDR;
	chip.reset_addr =	   	TDDI_CHIP_RESET_ADDR;
	chip.spi_speed_ctrl =		DISABLE;

	ili9881x_ts->protocol = &protocol_info[PROTOCL_VER_NUM - 1];
	ili9881x_ts->chip = &chip;
}
