#ifndef RNDU4000_CONTEXT_HEADER
#define RNDU4000_CONTEXT_HEADER

#include <linux/kconfig.h>
#include <linux/compiler_types.h>


#include <linux/kernel.h>
#include <linux/device.h>
#include "linux/pci.h"

struct Context
{
    u32 mem_base;

    struct pci_dev* lpc;
    struct device*  device;
    spinlock_t lock;

    // Page 534 of doc/ich9
    u32 lpc_gpio_use_sel;
    u32 lpc_gpio_use_sel2;
    u32 lpc_gpo_blink;
    u32 lpc_gpi_inv;
    u32 lpc_gp_io_sel;
    u32 lpc_gp_io_sel2;
    u32 lpc_gp_lvl;
    u32 lpc_gp_lvl2;
};


#endif
