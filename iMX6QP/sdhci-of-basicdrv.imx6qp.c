// SPDX-License-Identifier: GPL-2.0
/*
 * Study of i.MX6QP SDHCI-PLTFM based Controller driver
 *
 * Copyright (c) 2018  alamy.liu@gmail.com
 */

#include <linux/module.h>
#include <linux/mmc/host.h>
#ifdef CONFIG_MMC_SDHCI_IO_ACCESSORS
#include <linux/mmc/mmc.h>
#endif
#include <linux/mmc/slot-gpio.h>
#include <linux/pinctrl/consumer.h>

/*
#include <linux/of_device.h>
*/
#include "sdhci-pltfm.h"


/********************************************************************
 * i.MX6QP related code
 ********************************************************************/
#include <linux/delay.h>
#include "sdhci-esdhc.h"

#ifdef CONFIG_MMC_SDHCI_IO_ACCESSORS
/*
 * There is an INT DMA ERR mismatch between eSDHC and STD SDHC SPEC:
 * Bit25 is used in STD SPEC, and is reserved in fsl eSDHC design,
 * but bit28 is used as the INT DMA ERR in fsl eSDHC design.
 * Define this macro DMA error INT for fsl eSDHC
 */
  #define ESDHC_INT_VENDOR_SPEC_DMA_ERR	(1 << 28)

#define	ESDHC_CTRL_D3CD			0x08

#define ESDHC_MIX_CTRL			0x48
#define  ESDHC_MIX_CTRL_DDREN		(1 << 3)
#define  ESDHC_MIX_CTRL_AC23EN		(1 << 7)
#define  ESDHC_MIX_CTRL_EXE_TUNE	(1 << 22)
#define  ESDHC_MIX_CTRL_SMPCLK_SEL	(1 << 23)
#define  ESDHC_MIX_CTRL_AUTO_TUNE_EN	(1 << 24)
#define  ESDHC_MIX_CTRL_FBCLK_SEL	(1 << 25)
#define  ESDHC_MIX_CTRL_HS400_EN	(1 << 26)
/* Bits 3 and 6 are not SDHCI standard definitions */
#define  ESDHC_MIX_CTRL_SDHCI_MASK	0xb7
/* Tuning bits */
#define  ESDHC_MIX_CTRL_TUNING_MASK	0x03c00000

#endif  /* CONFIG_MMC_SDHCI_IO_ACCESSORS */


/* HWInit */
#define ESDHC_WTMK_LVL			0x44
#define  ESDHC_WTMK_DEFAULT_VAL		0x10401040
#define ESDHC_BURST_LEN_EN_INCR		(1 << 27)
/* dll control register */
#define ESDHC_DLL_CTRL			0x60
#define ESDHC_DLL_OVERRIDE_VAL_SHIFT	9
#define ESDHC_DLL_OVERRIDE_EN_SHIFT	8

/* VENDOR SPEC register */
#define	ESDHC_VENDOR_SPEC		(0xc0)
#define	ESDHC_VENDOR_SPEC_SDIO_QUIRK	(1 << 1)
#define	ESDHC_VENDOR_SPEC_VSELECT	(1 << 1)
#define	ESDHC_VENDOR_SPEC_FRC_SDCLK_ON	(1 << 8)

/* Timeout */
#define ESDHC_SYS_CTRL_DTOCV_MASK	0x0f

/* tune control register */
#define ESDHC_TUNE_CTRL_STATUS		0x68
#define  ESDHC_TUNE_CTRL_STEP		1
#define  ESDHC_TUNE_CTRL_MIN		0
#define  ESDHC_TUNE_CTRL_MAX		((1 << 7) - 1)


static inline void esdhc_clrset(struct sdhci_host *host, u32 mask, u32 val, int reg)
{
	int reg_ofst = (reg & ~0x3);
	u32 reg_shft = (reg & 0x3) * 8;

	sdhci_writel(host,
		(sdhci_readl(host, reg_ofst) & ~(mask << reg_shft)) | (val << reg_shft),
		reg_ofst);
}

