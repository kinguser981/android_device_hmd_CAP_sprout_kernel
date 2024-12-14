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

struct touch_bus_info {
	struct spi_driver bus_driver;
	struct ilitek_hwif_info *hwif;
};

struct ilitek_ts_data *ili9881x_ts;

#if SPI_DMA_TRANSFER_SPLIT
#define DMA_TRANSFER_MAX_CHUNK		64   // number of chunks to be transferred.
#define DMA_TRANSFER_MAX_LEN		1024 // length of a chunk.
static struct spi_transfer	xfer[DMA_TRANSFER_MAX_CHUNK];

int ili_spi_write_then_read_split(struct spi_device *spi,
		const void *txbuf, unsigned n_tx,
		void *rxbuf, unsigned n_rx)
{
	int status = -1, duplex_len = 0;
	int xfercnt = 0, xferlen = 0, xferloop = 0;
	u8 *dma_txbuf = NULL, *dma_rxbuf = NULL;
	u8 cmd;
	struct spi_message message;
	int index = 0;

	if (!atomic_read(&ili9881x_ts->ice_stat))
		index = 2;

	spi_message_init(&message);
	memset(xfer, 0, sizeof(xfer));
	memset(ili9881x_ts->spi_tx, 0x0, SPI_TX_BUF_SIZE);
	memset(ili9881x_ts->spi_rx, 0x0, SPI_RX_BUF_SIZE);

	dma_txbuf = ili9881x_ts->spi_tx;
	dma_rxbuf = ili9881x_ts->spi_rx + (SPI_BUF_SIZE >> 1);

	if ((n_tx == 1) && (n_rx == 1))
		cmd = SPI_READ;
	else if ((n_tx > 0) && (n_rx > 0))
		cmd = SPI_READ;
	else
		cmd = SPI_WRITE;

	switch (cmd) {
	case SPI_WRITE:
		if (n_tx % DMA_TRANSFER_MAX_LEN)
			xferloop = (n_tx / DMA_TRANSFER_MAX_LEN) + 1;
		else
			xferloop = n_tx / DMA_TRANSFER_MAX_LEN;

		xferlen = n_tx;
		memcpy(dma_txbuf, (u8 *)txbuf, xferlen);

		for (xfercnt = 0; xfercnt < xferloop; xfercnt++) {
			if (xferlen > DMA_TRANSFER_MAX_LEN)
				xferlen = DMA_TRANSFER_MAX_LEN;

			xfer[xfercnt].len = xferlen;
			xfer[xfercnt].tx_buf = dma_txbuf + xfercnt * DMA_TRANSFER_MAX_LEN;
			spi_message_add_tail(&xfer[xfercnt], &message);
			xferlen = n_tx - (xfercnt+1) * DMA_TRANSFER_MAX_LEN;
		}
		status = spi_sync(spi, &message);
		break;
	case SPI_READ:
		if (n_tx > DMA_TRANSFER_MAX_LEN) {
			ILI_ERR("Tx length must be lower than transfer length (%d).\n", DMA_TRANSFER_MAX_LEN);
			status = -EINVAL;
			break;
		}

		memcpy(dma_txbuf, txbuf, n_tx);
		duplex_len = n_tx + n_rx;

		if ((duplex_len + index) % DMA_TRANSFER_MAX_LEN)
			xferloop = ((duplex_len + index) / DMA_TRANSFER_MAX_LEN) + 1;
		else
			xferloop = (duplex_len + index) / DMA_TRANSFER_MAX_LEN;

		xferlen = (duplex_len + index);
		for (xfercnt = 0; xfercnt < xferloop; xfercnt++) {
			if (xferlen > DMA_TRANSFER_MAX_LEN)
				xferlen = DMA_TRANSFER_MAX_LEN;

			xfer[xfercnt].len = xferlen;
			xfer[xfercnt].tx_buf = dma_txbuf;
			xfer[xfercnt].rx_buf = dma_rxbuf + xfercnt * DMA_TRANSFER_MAX_LEN;
			spi_message_add_tail(&xfer[xfercnt], &message);
			xferlen = (duplex_len + index) - xfercnt * DMA_TRANSFER_MAX_LEN;
		}

		status = spi_sync(spi, &message);
		if (status == 0) {
			if (dma_rxbuf[1] != SPI_ACK && !atomic_read(&ili9881x_ts->ice_stat)) {
				status = DO_SPI_RECOVER;
				ILI_ERR("Do spi recovery: rxbuf[1] = 0x%x, ice = %d\n", dma_rxbuf[1], atomic_read(&ili9881x_ts->ice_stat));
				break;
			}

			memcpy((u8 *)rxbuf, dma_rxbuf + index + 1, n_rx);
		} else {
			ILI_ERR("spi read fail, status = %d, do spi recovery\n", status);
			status = DO_SPI_RECOVER;
		}
		break;
	default:
		ILI_INFO("Unknown command 0x%x\n", cmd);
		break;
	}

	return status;
}
#else
int ili_spi_write_then_read_direct(struct spi_device *spi,
		const void *txbuf, unsigned n_tx,
		void *rxbuf, unsigned n_rx)
{
	static DEFINE_MUTEX(lock);

	int i, status = -1, index = 1;
	u8 cmd;
	struct spi_message	message;
	struct spi_transfer	xfer;
	u8 *tmp = (u8 *)rxbuf;

	mutex_trylock(&lock);

	spi_message_init(&message);
	memset(&xfer, 0, sizeof(xfer));

	if (!atomic_read(&ili9881x_ts->ice_stat))
		index = 3;

	if ((n_tx == 1) && (n_rx == 1))
		cmd = SPI_READ;
	else if ((n_tx > 0) && (n_rx > 0))
		cmd = SPI_READ;
	else
		cmd = SPI_WRITE;

	switch (cmd) {
	case SPI_WRITE:
		xfer.len = n_tx;
		xfer.tx_buf = txbuf;
		spi_message_add_tail(&xfer, &message);
		status = spi_sync(spi, &message);
		break;
	case SPI_READ:
		xfer.len = n_tx + n_rx + index - 1;
		xfer.tx_buf = txbuf;
		memset(ili9881x_ts->update_buf, 0x0, MAX_HEX_FILE_SIZE);
		xfer.rx_buf = ili9881x_ts->update_buf;

		spi_message_add_tail(&xfer, &message);
		status = spi_sync(spi, &message);
		if (status != 0) {
			ILI_ERR("spi read fail, status = %d\n", status);
			break;
		}

		if (ili9881x_ts->update_buf[1] != SPI_ACK && !atomic_read(&ili9881x_ts->ice_stat)) {
			status = DO_SPI_RECOVER;
			ILI_ERR("Do spi recovery: rxbuf[0] = 0x%x, ice = %d\n", ili9881x_ts->update_buf[0], atomic_read(&ili9881x_ts->ice_stat));
			break;
		}
		//ili_dump_data(ili9881x_ts->update_buf, 8, 50, 0, "read org data");
		for (i = 0; i < (n_rx); i++)
			tmp[i] = ili9881x_ts->update_buf[i + n_tx + index - 1];

		//memcpy((u8 *)rxbuf, tmp, n_rx);
		break;
	default:
		ILI_INFO("Unknown command 0x%x\n", cmd);
		break;
	}

	mutex_unlock(&lock);
	return status;
}
#endif

