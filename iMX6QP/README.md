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