#ifdef CONFIG_MMC_SDHCI_IO_ACCESSORS
static u32 esdhc_readl(struct sdhci_host *host, int reg)
{
	u32 val = readl(host->ioaddr + reg);

	if (unlikely(reg == SDHCI_PRESENT_STATE)) {
		u32 fsl_prss = val;
		/* save the least 20 bits */
		val = fsl_prss & 0x000FFFFF;
		/* move dat[0-3] bits */
		val |= (fsl_prss & 0x0F000000) >> 4;
		/* move cmd line bit */
		val |= (fsl_prss & 0x00800000) << 1;
	}

	if (unlikely(reg == SDHCI_CAPABILITIES)) {
		/* In FSL esdhc IC module, only bit20 is used to indicate the
		 * ADMA2 capability of esdhc, but this bit is messed up on
		 * some SOCs (e.g. on MX25, MX35 this bit is set, but they
		 * don't actually support ADMA2). So set the BROKEN_ADMA
		 * quirk on MX25/35 platforms.
		 */
		if (val & SDHCI_CAN_DO_ADMA1) {
			val &= ~SDHCI_CAN_DO_ADMA1;
			val |= SDHCI_CAN_DO_ADMA2;
		}
	}

	if (unlikely(reg == SDHCI_CAPABILITIES_1)) {
		/* imx6q/dl does not have cap_1 register, fake one */
		val = SDHCI_SUPPORT_DDR50 | SDHCI_SUPPORT_SDR104
			| SDHCI_SUPPORT_SDR50
			| SDHCI_USE_SDR50_TUNING
			| (SDHCI_TUNING_MODE_3 << SDHCI_RETUNING_MODE_SHIFT);
	}

	if (unlikely(reg == SDHCI_MAX_CURRENT)) {
		val = 0;
		val |= 0xFF << SDHCI_MAX_CURRENT_330_SHIFT;
		val |= 0xFF << SDHCI_MAX_CURRENT_300_SHIFT;
		val |= 0xFF << SDHCI_MAX_CURRENT_180_SHIFT;
	}

	if (unlikely(reg == SDHCI_INT_STATUS)) {
		if (val & ESDHC_INT_VENDOR_SPEC_DMA_ERR) {
			val &= ~ESDHC_INT_VENDOR_SPEC_DMA_ERR;
			val |= SDHCI_INT_ADMA_ERROR;
		}
	}

	return val;
}

static void esdhc_writel(struct sdhci_host *host, u32 val, int reg)
{
    /* Interrupts: Bit-25 -> Bit-28 */
	if (unlikely(reg == SDHCI_INT_ENABLE || reg == SDHCI_SIGNAL_ENABLE ||
			reg == SDHCI_INT_STATUS)) {
		if (val & SDHCI_INT_ADMA_ERROR) {
			val &= ~SDHCI_INT_ADMA_ERROR;
			val |= ESDHC_INT_VENDOR_SPEC_DMA_ERR;
		}
	}

    writel(val, host->ioaddr + reg);
}

static u16 esdhc_readw(struct sdhci_host *host, int reg)
{
	u16 ret = 0;
	u32 val;

	if (unlikely(reg == SDHCI_HOST_VERSION)) {
		reg ^= 2;
		/*
		 * The usdhc register returns a wrong host version.
		 * Correct it here.
		 */
		return SDHCI_SPEC_300;
	}

	if (unlikely(reg == SDHCI_HOST_CONTROL2)) {
		val = readl(host->ioaddr + ESDHC_VENDOR_SPEC);
		if (val & ESDHC_VENDOR_SPEC_VSELECT)
			ret |= SDHCI_CTRL_VDD_180;

		val = readl(host->ioaddr + ESDHC_MIX_CTRL);
		if (val & ESDHC_MIX_CTRL_EXE_TUNE)
			ret |= SDHCI_CTRL_EXEC_TUNING;
		if (val & ESDHC_MIX_CTRL_SMPCLK_SEL)
			ret |= SDHCI_CTRL_TUNED_CLK;

		ret &= ~SDHCI_CTRL_PRESET_VAL_ENABLE;

		return ret;
	}

	if (unlikely(reg == SDHCI_TRANSFER_MODE)) {
		u32 m = readl(host->ioaddr + ESDHC_MIX_CTRL);
		ret = m & ESDHC_MIX_CTRL_SDHCI_MASK;
		/* Swap AC23 bit */
		if (m & ESDHC_MIX_CTRL_AC23EN) {
			ret &= ~ESDHC_MIX_CTRL_AC23EN;
			ret |= SDHCI_TRNS_AUTO_CMD23;
		}

		return ret;
	}

	return readw(host->ioaddr + reg);
}

