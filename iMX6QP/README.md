# MMC driver on i.MX6QP (SD2 socket)
## Hardware
  - uSDHC (successor of eSDHC)
    - eSDHC: [Enhanced Secure Digital Host Controller](https://www.kernel.org/doc/Documentation/devicetree/bindings/mmc/fsl-imx-esdhc.txt)
    - uSDHC: Ultra Secured Digital Host Controller
    - Some bit fields are different from STD SDHC
  - Support up to SD specification 3.0
    - i.e.: No UHS-II

## Driver
- Study is based on Linux-Stable v4.17.3 (Jun-26, 2018)
- SDHCI-PLTFM based
- uSDHC specific
  - Register fields modification
  - GPIO CD (Card detection) & RO (Write-Protection)
  - Clocks
  - Pinmux (speed change on i.MX6QP board)
  - set_uhs_signaling
  - QUIRKS (non-standard, errata, ...etc)
  - ...

# Build the driver
Let's start with the very basic SDHCI-OF driver
[Initial Basic MMC Driver](SDHC_BasicDrv.md)
- Ported from sdhci-of-hwld.c (Looks like Nintendo MMC interface is fully SDHC compliant)
- sdhci_xxx standard functions in sdhci_ops.
- No QUIRKs
- sdhci_pltfm_xxx probe & remove

## Device tree and defconfig
### arch/arm/configs/imx_v6_v7_defconfig
Enable the variable to compile the Basic Driver. (match the variable used in Kconfig & Makefile)
```
 CONFIG_MMC_SDHCI=y
 CONFIG_MMC_SDHCI_PLTFM=y
 CONFIG_MMC_SDHCI_ESDHC_IMX=y
+CONFIG_MMC_SDHCI_OF_BASICDRV=y
 CONFIG_NEW_LEDS=y
 CONFIG_LEDS_CLASS=y
 CONFIG_LEDS_GPIO=y
```
Note: Also enable the following variables in the developing stage
- CONFIG_MMC_DEBUG
- CONFIG_DYNAMIC_DEBUG

### arch/arm/boot/dts/imx6qp-sabresd.dts
Provide the same `compatible` string in device tree for driver model to load the Basic Driver.
```
&usdhc2 {
       compatible = "virtualcom,basicdrv-sdhci";
};
```

## Customize PROBE function
By calling sdhci_pltfm_regisgter(), we lose the flexibility to do something for uSDHCI.
Also, lost some device-tree configuration features. i.e.: "cd-gpios" (in mmc_of_parse).

There are some i.MX6QP specific code need to be done in Probe.
Pulling in the sdhci_pltfm_register() code and lay out i.MX6QP tasks sequence.

```
static int sdhci_basicdrv_probe(struct platform_device *pdev)
{
	struct sdhci_host *host;
	int ret = 0;

	host = sdhci_pltfm_init(pdev, pdata, priv_size);
	if (IS_ERR(host))
		return PTR_ERR(host);

	/* i.MX6QP: Enable clock sources (IPG, AHB, PER) */

	/* i.MX6QP: Set PINCTRL / PINMUX */

	/* i.MX6QP: Update QUIRKS */

	/* i.MX6QP: ROM tuning bits fix up */

	sdhci_get_of_property(pdev);

	/* support of "cd-gpios" & "ro-gpios", speed, voltages, ...etc */
	ret = mmc_of_parse(host->mmc);
	if (ret) {
		dev_err(&pdev->dev, "parsing dt failed (%d)\n", ret);
		goto err;
	}

	/* i.MX6QP: H.W. Errata */

	ret = sdhci_add_host(host);
	if (ret)
		goto err;

	return 0;

err:
	sdhci_pltfm_free(pdev);

	return ret;
}
```

