
# Hardware doesn't specify base clock frequency

## Problem
```Hardware doesn't specify base clock frequency```

## Condition
Max clock is 0 (i.e.: not specified) OR set Clock Base Broken QUIRK.<br>
And no .get_max_clock

## The experience here is:
```
sdhci_setup_host: mmc1: sdhci: Version:   0x00000000 | Present:  0x00000000
sdhci_setup_host: mmc1: sdhci: Caps:      0x00000000 | Caps_1:   0x00000001
```

Says my max clock should be 200 MHz (for SDR104).<br>
The Host Controller version is 5 (>= `SDHCI_SPEC_300` == 0x02)<br>
I should have 200 == 0xc8 at host->caps[15:8] (mask = 0x0000FF00 = `SDHCI_CLOCK_V3_BASE_MASK`)

## Hacking
* sdhci_setup_host: modify the code to call **__sdhci_read_caps**, instead of sdhci_read_caps, so that one could provide Version, Cap, and Cap1 values.
* Override capability register values in Device Tree
* Use sdhci_host.ops.get_max_clock function

As I'm a lazy engineer, I just provide the 64-bit CAPs value in device tree<br>
(flexibility, code v.s. data, ...)
```
sdhci-caps-mask = <0x00000000 0x0000FF00>;
sdhci-caps      = <0x00000000 0x0000C800>;
```

## Result
Add some code to inspect
```
diff --git a/drivers/mmc/host/sdhci.c b/drivers/mmc/host/sdhci.c
index 2020e57ffa..900a633de2 100644
--- a/drivers/mmc/host/sdhci.c
+++ b/drivers/mmc/host/sdhci.c
@@ -3286,6 +3286,9 @@ void __sdhci_read_caps(struct sdhci_host *host, u16 *ver, u32 *caps, u32 *caps1)
        of_property_read_u64(mmc_dev(host->mmc)->of_node,
                             "sdhci-caps", &dt_caps);
 
+       DBG("sdhci-caps-mask:   0x%016llx\n", dt_caps_mask);
+       DBG("sdhci-caps     :   0x%016llx\n", dt_caps);
+
        v = ver ? *ver : sdhci_readw(host, SDHCI_HOST_VERSION);
        host->version = (v & SDHCI_SPEC_VER_MASK) >> SDHCI_SPEC_VER_SHIFT;
```

Compile the DBG() code
```
CONFIG_PRINTK=y
CONFIG_DYNAMIC_DEBUG=y
CONFIG_MMC_DEBUG=y
```

Enable the message<br>
```[    0.000000] Kernel command line: console=ttySAC3,115200n8 root=/dev/mmcblk0p3 rw rootfstype=ext4 loglevel=7 dyndbg="func __sdhci_read_caps +fp; func sdhci_setup_host +fp"```

See what we got
```
[root@localhost ~]# dmesg | grep sdhci-caps
[    9.651004] __sdhci_read_caps: mmc1: sdhci: sdhci-caps-mask:   0x000000000000ff00
[    9.651013] __sdhci_read_caps: mmc1: sdhci: sdhci-caps     :   0x000000000000c800
[root@localhost ~]# dmesg | grep "clock frequency"
[    9.660886] mmc1: Hardware doesn't specify timeout clock frequency.
```

Good, _base clock frequency_ problem was gone!<br>
Next: _timeout clock frequency_