static void esdhc_writew(struct sdhci_host *host, u16 val, int reg)
{
	u32 new_val = 0;

	switch (reg) {
	case SDHCI_CLOCK_CONTROL:
		new_val = readl(host->ioaddr + ESDHC_VENDOR_SPEC);
		if (val & SDHCI_CLOCK_CARD_EN)
			new_val |= ESDHC_VENDOR_SPEC_FRC_SDCLK_ON;
		else
			new_val &= ~ESDHC_VENDOR_SPEC_FRC_SDCLK_ON;
		writel(new_val, host->ioaddr + ESDHC_VENDOR_SPEC);
		return;
	case SDHCI_HOST_CONTROL2:
		new_val = readl(host->ioaddr + ESDHC_VENDOR_SPEC);
		if (val & SDHCI_CTRL_VDD_180)
			new_val |= ESDHC_VENDOR_SPEC_VSELECT;
		else
			new_val &= ~ESDHC_VENDOR_SPEC_VSELECT;
		writel(new_val, host->ioaddr + ESDHC_VENDOR_SPEC);
		new_val = readl(host->ioaddr + ESDHC_MIX_CTRL);
		if (val & SDHCI_CTRL_TUNED_CLK) {
			new_val |= ESDHC_MIX_CTRL_SMPCLK_SEL;
			new_val |= ESDHC_MIX_CTRL_AUTO_TUNE_EN;
		} else {
			new_val &= ~ESDHC_MIX_CTRL_SMPCLK_SEL;
			new_val &= ~ESDHC_MIX_CTRL_AUTO_TUNE_EN;
		}
		writel(new_val , host->ioaddr + ESDHC_MIX_CTRL);

		return;
	case SDHCI_TRANSFER_MODE:
		new_val = readl(host->ioaddr + ESDHC_MIX_CTRL);
		/* Swap AC23 bit */
		if (val & SDHCI_TRNS_AUTO_CMD23) {
			val &= ~SDHCI_TRNS_AUTO_CMD23;
			val |= ESDHC_MIX_CTRL_AC23EN;
		}
		new_val = val | (new_val & ~ESDHC_MIX_CTRL_SDHCI_MASK);
		writel(new_val, host->ioaddr + ESDHC_MIX_CTRL);
		return;
	case SDHCI_COMMAND:
		if (host->cmd->opcode == MMC_STOP_TRANSMISSION)
			val |= SDHCI_CMD_ABORTCMD;

		writel(val << 16,
		       host->ioaddr + SDHCI_TRANSFER_MODE);
		return;
	case SDHCI_BLOCK_SIZE:
		val &= ~SDHCI_MAKE_BLKSZ(0x7, 0);
		break;
	}
	esdhc_clrset(host, 0xffff, val, reg);
}

static u8 esdhc_readb(struct sdhci_host *host, int reg)
{
	u8 ret;
	u32 val;

	switch (reg) {
	case SDHCI_HOST_CONTROL:
		val = readl(host->ioaddr + reg);

		ret = val & SDHCI_CTRL_LED;
		ret |= (val >> 5) & SDHCI_CTRL_DMA_MASK;
		ret |= (val & ESDHC_CTRL_4BITBUS);
		ret |= (val & ESDHC_CTRL_8BITBUS) << 3;
		return ret;
	}

	return readb(host->ioaddr + reg);
}

