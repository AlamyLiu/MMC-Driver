# Command Queue Engine (CQE)
There is not much CQE code examples in kernel, the sdhci-of-arasan.c and sdhci-pci-core.c are basically the same.

- Create own add_host (i.e.: basicdrv_add_host) instead of calling sdhci_add_host() which doesn't initiate CQHCI (cqhci_init)
- Why NOT add **cqhci_init** into **sdhci_add_host** and enabled by environment variable (CONFIG_SDHCI_CQE) ? Not all the SD/eMMC interfaces on the same paltform have Command Queue Engine (CQE)

```
int sdhci_add_host(struct sdhci_host *host)
{
	int ret;

	ret = sdhci_setup_host(host);
	if (ret)
		return ret;

+#ifdef (CONFIG_SDHCI_CQE)
+	/* allocate _cq_host_ and basic initialization */
+	ret = cqhci_init(cq_host, host->mmc, dma64);
+	if (ret)
+		goto cleanup;
+#endif

	ret = __sdhci_add_host(host);
	if (ret)
		goto cleanup;

	return 0;

cleanup:
	sdhci_cleanup_host(host);

	return ret;
}
EXPORT_SYMBOL_GPL(sdhci_add_host);
```

## Interrupt
Use **sdhci_ops.irq** to hook the irq handler, which handles SDHC & CQHC interrupts if **sdhci_host.cqe_on** is set. Meaning: CQE is enabled (see `sdhci_cqe_enable`)
```
static u32 sdhci_sampledrv_cqhci_irq(struct sdhci_host *host, u32 intmask)
{
	int cmd_error = 0;
	int data_error = 0;

	if (!sdhci_cqe_irq(host, intmask, &cmd_error, &data_error))
		return intmask;

	cqhci_irq(host->mmc, intmask, cmd_error, data_error);

	return 0;
}

struct sdhci_ops sdhci_sampledrv_ops = {
	.irq = sdhci_sampledrv_cqhci_irq,
};
```

