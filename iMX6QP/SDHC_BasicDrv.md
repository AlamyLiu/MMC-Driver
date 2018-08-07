# SDHCI-PLTFM based Basic Driver

## drivers/mmc/host/Kconfig
```sh
+config MMC_SDHCI_OF_BASICDRV
+       tristate "SDHCI OF support for the BasicDrv SD/SDIO/MMC controllers"
+       depends on MMC_SDHCI_PLTFM
+       depends on OF
+       depends on ARM
+       help
+         This selects the xxx Secure Digital Host Controller Interface
+         (SDHCI) on VirtualCOM VirtualMMC SoC.
+
+         If you have a controller with this interface, say Y or M here.
+
+         If unsure, say N.
+

```

## drivers/mmc/host/Makefile
```sh
 obj-$(CONFIG_MMC_SDHCI_OF_AT91)        += sdhci-of-at91.o
+obj-$(CONFIG_MMC_SDHCI_OF_BASICDRV)    += sdhci-of-basicdrv.o
 obj-$(CONFIG_MMC_SDHCI_OF_ESDHC)       += sdhci-of-esdhc.o
 obj-$(CONFIG_MMC_SDHCI_OF_HLWD)        += sdhci-of-hlwd.o
```

## drivers/mmc/host/sdhci-of-basicdrv.c
```sh
// SPDX-License-Identifier: GPL-2.0
/*
 * SDHCI-PLTFM based Controller driver
 *
 * Copyright (c) 2018  alamy.liu@gmail.com
 */

#include <linux/module.h>
#include <linux/mmc/host.h>
/*
#include <linux/of_device.h>
*/
#include "sdhci-pltfm.h"


static const struct sdhci_ops sdhci_basicdrv_ops = {
	.set_clock		= sdhci_set_clock,
	.set_bus_width		= sdhci_set_bus_width,
	.reset			= sdhci_reset,
	.set_uhs_signaling	= sdhci_set_uhs_signaling,

/*  ***WARNING: TBD (sdhci-pci-core.c)
	.enable_dma		= sdhci_pci_enable_dma, (synopsys)
	.hw_reset		= sdhci_pci_hw_reset,
	.platform_execute_tuning= amd_execute_tuning,
	.set_power		= sdhci_intel_set_power,
	.voltage_switch		= sdhci_intel_voltage_switch,
*/
};

static const struct sdhci_pltfm_data sdhci_pltfm_basicdrv_pdata = {
/*  (Synopsys does not use quirks in their PCI driver)
	.quirks = SDHCI_QUIRK_32BIT_DMA_ADDR |
		  SDHCI_QUIRK_32BIT_DMA_SIZE,

	.quirks		= SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER,

	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
			SDHCI_QUIRK2_CLOCK_DIV_ZERO_BROKEN,

	.quirks2	= SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
			SDHCI_QUIRK2_BROKEN_64_BIT_DMA |
			SDHCI_QUIRK2_BROKEN_DDR50,

	.quirks = ESDHC_DEFAULT_QUIRKS |
		  SDHCI_QUIRK_BROKEN_CARD_DETECTION |
		  SDHCI_QUIRK_NO_CARD_NO_RESET |
		  SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
*/
	.ops = &sdhci_basicdrv_ops,
};

static int sdhci_basicdrv_probe(struct platform_device *pdev)
{
	return sdhci_pltfm_register(pdev, &sdhci_basicdrv_pdata, 0);
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
```