static void esdhc_writeb(struct sdhci_host *host, u8 val, int reg)
{
	u32 new_val = 0;
	u32 mask;

	switch (reg) {
	case SDHCI_POWER_CONTROL:
		/*
		 * FSL put some DMA bits here
		 * If your board has a regulator, code should be here
		 */
		return;
	case SDHCI_HOST_CONTROL:
		/* FSL messed up here, so we need to manually compose it. */
		new_val = val & SDHCI_CTRL_LED;
		/* ensure the endianness */
		new_val |= ESDHC_HOST_CONTROL_LE;
		/* DMA mode bits are shifted */
		new_val |= (val & SDHCI_CTRL_DMA_MASK) << 5;

		/*
		 * Do not touch buswidth bits here. This is done in
		 * esdhc_pltfm_bus_width.
		 * Do not touch the D3CD bit either which is used for the
		 * SDIO interrupt erratum workaround.
		 */
		mask = 0xffff & ~(ESDHC_CTRL_BUSWIDTH_MASK | ESDHC_CTRL_D3CD);

		esdhc_clrset(host, mask, new_val, reg);
		return;
	case SDHCI_SOFTWARE_RESET:
		if (val & SDHCI_RESET_DATA)
			new_val = readl(host->ioaddr + SDHCI_HOST_CONTROL);
		break;
	}
	esdhc_clrset(host, 0xff, val, reg);

	if (reg == SDHCI_SOFTWARE_RESET) {
		if (val & SDHCI_RESET_ALL) {
			/*
			 * The esdhc has a design violation to SDHC spec which
			 * tells that software reset should not affect card
			 * detection circuit. But esdhc clears its SYSCTL
			 * register bits [0..2] during the software reset. This
			 * will stop those clocks that card detection circuit
			 * relies on. To work around it, we turn the clocks on
			 * back to keep card detection circuit functional.
			 */
			esdhc_clrset(host, 0x7, 0x7, ESDHC_SYSTEM_CONTROL);
			/*
			 * The reset on usdhc fails to clear MIX_CTRL register.
			 * Do it manually here.
			 */
			/*
			 * the tuning bits should be kept during reset
			 */
			new_val = readl(host->ioaddr + ESDHC_MIX_CTRL);
			writel(new_val & ESDHC_MIX_CTRL_TUNING_MASK,
					host->ioaddr + ESDHC_MIX_CTRL);
		} else if (val & SDHCI_RESET_DATA) {
			/*
			 * The eSDHC DAT line software reset clears at least the
			 * data transfer width on i.MX25, so make sure that the
			 * Host Control register is unaffected.
			 */
			esdhc_clrset(host, 0xff, new_val,
					SDHCI_HOST_CONTROL);
		}
	}
}
#endif

static void imx6q_basicdrv_reset(struct sdhci_host *host, u8 mask)
{
	pr_debug("%s\n", mmc_hostname(host->mmc));

	sdhci_reset(host, mask);

	sdhci_writel(host, host->ier, SDHCI_INT_ENABLE);
	sdhci_writel(host, host->ier, SDHCI_SIGNAL_ENABLE);
}

static unsigned int imx6q_basicdrv_get_max_timeout_count(struct sdhci_host *host)
{
	pr_debug("%s\n", mmc_hostname(host->mmc));

	/* Doc Erratum: the uSDHC actual maximum timeout count is 1 << 29 */
	return (1 << 29);
}

static void imx6q_basicdrv_set_timeout(struct sdhci_host *host, struct mmc_command *cmd)
{
	pr_debug("%s\n", mmc_hostname(host->mmc));

	/* use maximum timeout counter */
	esdhc_clrset(host,
			ESDHC_SYS_CTRL_DTOCV_MASK, 0xF,
			SDHCI_TIMEOUT_CONTROL);
}

#define IMX6Q_HOST_CLOCK		(198000000)

static unsigned int imx6q_basicdrv_get_max_clock(struct sdhci_host *host)
{
	pr_debug("%s: max clock=%d\n", mmc_hostname(host->mmc), IMX6Q_HOST_CLOCK);

	return IMX6Q_HOST_CLOCK;   /* Hacking */
}

static unsigned int imx6q_basicdrv_get_min_clock(struct sdhci_host *host)
{
	pr_debug("%s: min clock=%d\n", mmc_hostname(host->mmc),
		IMX6Q_HOST_CLOCK / 256 / 16);

	return IMX6Q_HOST_CLOCK / 256 / 16;
}

