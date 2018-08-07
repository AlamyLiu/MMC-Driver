# MMC data structure
The `host` variable was confusing me when I read the source code.
There are mainly two hosts. **mmc_host** and **sdhci_host**.
- mmc_host is the core data structure of MMC driver
- sdhci_host is built on top of mmc_host
- sdhci_pltfm_host is built on top of sdhci_host
- Both hosts have their own OPS
  - mmc_host.ops = **mmc_ops**
  - sdhci_host.ops = **sdhci_ops**
- Both hosts have their own Private data
  - mmc_host.private = **sdhci_host**
  - sdhci_host.private = **sdhci_pltfm_host**
  - sdhci_pltfm_host.private = **developer data**


```
  mmc_card
  +--------------------------------------------------------+
  | .host (mmc_host)                                       |
  | +----------------------------------------------------+ |
  | | .ops (mmc_host_ops)                                | |
  | | .private (sdhci_host)                              | |
  | | +------------------------------------------------+ | |
  | | | .ops (sdhci_host_ops)                          | | |
  | | | .private (sdhci_pltfm_host)                    | | |
  | | | +--------------------------------------------+ | | |
  | | | | .private                                   | | | |
  | | | +--------------------------------------------+ | | |
  | | +------------------------------------------------+ | |
  | +----------------------------------------------------+ |
  +--------------------------------------------------------+
```
### Host layers (visualization)
```
  +--------------------+
  |  sdhci_pltfm_host  |
  +--------------------+
  |        sdhci_host  |
  +--------------------+
  |          mmc_host  |
  +--------------------+
```