static int ili_spi_mp_pre_cmd(u8 cdc)
{
	u8 pre[5] = {0};

	if (!atomic_read(&ili9881x_ts->mp_stat) || cdc != 0xF1 ||
		ili9881x_ts->chip->core_ver >= CORE_VER_1430)
		return 0;

	ILI_DBG("mp test with pre commands\n");

	pre[0] = SPI_WRITE;
	pre[1] = 0x0;// len low byte
	pre[2] = 0x2;// len high byte
	pre[3] = P5_X_READ_DATA_CTRL;
	pre[4] = P5_X_GET_CDC_DATA;
	if (ili9881x_ts->spi_write_then_read(ili9881x_ts->spi, pre, 5, NULL, 0) < 0) {
		ILI_ERR("Failed to write pre commands\n");
		return -1;
	}

	pre[0] = SPI_WRITE;
	pre[1] = 0x0;// len low byte
	pre[2] = 0x1;// len high byte
	pre[3] = P5_X_GET_CDC_DATA;
	if (ili9881x_ts->spi_write_then_read(ili9881x_ts->spi, pre, 4, NULL, 0) < 0) {
		ILI_ERR("Failed to write pre commands\n");
		return -1;
	}
	return 0;
}

#if 1
static int ili_spi_pll_clk_wakeup(void)
{
	int index = 0;
	u8 wdata[32] = {0};
	u8 wakeup[9] = {0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3, 0xA3};
	u32 wlen = sizeof(wakeup);

	ILI_INFO("\n");
	wdata[0] = SPI_WRITE;
	wdata[1] = wlen >> 8;
	wdata[2] = wlen & 0xff;
	index = 3;
	wlen += index;

	ipio_memcpy(&wdata[index], wakeup, wlen, wlen);

	if (ili9881x_ts->spi_write_then_read(ili9881x_ts->spi, wdata, wlen, NULL, 0) < 0) {
		ILI_ERR("spi slave write error\n");
		return -1;
	}

	return 0;
}
#endif