static void imx6q_basicdrv_set_clock(
	struct sdhci_host *host,
	unsigned int clock)
{
	unsigned int host_clock = IMX6Q_HOST_CLOCK;   /* Hacking */

/*	int ddr_pre_div = imx_data->is_ddr ? 2 : 1; */
	int ddr_pre_div = 1;
	int pre_div = 1;
	int div = 1;
	u32 temp, val;

	if (clock == 0) {
		host->mmc->actual_clock = 0;

		val = sdhci_readl(host, ESDHC_VENDOR_SPEC);
		sdhci_writel(host, val & ~ESDHC_VENDOR_SPEC_FRC_SDCLK_ON,
				ESDHC_VENDOR_SPEC);
		return;
	}

	temp = sdhci_readl(host, ESDHC_SYSTEM_CONTROL);
	temp &= ~(ESDHC_CLOCK_IPGEN | ESDHC_CLOCK_HCKEN | ESDHC_CLOCK_PEREN
		| ESDHC_CLOCK_MASK);
	sdhci_writel(host, temp, ESDHC_SYSTEM_CONTROL);

	while (host_clock / (16 * pre_div * ddr_pre_div) > clock &&
			pre_div < 256)
		pre_div *= 2;

	while (host_clock / (div * pre_div * ddr_pre_div) > clock && div < 16)
		div++;

	host->mmc->actual_clock = host_clock / (div * pre_div * ddr_pre_div);
	dev_dbg(mmc_dev(host->mmc), "desired SD clock: %d, actual: %d\n",
		clock, host->mmc->actual_clock);

	pre_div >>= 1;
	div--;

	temp = sdhci_readl(host, ESDHC_SYSTEM_CONTROL);
	temp |= (ESDHC_CLOCK_IPGEN | ESDHC_CLOCK_HCKEN | ESDHC_CLOCK_PEREN
		| (div << ESDHC_DIVIDER_SHIFT)
		| (pre_div << ESDHC_PREDIV_SHIFT));
	sdhci_writel(host, temp, ESDHC_SYSTEM_CONTROL);

	val = sdhci_readl(host, ESDHC_VENDOR_SPEC);
	sdhci_writel(host, val | ESDHC_VENDOR_SPEC_FRC_SDCLK_ON,
		ESDHC_VENDOR_SPEC);

	mdelay(1);
}

static unsigned int imx6q_basicdrv_get_ro(struct sdhci_host *host)
{
	pr_debug("%s\n", mmc_hostname(host->mmc));

	return mmc_gpio_get_ro(host->mmc);
}

static void imx6q_basicdrv_set_bus_width(struct sdhci_host *host, int width)
{
	u32 ctrl;

	switch (width) {
	case MMC_BUS_WIDTH_8:
		ctrl = ESDHC_CTRL_8BITBUS;
		break;
	case MMC_BUS_WIDTH_4:
		ctrl = ESDHC_CTRL_4BITBUS;
		break;
	default:
		ctrl = 0;
		break;
	}

	esdhc_clrset(host, ESDHC_CTRL_BUSWIDTH_MASK, ctrl,
		SDHCI_HOST_CONTROL);
}

static void imx6q_basicdrv_reset_tuning(struct sdhci_host *host)
{
	u32 ctrl;

	/* Reset the tuning circuit */
	ctrl = readl(host->ioaddr + ESDHC_MIX_CTRL);
	ctrl &= ~ESDHC_MIX_CTRL_SMPCLK_SEL;
	ctrl &= ~ESDHC_MIX_CTRL_FBCLK_SEL;
	writel(ctrl, host->ioaddr + ESDHC_MIX_CTRL);
	writel(0, host->ioaddr + ESDHC_TUNE_CTRL_STATUS);
}

static void imx6q_basicdrv_set_uhs_signaling(
	struct sdhci_host *host,
	unsigned timing)
{
	imx6q_basicdrv_reset_tuning(host);

/*
	esdhc_change_pinstate(host, timing);
*/
}

static void imx6q_basicdrv_prepare_tuning(struct sdhci_host *host, u32 val)
{
	u32 reg;

	/* FIXME: delay a bit for card to be ready for next tuning due to errors */
	mdelay(1);

	reg = readl(host->ioaddr + ESDHC_MIX_CTRL);
	reg |= ESDHC_MIX_CTRL_EXE_TUNE | ESDHC_MIX_CTRL_SMPCLK_SEL |
			ESDHC_MIX_CTRL_FBCLK_SEL;
	writel(reg, host->ioaddr + ESDHC_MIX_CTRL);
	writel(val << 8, host->ioaddr + ESDHC_TUNE_CTRL_STATUS);
	dev_dbg(mmc_dev(host->mmc),
		"tuning with delay 0x%x ESDHC_TUNE_CTRL_STATUS 0x%x\n",
			val, readl(host->ioaddr + ESDHC_TUNE_CTRL_STATUS));
}

