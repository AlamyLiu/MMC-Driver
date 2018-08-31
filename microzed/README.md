# Xilinx MicroZed MMC driver (MicroSD socket)
## Hardware
  - J6 connector (MicroSD)
  - 3.3V

## Driver
- Study is based on Linux-Stable v4.17.18 (August, 2018)
- SDHCI-PLTFM based
- SDHCI complied (Easy to get it to work)
- compatible = `arasan,sdhci-8.9a`

# Build the driver
## defconfig for MicroZed (arch/arm/configs/xilinx_zynq_defconfig)
Kernel v4.17.18 does not have the DEFCONFIG for ZYNQ platforms. Got it from [linux-xlnx](https://github.com/Xilinx/linux-xlnx)

And I like to enable those debugging configurations
```
 CONFIG_USB_CONFIGFS_MASS_STORAGE=y
 CONFIG_USB_ZERO=m
 CONFIG_MMC=y
+CONFIG_MMC_TEST=y
+CONFIG_MMC_DEBUG=y
 CONFIG_MMC_SDHCI=y
 CONFIG_MMC_SDHCI_PLTFM=y
 CONFIG_MMC_SDHCI_OF_ARASAN=y

 CONFIG_NLS_CODEPAGE_437=y
 CONFIG_NLS_ASCII=y
 CONFIG_NLS_ISO8859_1=y
+CONFIG_DYNAMIC_DEBUG=y
+CONFIG_DEBUG_FS=y
 # CONFIG_SCHED_DEBUG is not set
 # CONFIG_DEBUG_PREEMPT is not set
 CONFIG_RCU_CPU_STALL_TIMEOUT=60
```

## Initial MMC driver
Based on the [i.MX6QP experience](../iMX6QP/README.md), starting with the
[SDHCI-PLTFM based Initial MMC Driver](SDHCI_PLTFM.base.md)
- Ported from sdhci-of-hwld.c (Looks like Nintendo MMC interface is fully SDHC compliant)
- sdhci_xxx standard functions in sdhci_ops.
- No QUIRKs
- sdhci_pltfm_xxx probe & remove (`sdhci_pltfm_register, sdhci_pltfm_unregister`)

## Device tree and defconfig
### arch/arm/configs/xilinx_zynq_defconfig
Enable the variable to compile the Initial MMC Study Driver. (match the variable used in Kconfig & Makefile)
```
 CONFIG_MMC_SDHCI=y
 CONFIG_MMC_SDHCI_PLTFM=y
 CONFIG_MMC_SDHCI_OF_ARASAN=y
+CONFIG_MMC_SDHCI_OF_STUDY=y
 CONFIG_NEW_LEDS=y
 CONFIG_LEDS_CLASS=y
 CONFIG_LEDS_GPIO=y
```

### arch/arm/boot/dts/zynq-microzed.dts
Provide the same `compatible` string in device tree for driver model to load the matched driver.
```
 &sdhci0 {
+        compatible = "freeknowledge,study-sdhci";

         status = "okay";
 };
```
Obviously, the Initial STUDY MMC driver would not work. Kernel booted up to a point and died.

But we could get some useful information by enabling some Dynamic Debugging messages.
For example:
```
dyndbg="func sdhci_setup_host +fp"
```

## SDHC/MMC Host Controller Capabilities
MicroZed (ZYNQ-7000 SoC) MMC Host Controller reports its capabilities as:
```
sdhci_setup_host: mmc0: sdhci: Version:   0x00008901 | Present:  0x01ff0000
sdhci_setup_host: mmc0: sdhci: Caps:      0x69ec0080 | Caps_1:   0x00000000
sdhci_setup_host: mmc0: sdhci: Auto-CMD23 unavailable
```
So we know that
- SD Specification Version: **Ver. 2.00**
- Supported
  - 64-bit System Addr for V4
  - Suspense/Resume
  - SDMA
  - High Speed (20 MHz ~ 50 MHz)
  - ADMA2
- Voltage ([MMC schematic](schematic.uSD.png))
  - 1.8v: NOT supported
  - 3.0v: NOT supported
  - 3.3v: supported
- Maximum Block length: 512 Bytes
- Base clock frequency: **Another method**
- Timeout clock frequency: **Antoher method**

## Clock Frequency
Because the **Host Controller Capabilities** doesn't provide **base** and **timeout** clock frequencies,
and there is no ```.get_max_clock``` and ```.get_timeout_clock``` defined in the **sdhci_ops** structure
in the Initial STUDY driver. Therefore, you should see the error message:
<div style="font-family:courier; color:red">Hardware doesn't specify base clock frequency.</div>
or
<div style="font-family:courier; color:red">Hardware doesn't specify timeout clock frequency.</div>

<br />
Two solutions here:<br />

### Porting the functions from arasan driver
It's the better way comparing to providing clock frequency values in device tree (hard-coded).
Add the function definitions in the sdhci_ops structure:
```
static const struct sdhci_ops sdhci_study_ops = {
	...
	.get_max_clock = sdhci_pltfm_clk_get_max_clock,
	.get_timeout_clock = sdhci_pltfm_clk_get_max_clock,
	...
};
```
Don't forget to
- set the value of <span style="font-family:courier; color:blue">pltfm_host->clk</span> in PROBE function.
- quirk.SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN

### Proivde the clock frequency values in capabilities
Hard-coded method is always not suggested. But for experience/study purpose, this example will do so.
The value is **25 MHz**.
<span style="font-family:courier">arch/arm/boot/dts/zynq-microzed.dts</span>
```
 &sdhci0 {
        compatible = "freeknowledge,study-sdhci";
+       sdhci-caps-mask = <0x00000000 0x0000FFFF>;
+       sdhci-caps      = <0x00000000 0x00001999>;
+
        status = "okay";
 };

```
## Enable clocks
There are two clocks defined in device tree
```
sdhci0: sdhci@e0100000 {
	clock-names = "clk_xin", "clk_ahb";
	clocks = <&clkc 21>, <&clkc 32>;
};
```
Clocks should be enabled in the PROBE function - sdhci_study_probe(), but it should be done after sdhci_pltfm_init().<br />
Instead of modifying the code inside sdhci_pltfm_register(), I like to bring it in to my PROBE function so that I don't touch the helper function that other driver might base on.
```
 static int sdhci_study_probe(struct platform_device *pdev)
 {
-       return sdhci_pltfm_register(pdev, &sdhci_study_pdata, 0);
+       struct sdhci_host *host;
+       int ret = 0;
+
+       host = sdhci_pltfm_init(pdev, &sdhci_study_pdata, 0);
+       if (IS_ERR(host))
+               return PTR_ERR(host);
+
+       sdhci_get_of_property(pdev);
+
+       ret = sdhci_add_host(host);
+       if (ret)
+               sdhci_pltfm_free(pdev);
+
+       return ret;
 }

```

Now enable the clocks
```
 static int sdhci_study_probe(struct platform_device *pdev)
 {
 	struct sdhci_host *host;
+	struct clk *clk_ahb, *clk_xin;
 	int ret = 0;
 
 	host = sdhci_pltfm_init(pdev, &sdhci_study_pdata, 0);
 	if (IS_ERR(host))
 		return PTR_ERR(host);
 
+	/* Enable clocks */
+	clk_ahb = devm_clk_get(&pdev->dev, "clk_ahb");
+	if (IS_ERR(clk_ahb)) {
+		dev_err(&pdev->dev, "clk_ahb clock not found.\n");
+		ret = PTR_ERR(clk_ahb);
+		goto err_pltfm_free;
+	}
+	clk_xin = devm_clk_get(&pdev->dev, "clk_xin");
+	if (IS_ERR(clk_xin)) {
+		dev_err(&pdev->dev, "clk_xin clock not found.\n");
+		ret = PTR_ERR(clk_xin);
+		goto err_pltfm_free;
+	}
+
+	ret = clk_prepare_enable(clk_ahb);
+	if (ret) {
+		dev_err(&pdev->dev, "Unable to enable AHB clock.\n");
+		goto err_pltfm_free;
+	}
+	ret = clk_prepare_enable(clk_xin);
+	if (ret) {
+		dev_err(&pdev->dev, "Unable to enable SD clock.\n");
+		goto clk_dis_ahb;
+	}
+
 	sdhci_get_of_property(pdev);
 
 	ret = sdhci_add_host(host);
 	if (ret)
-		sdhci_pltfm_free(pdev);
+		goto clk_disable_all;
+
+	return 0;
+
+clk_disable_all:
+	clk_disable_unprepare(clk_xin);
+clk_dis_ahb:
+	clk_disable_unprepare(clk_ahb);
+err_pltfm_free:
+	sdhci_pltfm_free(pdev);
 
 	return ret;
 }
```

Surprisingly, the MMC driver is UP and running.<br />
And Kernel could boot all the way to find the RootFS (one partition on uSD) to prompt login.<br />
The code looks like:
- [zynq-microzed.bootup.dts](zynq-microzed.bootup.dts)
- [sdhci-of-study.bootup.c](sdhci-of-study.bootup.c)

2018/08/30<br />

<hr />

# Misc
## QUIRKS
### quirks.SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN
It's used to trigger reading `sdhci_host.max_clk` from `sdhci_ops.get_max_clock`.<br />
Don't need this if providing the clock frequency values in device tree capabilities.

### quirks2.SDHCI_QUIRK2_PRESET_VALUE_BROKEN
Since MicroZed's SD Specification Version is 2.00 (SDHCI_SPEC_200), this setting could be omitted.
- `sdhci_set_ios`: No effect
  - host->version = 0x1 (SDHCI_SPEC_200) < SDHCI_SPEC_300
- `sdhci_runtime_resume_host`: No effect
  - `if (host->version < SDHCI_SPEC_300)  return;`

### quirks2.SDHCI_QUIRK2_CLOCK_DIV_ZERO_BROKEN
No effect on MicroZed: sdhci_calc_clk is inside the `host->version >= SDHCI_SPEC_300) scope

## SDHCI_OPS
### .reset
MicroZed does not have `SDHCI_ARASAN_QUIRK_FORCE_CDTEST` bit set. Simplify to `sdhci_reset`
### .set_bus_width
Hooks to standard `sdhci_set_bus_width`
### .set_uhs_signaling
Hooks to standard `sdhci_set_uhs_signaling`
### .set_power
- MicroZed's device-tree does not set **vmmc**, `sdhci_arasan_set_power` just calls `sdhci_set_power_noreg`
- `sdhci_set_power` does the same (when **vmmc** is not set)

## rockchip,rk3399
(To Do)
- base clock frequency
- clock multiplier

## Command Queue Engine (CQE)
The only example in kernel v4.17.18 is when
- compatible = `arasan,sdhci-5.1`

## External PHY
The only example in kernel v4.17.18 is when
- compatible = `arasan,sdhci-5.1`

[Command Queue Engine](../CQE/README.md)

