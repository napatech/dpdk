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
	{ 0x18c70492, __VMLINUX_SYMBOL_STR(up_read) },
	{ 0xbd100793, __VMLINUX_SYMBOL_STR(cpu_online_mask) },
	{ 0x79aa04a2, __VMLINUX_SYMBOL_STR(get_random_bytes) },
	{ 0x5aeb83a1, __VMLINUX_SYMBOL_STR(netif_carrier_on) },
	{ 0x50c77500, __VMLINUX_SYMBOL_STR(netif_carrier_off) },
	{ 0x44b1d426, __VMLINUX_SYMBOL_STR(__dynamic_pr_debug) },
	{ 0xb1c193fe, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x8fb1f0fd, __VMLINUX_SYMBOL_STR(__put_net) },
	{ 0xe4d3cb72, __VMLINUX_SYMBOL_STR(kthread_create_on_node) },
	{ 0x7d11c268, __VMLINUX_SYMBOL_STR(jiffies) },
	{ 0x2747380, __VMLINUX_SYMBOL_STR(down_read) },
	{ 0xe2d5255a, __VMLINUX_SYMBOL_STR(strcmp) },
	{ 0xdcc32a4a, __VMLINUX_SYMBOL_STR(kthread_bind) },
	{ 0x270c2a22, __VMLINUX_SYMBOL_STR(__netdev_alloc_skb) },
	{ 0x9e88526, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0xc26d6104, __VMLINUX_SYMBOL_STR(param_ops_charp) },
	{ 0xe9e01880, __VMLINUX_SYMBOL_STR(misc_register) },
	{ 0xfb578fc5, __VMLINUX_SYMBOL_STR(memset) },
	{ 0xb6137ab5, __VMLINUX_SYMBOL_STR(netif_rx_ni) },
	{ 0x7380a66b, __VMLINUX_SYMBOL_STR(unregister_pernet_subsys) },
	{ 0xdfca912e, __VMLINUX_SYMBOL_STR(netif_tx_wake_queue) },
	{ 0x6c1efb72, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0xb81a2997, __VMLINUX_SYMBOL_STR(__mutex_init) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xc6067d0a, __VMLINUX_SYMBOL_STR(kthread_stop) },
	{ 0x518f9733, __VMLINUX_SYMBOL_STR(free_netdev) },
	{ 0xa1c76e0a, __VMLINUX_SYMBOL_STR(_cond_resched) },
	{ 0x9166fada, __VMLINUX_SYMBOL_STR(strncpy) },
	{ 0x1297aaa3, __VMLINUX_SYMBOL_STR(register_netdev) },
	{ 0x5a921311, __VMLINUX_SYMBOL_STR(strncmp) },
	{ 0x2447e78e, __VMLINUX_SYMBOL_STR(skb_push) },
	{ 0xeecdb3a5, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0xbe5c39d2, __VMLINUX_SYMBOL_STR(up_write) },
	{ 0x4dfffbed, __VMLINUX_SYMBOL_STR(down_write) },
	{ 0xa916b694, __VMLINUX_SYMBOL_STR(strnlen) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0xd62c833f, __VMLINUX_SYMBOL_STR(schedule_timeout) },
	{ 0x966ad9f, __VMLINUX_SYMBOL_STR(alloc_netdev_mqs) },
	{ 0xfee0cbb4, __VMLINUX_SYMBOL_STR(eth_type_trans) },
	{ 0x7f24de73, __VMLINUX_SYMBOL_STR(jiffies_to_usecs) },
	{ 0xa1b09572, __VMLINUX_SYMBOL_STR(wake_up_process) },
	{ 0x69580a4d, __VMLINUX_SYMBOL_STR(register_pernet_subsys) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0xf331f149, __VMLINUX_SYMBOL_STR(ether_setup) },
	{ 0xa6bbd805, __VMLINUX_SYMBOL_STR(__wake_up) },
	{ 0xb3f7646e, __VMLINUX_SYMBOL_STR(kthread_should_stop) },
	{ 0x2207a57f, __VMLINUX_SYMBOL_STR(prepare_to_wait_event) },
	{ 0x9c55cec, __VMLINUX_SYMBOL_STR(schedule_timeout_interruptible) },
	{ 0x69acdf38, __VMLINUX_SYMBOL_STR(memcpy) },
	{ 0x6d3c321b, __VMLINUX_SYMBOL_STR(dev_trans_start) },
	{ 0xf08242c2, __VMLINUX_SYMBOL_STR(finish_wait) },
	{ 0xeb68aa96, __VMLINUX_SYMBOL_STR(unregister_netdev) },
	{ 0x94045477, __VMLINUX_SYMBOL_STR(consume_skb) },
	{ 0x464e7afb, __VMLINUX_SYMBOL_STR(skb_put) },
	{ 0x4f6b400b, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0xb1d224dc, __VMLINUX_SYMBOL_STR(misc_deregister) },
	{ 0x9fc35244, __VMLINUX_SYMBOL_STR(__init_rwsem) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "C1BCE1852D37B5F833BB878");