static void imx6q_basicdrv_post_tuning(struct sdhci_host *host)
{
	u32 reg;

	reg = readl(host->ioaddr + ESDHC_MIX_CTRL);
	reg &= ~ESDHC_MIX_CTRL_EXE_TUNE;
	reg |= ESDHC_MIX_CTRL_AUTO_TUNE_EN;
	writel(reg, host->ioaddr + ESDHC_MIX_CTRL);
}

static int imx6q_basicdrv_executing_tuning(struct sdhci_host *host, u32 opcode)
{
	int min, max, avg, ret;

	/* find the mininum delay first which can pass tuning */
	min = ESDHC_TUNE_CTRL_MIN;
	while (min < ESDHC_TUNE_CTRL_MAX) {
		imx6q_basicdrv_prepare_tuning(host, min);
		if (!mmc_send_tuning(host->mmc, opcode, NULL))
			break;
		min += ESDHC_TUNE_CTRL_STEP;
	}

	/* find the maxinum delay which can not pass tuning */
	max = min + ESDHC_TUNE_CTRL_STEP;
	while (max < ESDHC_TUNE_CTRL_MAX) {
		imx6q_basicdrv_prepare_tuning(host, max);
		if (mmc_send_tuning(host->mmc, opcode, NULL)) {
			max -= ESDHC_TUNE_CTRL_STEP;
			break;
		}
		max += ESDHC_TUNE_CTRL_STEP;
	}

	/* use average delay to get the best timing */
	avg = (min + max) / 2;
	imx6q_basicdrv_prepare_tuning(host, avg);
	ret = mmc_send_tuning(host->mmc, opcode, NULL);
	imx6q_basicdrv_post_tuning(host);

	dev_dbg(mmc_dev(host->mmc), "tuning %s at 0x%x ret %d\n",
		ret ? "failed" : "passed", avg, ret);

	return ret;
}


static const struct sdhci_ops sdhci_basicdrv_ops = {
#ifdef CONFIG_MMC_SDHCI_IO_ACCESSORS
	.read_l = esdhc_readl,
	.read_w = esdhc_readw,
	.read_b = esdhc_readb,
	.write_l = esdhc_writel,
	.write_w = esdhc_writew,
	.write_b = esdhc_writeb,
#endif

	.reset = imx6q_basicdrv_reset,
	.get_max_timeout_count = imx6q_basicdrv_get_max_timeout_count,
	.set_timeout = imx6q_basicdrv_set_timeout,
	.get_max_clock = imx6q_basicdrv_get_max_clock,
	.get_min_clock = imx6q_basicdrv_get_min_clock,
	.set_clock = imx6q_basicdrv_set_clock,
	.get_ro = imx6q_basicdrv_get_ro,
	.set_bus_width = imx6q_basicdrv_set_bus_width,
	.set_uhs_signaling = imx6q_basicdrv_set_uhs_signaling,
	.platform_execute_tuning = imx6q_basicdrv_executing_tuning,

/*  ***WARNING: TBD (sdhci-pci-core.c)
	.enable_dma	= sdhci_pci_enable_dma, (synopsys)
	.hw_reset		= sdhci_pci_hw_reset,
	.platform_execute_tuning	= amd_execute_tuning,
	.set_power		= sdhci_intel_set_power,
	.voltage_switch		= sdhci_intel_voltage_switch,
*/
};

static const struct sdhci_pltfm_data sdhci_basicdrv_pdata = {
	/* i.MX6QP quirks */
	.quirks = ESDHC_DEFAULT_QUIRKS | SDHCI_QUIRK_NO_HISPD_BIT
			| SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC
			| SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC
			| SDHCI_QUIRK_BROKEN_CARD_DETECTION,

	.ops = &sdhci_basicdrv_ops,
};

