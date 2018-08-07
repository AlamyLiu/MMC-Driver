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