static int ili_spi_wrapper(u8 *txbuf, u32 wlen, u8 *rxbuf, u32 rlen, bool irq)
{
	int ret = 0;
	int mode = 0, index = 0;
	u8 wdata[32] = {0};
	bool ice = atomic_read(&ili9881x_ts->ice_stat);

	if (wlen > 0) {
		if (!txbuf) {
			ILI_ERR("txbuf is null\n");
			return -ENOMEM;
		}

		/* 3 bytes data consist of length and header */
		if ((wlen + 3) > sizeof(wdata)) {
			ILI_ERR("WARNING! wlen(%d) > wdata(%ld), using wdata length to transfer\n", wlen, sizeof(wdata));
			wlen = sizeof(wdata);
		}
	}

	if (rlen > 0) {
		if (!rxbuf) {
			ILI_ERR("rxbuf is null\n");
			return -ENOMEM;
		}
	}

	if (wlen > 0 && rlen > 0)
		mode = SPI_WRITE;
	else if (wlen > 0 && !rlen)
		mode = SPI_WRITE;
	else
		mode = SPI_READ;

	if (irq)
		atomic_set(&ili9881x_ts->cmd_int_check, ENABLE);

	switch (mode) {
	case SPI_WRITE:
		if (ice) {
			wdata[0] = SPI_WRITE;
			index = 1;
		} else {
			wdata[0] = SPI_WRITE;
			wdata[1] = wlen >> 8;
			wdata[2] = wlen & 0xff;
			index = 3;
		}

		wlen += index;

		ipio_memcpy(&wdata[index], txbuf, wlen, wlen);
#if 1
		if (ili9881x_ts->tp_suspend && !ice && !ili9881x_ts->skip_wake) {
			ret = ili_spi_pll_clk_wakeup();
			if (ret < 0) {
				ILI_ERR("Wakeup pll clk error\n");
				break;
			}
		}
#endif
		if (atomic_read(&ili9881x_ts->mp_stat))
			ili_dump_data(wdata, 8, wlen, 0, "write");

		ret = ili9881x_ts->spi_write_then_read(ili9881x_ts->spi, wdata, wlen, txbuf, 0);
		if (ret < 0) {
			ILI_INFO("spi-wrapper write error\n");
			break;
		}

		/* Won't break if it needs to read data following with writing. */
		if (!rlen)
			break;
	case SPI_READ:
		if (!ice && irq) {
			/* Check INT triggered by FW when sending cmds. */
			if (ili_ic_check_int_stat() < 0) {
				ILI_ERR("ERROR! Check INT timeout\n");
				ret = -ETIME;
				break;
			}
		}

		ret = ili_spi_mp_pre_cmd(wdata[3]);
		if (ret < 0)
			ILI_ERR("spi-wrapper mp pre cmd error\n");

		wdata[0] = SPI_READ;
		ili9881x_ts->spi_tx[0] = SPI_READ;
		ret = ili9881x_ts->spi_write_then_read(ili9881x_ts->spi, ili9881x_ts->spi_tx, 1, rxbuf, rlen);
		if (ret < 0)
			ILI_ERR("spi-wrapper read error\n");
		break;
	default:
		ILI_ERR("Unknown spi mode (%d)\n", mode);
		ret = -EINVAL;
		break;
	}

	if (irq)
		atomic_set(&ili9881x_ts->cmd_int_check, DISABLE);

	return ret;
}

