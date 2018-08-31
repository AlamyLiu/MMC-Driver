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
	struct sdhci_host *host;
	struct clk *clk_ahb, *clk_xin;
	int ret = 0;

	host = sdhci_pltfm_init(pdev, &sdhci_study_pdata, 0);
	if (IS_ERR(host))
		return PTR_ERR(host);

	/* Enable clocks */
	clk_ahb = devm_clk_get(&pdev->dev, "clk_ahb");
	if (IS_ERR(clk_ahb)) {
		dev_err(&pdev->dev, "clk_ahb clock not found.\n");
		ret = PTR_ERR(clk_ahb);
		goto err_pltfm_free;
	}
	clk_xin = devm_clk_get(&pdev->dev, "clk_xin");
	if (IS_ERR(clk_xin)) {
		dev_err(&pdev->dev, "clk_xin clock not found.\n");
		ret = PTR_ERR(clk_xin);
		goto err_pltfm_free;
	}

	ret = clk_prepare_enable(clk_ahb);
	if (ret) {
		dev_err(&pdev->dev, "Unable to enable AHB clock.\n");
		goto err_pltfm_free;
	}
	ret = clk_prepare_enable(clk_xin);
	if (ret) {
		dev_err(&pdev->dev, "Unable to enable SD clock.\n");
		goto clk_dis_ahb;
	}

	sdhci_get_of_property(pdev);

	ret = sdhci_add_host(host);
	if (ret)
		goto clk_disable_all;

	return 0;

clk_disable_all:
	clk_disable_unprepare(clk_xin);
clk_dis_ahb:
	clk_disable_unprepare(clk_ahb);
err_pltfm_free:
	sdhci_pltfm_free(pdev);

	return ret;
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
