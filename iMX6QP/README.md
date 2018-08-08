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

## Handle Non-standard registers
uSDHC is still SDHC based with some customization. To handle those special registers (bit-fields), MMC driver provides `CONFIG_MMC_SDHCI_IO_ACCESSORS` to help developer on this kind of situation.

Developer overrides the basic six Read/Write functions (byte, word, long).
```sh
#ifdef CONFIG_MMC_SDHCI_IO_ACCESSORS
	u32		(*read_l)(struct sdhci_host *host, int reg);
	u16		(*read_w)(struct sdhci_host *host, int reg);
	u8		(*read_b)(struct sdhci_host *host, int reg);
	void	(*write_l)(struct sdhci_host *host, u32 val, int reg);
	void	(*write_w)(struct sdhci_host *host, u16 val, int reg);
	void	(*write_b)(struct sdhci_host *host, u8 val, int reg);
#endif
```
Use standard **sdhci_read** and **sdhci_write** functions in the code, then SDHCI layer would call the override functions.
```
#ifdef CONFIG_MMC_SDHCI_IO_ACCESSORS

static inline void sdhci_writel(struct sdhci_host *host, u32 val, int reg)
{
	if (unlikely(host->ops->write_l))
		host->ops->write_l(host, val, reg);
	else
		writel(val, host->ioaddr + reg);
}

static inline void sdhci_writew(...) {...}
...
#endif
```
Note: Use **read** and **write** functions in the overridden functions to prevent resursive situation.

In each function, matching the registers of interest, then modifying them.

For example:
The mismatched bit between eSDHC/uSDHC and STD SDHC
```/*
 * There is an INT DMA ERR mismatch between eSDHC and STD SDHC SPEC:
 * Bit25 is used in STD SPEC, and is reserved in fsl eSDHC design,
 * but bit28 is used as the INT DMA ERR in fsl eSDHC design.
 * Define this macro DMA error INT for fsl eSDHC
 */
#define ESDHC_INT_VENDOR_SPEC_DMA_ERR	(1 << 28)
```
The interested registers for this bit are `INT_STATUS(0x30)`, `INT_ENABLE(0x34)`, and `SIGNAL_ENABLE(0x38)`

Thus, override the functions
```
static const struct sdhci_ops sdhci_basicdrv_ops = {
#ifdef CONFIG_MMC_SDHCI_IO_ACCESSORS
	.read_l = esdhc_readl,
	.read_w = esdhc_readw,
	.read_b = esdhc_readb,
	.write_l = esdhc_writel,
	.write_w = esdhc_writew,
	.write_b = esdhc_writeb,
#endif
};
```
Convert those non-standard bits in its related R/W function(s)
```
#ifdef CONFIG_MMC_SDHCI_IO_ACCESSORS
static u32 esdhc_readl(struct sdhci_host *host, int reg)
{
	u32 val = readl(host->ioaddr + reg);

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
#endif
```

## Next