static void imx6q_basicdrv_hwinit(struct sdhci_host *host)
{
	/*
	 * The imx6q ROM code will change the default watermark
	 * level setting to something insane.  Change it back here.
	 */
	writel(ESDHC_WTMK_DEFAULT_VAL, host->ioaddr + ESDHC_WTMK_LVL);

	/*
	 * ROM code will change the bit burst_length_enable setting
	 * to zero if this usdhc is chosen to boot system. Change
	 * it back here, otherwise it will impact the performance a
	 * lot. This bit is used to enable/disable the burst length
	 * for the external AHB2AXI bridge. It's useful especially
	 * for INCR transfer because without burst length indicator,
	 * the AHB2AXI bridge does not know the burst length in
	 * advance. And without burst length indicator, AHB INCR
	 * transfer can only be converted to singles on the AXI side.
	 */
	writel(readl(host->ioaddr + SDHCI_HOST_CONTROL)
		| ESDHC_BURST_LEN_EN_INCR,
		host->ioaddr + SDHCI_HOST_CONTROL);
	/*
	* erratum ESDHC_FLAG_ERR004536 fix for MX6Q TO1.2 and MX6DL
	* TO1.1, it's harmless for MX6SL
	*/
	writel(readl(host->ioaddr + 0x6c) | BIT(7),
		host->ioaddr + 0x6c);

	/* disable DLL_CTRL delay line settings */
	writel(0x0, host->ioaddr + ESDHC_DLL_CTRL);
}

static int sdhci_basicdrv_probe(struct platform_device *pdev)
{
	struct sdhci_host *host;
	int ret = 0;
	struct clk *clk_ipg, *clk_ahb, *clk_per;
	struct pinctrl *pins_default;

	host = sdhci_pltfm_init(pdev, &sdhci_basicdrv_pdata, 0);
	if (IS_ERR(host))
		return PTR_ERR(host);


	/* Enable clock sources (IPG, AHB, PER) */
	clk_ipg = devm_clk_get(&pdev->dev, "ipg");
	clk_ahb = devm_clk_get(&pdev->dev, "ahb");
	clk_per = devm_clk_get(&pdev->dev, "per");
	if (IS_ERR(clk_ipg) || IS_ERR(clk_ahb) || IS_ERR(clk_per)) {
		dev_err(&pdev->dev, "Unable to fetch IPG/AHB/PER clock resources\n");
		goto err;
	}
	if (clk_prepare_enable(clk_per))	goto clk_err;
	if (clk_prepare_enable(clk_ipg))	goto clk_err;
	if (clk_prepare_enable(clk_ahb))	goto clk_err;

	/* Set PINCTRL */
	pins_default = devm_pinctrl_get_select_default(&pdev->dev);

	/* Update QUIRKS */
	host->quirks2 |= SDHCI_QUIRK2_PRESET_VALUE_BROKEN;
	host->mmc->caps |= MMC_CAP_1_8V_DDR;
	host->quirks2 |= SDHCI_QUIRK2_BROKEN_HS200;

	/* clear tuning bits in case ROM has set it already */
	writel(0x0, host->ioaddr + ESDHC_MIX_CTRL);
	writel(0x0, host->ioaddr + SDHCI_ACMD12_ERR);
	writel(0x0, host->ioaddr + ESDHC_TUNE_CTRL_STATUS);


	sdhci_get_of_property(pdev);

	ret = mmc_of_parse(host->mmc);
	if (ret) {
		dev_err(&pdev->dev, "parsing dt failed (%d)\n", ret);
		goto err;
	}


	/* Errata */
	imx6q_basicdrv_hwinit(host);


	ret = sdhci_add_host(host);
	if (ret)
		goto err;

	return 0;

clk_err:
	clk_disable_unprepare(clk_ahb);
	clk_disable_unprepare(clk_ipg);
	clk_disable_unprepare(clk_per);
err:
	sdhci_pltfm_free(pdev);

	return ret;
}

static const struct of_device_id sdhci_basicdrv_of_match[] = {
	{ .compatible = "virtualcom,basicdrv-dwc_mshc" },
	{ .compatible = "virtualcom,basicdrv-sdhci" },
	{ }
};
MODULE_DEVICE_TABLE(of, sdhci_basicdrv_of_match);

static struct platform_driver sdhci_basicdrv_driver = {
	.driver = {
		.name = "sdhci-basicdrv",
		.of_match_table = sdhci_basicdrv_of_match,
		.pm = &sdhci_pltfm_pmops,
	},
	.probe = sdhci_basicdrv_probe,
	.remove = sdhci_pltfm_unregister,
};

module_platform_driver(sdhci_basicdrv_driver);

MODULE_DESCRIPTION("SDHCI OF driver for VirtualCOM BasicDrv DWC_mshc");
MODULE_AUTHOR("Public Domain Virtual Company");
MODULE_LICENSE("GPL v2");