static int core_spi_setup(u32 freq)
{
	ILI_INFO("spi clock = %d\n", freq);

	ili9881x_ts->spi->mode = SPI_MODE_0;
	ili9881x_ts->spi->bits_per_word = 8;
	ili9881x_ts->spi->max_speed_hz = freq;

	if (spi_setup(ili9881x_ts->spi) < 0) {
		ILI_ERR("Failed to setup spi device\n");
		return -ENODEV;
	}

	ILI_INFO("name = %s, bus_num = %d,cs = %d, mode = %d, speed = %d\n",
			ili9881x_ts->spi->modalias,
			ili9881x_ts->spi->master->bus_num,
			ili9881x_ts->spi->chip_select,
			ili9881x_ts->spi->mode,
			ili9881x_ts->spi->max_speed_hz);
	return 0;
}

#if ENABLE_GESTURE
int ili9881x_tp_gesture_callback(bool flag){
	bool enable = !!flag;

	if(enable == ili9881x_ts->gesture){
		return 0;
	}
	
	ili9881x_ts->gesture = enable;
#if REGULATOR_POWER
/*	if(ili9881x_ts->gesture){
		nvt_irq_enable(false);
		ts->is_gesture_mode = false;
	}
*/
	ili_plat_regulator_power_on(ili9881x_ts->gesture);
#endif

	return 0;
}
#endif

static int ilitek_spi_probe(struct spi_device *spi)
{
	struct touch_bus_info *info =
	container_of(to_spi_driver(spi->dev.driver),
		struct touch_bus_info, bus_driver);

	ILI_INFO("ilitek spi probe\n");

	if (!spi) {
		ILI_ERR("spi device is NULL\n");
		return -ENODEV;
	}

	spi->chip_select = 0; //for multi spi device

	ili9881x_ts = devm_kzalloc(&spi->dev, sizeof(struct ilitek_ts_data), GFP_KERNEL);
	if (ERR_ALLOC_MEM(ili9881x_ts)) {
		ILI_ERR("Failed to allocate ili9881x_ts memory, %ld\n", PTR_ERR(ili9881x_ts));
		return -ENOMEM;
	}

	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
		ILI_ERR("Full duplex not supported by master\n");
		return -EIO;
	}

	ili9881x_ts->update_buf = kzalloc(MAX_HEX_FILE_SIZE, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(ili9881x_ts->update_buf)) {
		ILI_ERR("fw kzalloc error\n");
		return -ENOMEM;
	}

	/* Used for receiving touch data only, do not mix up with others. */
	ili9881x_ts->tr_buf = kzalloc(TR_BUF_SIZE, GFP_ATOMIC);
	if (ERR_ALLOC_MEM(ili9881x_ts->tr_buf)) {
		ILI_ERR("failed to allocate touch report buffer\n");
		return -ENOMEM;
	}

	ili9881x_ts->spi_tx = kzalloc(SPI_TX_BUF_SIZE, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(ili9881x_ts->spi_tx)) {
		ILI_ERR("Failed to allocate spi tx buffer\n");
		return -ENOMEM;
	}

	ili9881x_ts->spi_rx = kzalloc(SPI_RX_BUF_SIZE, GFP_KERNEL | GFP_DMA);
	if (ERR_ALLOC_MEM(ili9881x_ts->spi_rx)) {
		ILI_ERR("Failed to allocate spi rx buffer\n");
		return -ENOMEM;
	}

	ili9881x_ts->gcoord = kzalloc(sizeof(struct gesture_coordinate), GFP_KERNEL);
	if (ERR_ALLOC_MEM(ili9881x_ts->gcoord)) {
		ILI_ERR("Failed to allocate gresture coordinate buffer\n");
		return -ENOMEM;
	}

	ili9881x_ts->i2c = NULL;
	ili9881x_ts->spi = spi;
	ili9881x_ts->dev = &spi->dev;
	ili9881x_ts->hwif = info->hwif;
	ili9881x_ts->phys = "SPI";
	ili9881x_ts->wrapper = ili_spi_wrapper;

