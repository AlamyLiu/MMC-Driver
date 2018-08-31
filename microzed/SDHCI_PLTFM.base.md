# SDHCI-PLTFM based Initial Driver

## drivers/mmc/host/Kconfig
```sh
+config MMC_SDHCI_OF_STUDY
+	tristate "Study driver of SDHCI"
+	depends on MMC_SDHCI_PLTFM
+	depends on OF
+	depends on ARM
+	help
+	  This selects the Study driver of
+	  Secure Digital Host Controller Interface (SDHCI).
+
+	  If you have a controller with this interface, say Y or M here.
+
+	  If unsure, say N.
+
```

## drivers/mmc/host/Makefile
```sh
 obj-$(CONFIG_MMC_SDHCI_OF_ESDHC)	+= sdhci-of-esdhc.o
 obj-$(CONFIG_MMC_SDHCI_OF_HLWD)		+= sdhci-of-hlwd.o
+obj-$(CONFIG_MMC_SDHCI_OF_STUDY)	+= sdhci-of-study.o
 obj-$(CONFIG_MMC_SDHCI_BCM_KONA)	+= sdhci-bcm-kona.o
```

## drivers/mmc/host/sdhci-of-study.c
```sh
// SPDX-License-Identifier: GPL-2.0
/*
 * SDHCI driver study
 *
 * Copyright (c) 2018  Free Knowledge Tribe/Org. (www.freeknowledge.org)
 */

#include <linux/module.h>
#include <linux/mmc/host.h>
/*
#include <linux/of_device.h>
*/
#include "sdhci-pltfm.h"


static const struct sdhci_ops sdhci_study_ops = {
	.set_clock      = sdhci_set_clock,
	.set_bus_width  = sdhci_set_bus_width,
	.reset          = sdhci_reset,
	.set_uhs_signaling = sdhci_set_uhs_signaling,
	.set_power      = sdhci_set_power,
/*  ***WARNING: TBD
	.enable_dma     = sdhci_pci_enable_dma,
	.hw_reset       = sdhci_pci_hw_reset,
	.platform_execute_tuning = amd_execute_tuning,
	.set_power      = sdhci_intel_set_power,
	.voltage_switch = sdhci_intel_voltage_switch,
*/
};

static const struct sdhci_pltfm_data sdhci_study_pdata = {
/*
	.quirks = \
		SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN |
		SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER |
		SDHCI_QUIRK_32BIT_DMA_ADDR |
		SDHCI_QUIRK_32BIT_DMA_SIZE |
		SDHCI_QUIRK_BROKEN_CARD_DETECTION |
		SDHCI_QUIRK_NO_CARD_NO_RESET |
		SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
	.quirks2 = \
		SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
		SDHCI_QUIRK2_CLOCK_DIV_ZERO_BROKEN,
		SDHCI_QUIRK2_BROKEN_64_BIT_DMA |
		SDHCI_QUIRK2_BROKEN_DDR50,
*/
	.ops = &sdhci_study_ops,
};

static int sdhci_study_probe(struct platform_device *pdev)
{
	return sdhci_pltfm_register(pdev, &sdhci_study_pdata, 0);
}

static const struct of_device_id sdhci_study_of_match[] = {
	{ .compatible = "freeknowledge,study-sdhci" },
	{ }
};
MODULE_DEVICE_TABLE(of, sdhci_study_of_match);

static struct platform_driver sdhci_study_driver = {
	.driver = {
		.name = "sdhci-study",
		.of_match_table = sdhci_study_of_match,
		.pm = &sdhci_pltfm_pmops,
	},
	.probe = sdhci_study_probe,
	.remove = sdhci_pltfm_unregister,
};

module_platform_driver(sdhci_study_driver);

MODULE_DESCRIPTION("SDHCI OF driver study");
MODULE_AUTHOR("Free Knowledge");
MODULE_LICENSE("GPL v2");

```
