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

## sdhci_ops.get_ro
Since i.MX6QP uses GPIO pins for CD (Card-Detect) and RO (Read-Only/Write-Protect) detection. It would need to call `mmc_gpio_get_cd` and `mmc_gpio_get_ro` to get the status.

In SDHCI layer (sdhci.c), sdhci_get_cd does call mmc_gpio_get_cd to retrieve CD information. But RO information is from controller (**SDHCI_PRESENT_STATE[SDHCI_WRITE_PROTECT]**) by default except `sdhci_host.ops.get_ro` is defined.
```
static int sdhci_check_ro(struct sdhci_host *host)
{
	...
	else if (host->ops->get_ro)
		is_readonly = host->ops->get_ro(host);
	else
		is_readonly = !(sdhci_readl(host, SDHCI_PRESENT_STATE)
				& SDHCI_WRITE_PROTECT);
	...
}
```
Overriding the function is necessary to use GPIO for RO.
```
static unsigned int imx6q_basicdrv_get_ro(struct sdhci_host *host)
{
	pr_debug("%s\n", mmc_hostname(host->mmc));

	return mmc_gpio_get_ro(host->mmc);
}

static const struct sdhci_ops sdhci_basicdrv_ops = {
	.get_ro = imx6q_basicdrv_get_ro,
};
```

## PINCTRL & PINMUX
In imx driver (sdhci-esdhc-imx.c), it calls to change PINCTRL in `.set_uhs_signaling`.
Which is called by `sdhci_set_ios` when the SDHCI controller version >= SDHCI Specificaion 3.00 (TRUE in the case of i.MX6QP).

However, i.MX6QP is only switching between the two timing modes.
- MMC_TIMING_LEGACY
- MMC_TIMING_SD_HS

Also, in its device tree, it has only the `default` pinctrl setting.
Not like the imx6sx-sabreauto.dts which has all three modes (default, state_100mhz, state_200mhz)

Tracing the code in esdhc_set_uhs_signaling, the two modes are treated the same in the case of i.MX6QP.
- Meaning --- **No PINCTRL change at all**.

So the whole thing could be simplified:
- Set PINCTRL/PINMUX in PROBE
- Only do reset tuning in .set_uhs_signaling (maybe it could also be omitted in some way).

The code
```
static void imx6q_reset_tuning(struct sdhci_host *host)
{
	u32 ctrl;

	/* Reset the tuning circuit */
	ctrl = readl(host->ioaddr + ESDHC_MIX_CTRL);
	ctrl &= ~ESDHC_MIX_CTRL_SMPCLK_SEL;
	ctrl &= ~ESDHC_MIX_CTRL_FBCLK_SEL;
	writel(ctrl, host->ioaddr + ESDHC_MIX_CTRL);
	writel(0, host->ioaddr + ESDHC_TUNE_CTRL_STATUS);
}

static void imx6q_set_uhs_signaling(
	struct sdhci_host *host,
	unsigned timing)
{
	imx6q_reset_tuning(host);

/*
	imx6q_change_pinstate(host, timing);
*/
}

static const struct sdhci_ops sdhci_basicdrv_ops = {
	.set_uhs_signaling = imx6q_set_uhs_signaling,
};

static int sdhci_basicdrv_probe(struct platform_device *pdev)
{
	struct pinctrl *pins_default;

    ...

	/* i.MX6QP: Set PINCTRL / PINMUX */
	pins_default = devm_pinctrl_get_select_default(&pdev->dev);

    ...
}

```

## Clocks
i.MX6QP has its own way to handle clocks, it does not worth it to find a way to blend into standard SDHC clock framework. Just bringing those functions over.

1. Enable the IPG, AHB, PER clocks in PROBE.
2. Porting the sdhci_host.ops (sdhci_ops) clock functions
- .get_max_clock
- .get_min_clock
- .set_clock

Note: it queries PER clock value in PROBE fucntion and brings it over to those clock functions with `pltfm_host->clock`. The value on i.MX6QP is `198000000` (=~ 200 MHz: SDR104)


## Other i.MX6QP specific functions
It's pretty straight forward to bring other tasks/functions over. Just because it's not standard SDHC and need special care.

For example:
- eSDHC/uSDHC has special design.
- Caps (and Caps1) register(s) doesn't report correct value, driver needs to handles those corner cases. (It proves again that Hardware == Software, when hardware does not do its job well, Software needs to clean up for it, with much more effort. Memory, Speed, ...etc)

#### PROBE
- QUIRKS
- Clear tuning bits
- HWInit (Errata alike)
```
static int sdhci_basicdrv_probe(struct platform_device *pdev)
{
	...
	/* Update QUIRKS */
	host->quirks2 |= SDHCI_QUIRK2_PRESET_VALUE_BROKEN;
	host->mmc->caps |= MMC_CAP_1_8V_DDR;
	host->quirks2 |= SDHCI_QUIRK2_BROKEN_HS200;

	/* clear tuning bits in case ROM has set it already */
	writel(0x0, host->ioaddr + ESDHC_MIX_CTRL);
	writel(0x0, host->ioaddr + SDHCI_ACMD12_ERR);
	writel(0x0, host->ioaddr + ESDHC_TUNE_CTRL_STATUS);
	...

	/* Errata */
	imx6q_basicdrv_hwinit(host);
	...
}
```
#### SDHCI_OPS (sdhci_host.ops)
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
};

static const struct sdhci_pltfm_data sdhci_basicdrv_pdata = {
	/* i.MX6QP quirks */
	.quirks = ESDHC_DEFAULT_QUIRKS | SDHCI_QUIRK_NO_HISPD_BIT
			| SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC
			| SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC
			| SDHCI_QUIRK_BROKEN_CARD_DETECTION,

	.ops = &sdhci_basicdrv_ops,
};
```

## Misc
#### Differentiate SoC
In PROBE function, **of_match_device** found the matched `compatible` string in imx_esdhc_dt_ids table. Its **data** field points to one of the matched SoC structure. Which is saved in **imx_data->socdata**.
Using one of these **is_xxx_usdhc()** functions to determine which SoC the driver is running on.

The **imx_data->socdata->flags** will be (ESDHC_FLAG_USDHC | ESDHC_FLAG_MAN_TUNING) in the case of i.MX6QP platform. And the value is **TRUE** for is_imx6q_usdhc() and esdhc_is_usdhc().
```
static struct esdhc_soc_data usdhc_imx6q_data = {
	.flags = ESDHC_FLAG_USDHC | ESDHC_FLAG_MAN_TUNING,
};

static const struct of_device_id imx_esdhc_dt_ids[] = {
	{ .compatible = "fsl,imx6q-usdhc", .data = &usdhc_imx6q_data, },
	{ /* sentinel */ }
};

static inline int is_imx6q_usdhc(struct pltfm_imx_data *data)
{
	return data->socdata == &usdhc_imx6q_data;
}
static inline int esdhc_is_usdhc(struct pltfm_imx_data *data)
{
	return !!(data->socdata->flags & ESDHC_FLAG_USDHC);
}
```

## Next

