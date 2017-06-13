#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x5c7202ab, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xc26d6104, __VMLINUX_SYMBOL_STR(param_ops_charp) },
	{ 0xf00494f9, __VMLINUX_SYMBOL_STR(pci_unregister_driver) },
	{ 0x7cbbc393, __VMLINUX_SYMBOL_STR(__pci_register_driver) },
	{ 0xe2d5255a, __VMLINUX_SYMBOL_STR(strcmp) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x16305289, __VMLINUX_SYMBOL_STR(warn_slowpath_null) },
	{ 0x8e039cec, __VMLINUX_SYMBOL_STR(__dynamic_dev_dbg) },
	{ 0x3ef4159, __VMLINUX_SYMBOL_STR(dev_notice) },
	{ 0xa1b546e8, __VMLINUX_SYMBOL_STR(pci_intx_mask_supported) },
	{ 0x41a2bb3e, __VMLINUX_SYMBOL_STR(dma_ops) },
	{ 0x78764f4e, __VMLINUX_SYMBOL_STR(pv_irq_ops) },
	{ 0xcdbfc83a, __VMLINUX_SYMBOL_STR(arch_dma_alloc_attrs) },
	{ 0xc259b8f2, __VMLINUX_SYMBOL_STR(_dev_info) },
	{ 0x90080b8a, __VMLINUX_SYMBOL_STR(__uio_register_device) },
	{ 0xe6298301, __VMLINUX_SYMBOL_STR(sysfs_create_group) },
	{ 0xfcf0ebd8, __VMLINUX_SYMBOL_STR(pci_enable_msix) },
	{ 0xa11b55b2, __VMLINUX_SYMBOL_STR(xen_start_info) },
	{ 0x731dba7a, __VMLINUX_SYMBOL_STR(xen_domain_type) },
	{ 0x2e8106c8, __VMLINUX_SYMBOL_STR(dma_supported) },
	{ 0x42c8de35, __VMLINUX_SYMBOL_STR(ioremap_nocache) },
	{ 0xc2fd990e, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0xd518c28, __VMLINUX_SYMBOL_STR(pci_set_master) },
	{ 0x596ffe3d, __VMLINUX_SYMBOL_STR(pci_enable_device) },
	{ 0xe6f51caa, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xdfeb1fad, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0xce4f8fa1, __VMLINUX_SYMBOL_STR(pci_check_and_mask_intx) },
	{ 0xfa22056b, __VMLINUX_SYMBOL_STR(pci_intx) },
	{ 0x3bbd261c, __VMLINUX_SYMBOL_STR(pci_cfg_access_unlock) },
	{ 0x1a4e0a0a, __VMLINUX_SYMBOL_STR(pci_cfg_access_lock) },
	{ 0x17252a87, __VMLINUX_SYMBOL_STR(remap_pfn_range) },
	{ 0x5944d015, __VMLINUX_SYMBOL_STR(__cachemode2pte_tbl) },
	{ 0xa50a80c2, __VMLINUX_SYMBOL_STR(boot_cpu_data) },
	{ 0x565a5d9f, __VMLINUX_SYMBOL_STR(pci_disable_msix) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0xa1fce070, __VMLINUX_SYMBOL_STR(pci_disable_device) },
	{ 0xedc03953, __VMLINUX_SYMBOL_STR(iounmap) },
	{ 0x7d538d70, __VMLINUX_SYMBOL_STR(uio_unregister_device) },
	{ 0x518b6e69, __VMLINUX_SYMBOL_STR(sysfs_remove_group) },
	{ 0x28318305, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0x7dd39cc1, __VMLINUX_SYMBOL_STR(pci_bus_type) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0xb89489f2, __VMLINUX_SYMBOL_STR(pci_enable_sriov) },
	{ 0xa03e1946, __VMLINUX_SYMBOL_STR(pci_disable_sriov) },
	{ 0xeb2536e6, __VMLINUX_SYMBOL_STR(pci_num_vf) },
	{ 0x3c80c06c, __VMLINUX_SYMBOL_STR(kstrtoull) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=uio";


MODULE_INFO(srcversion, "5CAC66020820FB2AE369C4B");