#if SPI_DMA_TRANSFER_SPLIT
	ili9881x_ts->spi_write_then_read = ili_spi_write_then_read_split;
#else
	ili9881x_ts->spi_write_then_read = ili_spi_write_then_read_direct;
#endif

	ili9881x_ts->spi_speed = ili_ic_spi_speed_ctrl;
	ili9881x_ts->actual_tp_mode = P5_X_FW_AP_MODE;
	ili9881x_ts->tp_data_format = DATA_FORMAT_DEMO;
	ili9881x_ts->tp_data_len = P5_X_DEMO_MODE_PACKET_LEN;

	if (TDDI_RST_BIND)
		ili9881x_ts->reset = TP_IC_WHOLE_RST;
	else
		ili9881x_ts->reset = TP_HW_RST_ONLY;

	ili9881x_ts->rst_edge_delay = 5;
	ili9881x_ts->fw_open = REQUEST_FIRMWARE;
	ili9881x_ts->fw_upgrade_mode = UPGRADE_IRAM;
	ili9881x_ts->mp_move_code = ili_move_mp_code_iram;
	ili9881x_ts->gesture_move_code = ili_move_gesture_code_iram;
	ili9881x_ts->esd_recover = ili_wq_esd_spi_check;
	ili9881x_ts->ges_recover = ili_touch_esd_gesture_iram;
	ili9881x_ts->gesture_mode = DATA_FORMAT_GESTURE_INFO;
	ili9881x_ts->gesture_demo_ctrl = DISABLE;
	ili9881x_ts->wtd_ctrl = ON;
	ili9881x_ts->report = ENABLE;
	ili9881x_ts->netlink = DISABLE;
	ili9881x_ts->dnp = DISABLE;
	ili9881x_ts->irq_tirgger_type = IRQF_TRIGGER_FALLING;
	ili9881x_ts->info_from_hex = ENABLE;

	core_spi_setup(SPI_CLK);
	return info->hwif->plat_probe();
}

static int ilitek_spi_remove(struct spi_device *spi)
{
	ILI_INFO();
	return 0;
}

static struct spi_device_id tp_spi_id[] = {
	{TDDI_DEV_ID, 0},
	{},
};

int ili_interface_dev_init(struct ilitek_hwif_info *hwif)
{
	struct touch_bus_info *info;

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		ILI_ERR("faied to allocate spi_driver\n");
		return -ENOMEM;
	}

	if (hwif->bus_type != BUS_SPI) {
		ILI_ERR("Not SPI dev\n");
		return -EINVAL;
	}

	hwif->info = info;

	info->bus_driver.driver.name = hwif->name;
	info->bus_driver.driver.owner = hwif->owner;
	info->bus_driver.driver.of_match_table = hwif->of_match_table;

	info->bus_driver.probe = ilitek_spi_probe;
	info->bus_driver.remove = ilitek_spi_remove;
	info->bus_driver.id_table = tp_spi_id;

	info->hwif = hwif;
	return spi_register_driver(&info->bus_driver);
}

void ili_interface_dev_exit(struct ilitek_ts_data *ili9881x_ts)
{
	struct touch_bus_info *info = (struct touch_bus_info *)ili9881x_ts->hwif->info;

	ILI_INFO("remove spi dev\n");
	kfree(ili9881x_ts->update_buf);
	kfree(ili9881x_ts->spi_tx);
	kfree(ili9881x_ts->spi_rx);
	spi_unregister_driver(&info->bus_driver);
	ipio_kfree((void **)&info);
}
