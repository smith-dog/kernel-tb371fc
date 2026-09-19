/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * TB371FC ramoops forensic dumper KPM.
 * Loads, ioremaps the reserved ramoops region (0x27E00000, 2MB),
 * and serves it to userspace in chunks via KPM control (CTL0).
 *
 * Usage (on device, root):
 *   kpm ctl tb371fc-ramoops "<hex offset>"   -> writes chunk to out_msg
 * Or read via the APatch app KPM control UI.
 */

#include <compiler.h>
#include <kpmodule.h>
#include <linux/printk.h>
#include <common.h>

#define TAG "[TB371FC]"
#define logv(fmt, ...) pr_info(TAG fmt, ##__VA_ARGS__)

KPM_NAME("tb371fc-ramoops");
KPM_VERSION("1.0");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("tb371fc-project");
KPM_DESCRIPTION("Dump reserved ramoops region (0x27E00000/2MB) for forensics");

#define RAMOOPS_PHYS 0x27E00000ULL
#define RAMOOPS_SIZE 0x200000ULL

static void *mapped = 0;

static void *(*kf_ioremap_ncache)(uint64_t offset, uint64_t size) = 0;
static void (*kf_iounmap)(void *addr) = 0;
static void *(*kf_ioremap)(uint64_t offset, uint64_t size) = 0;

static long ramoops_init(const char *args, const char *event, void *__user reserved)
{
    kf_ioremap_ncache = (void *)kallsyms_lookup_name("ioremap_nocache");
    kf_ioremap = (void *)kallsyms_lookup_name("ioremap");
    kf_iounmap = (void *)kallsyms_lookup_name("iounmap");

    if (kf_ioremap_ncache)
        mapped = kf_ioremap_ncache(RAMOOPS_PHYS, RAMOOPS_SIZE);
    else if (kf_ioremap)
        mapped = kf_ioremap(RAMOOPS_PHYS, RAMOOPS_SIZE);

    logv("ramoops ioremap phys=%llx mapped=%px\n", RAMOOPS_PHYS, mapped);
    if (!mapped) {
        logv("ramoops ioremap FAILED — cannot dump\n");
        return -1;
    }

    /* sanity: dump first 64 bytes via kmsg so success is visible in dmesg */
    {
        const unsigned char *p = (const unsigned char *)mapped;
        logv("ramoops[0..15]: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
             p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
             p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
    }
    return 0;
}

/* args: hex offset in the region; writes min(outlen, remaining) bytes to out_msg */
static long ramoops_ctl0(const char *args, char *__user out_msg, int outlen)
{
    uint64_t off = 0;
    const char *p;
    uint64_t chunk;

    if (!mapped)
        return -1;

    for (p = args; *p; p++) {
        char c = *p;
        if (c >= '0' && c <= '9')
            off = off * 16 + (c - '0');
        else if (c >= 'a' && c <= 'f')
            off = off * 16 + (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            off = off * 16 + (c - 'A' + 10);
        else
            break;
    }

    if (off >= RAMOOPS_SIZE)
        return -1;

    chunk = (uint64_t)outlen;
    if (off + chunk > RAMOOPS_SIZE)
        chunk = RAMOOPS_SIZE - off;

    compat_copy_to_user(out_msg, (const char *)mapped + off, chunk);
    return (long)chunk;
}

static long ramoops_exit(void *__user reserved)
{
    if (mapped) {
        if (kf_iounmap)
            kf_iounmap(mapped);
        mapped = 0;
    }
    return 0;
}

KPM_INIT(ramoops_init);
KPM_CTL0(ramoops_ctl0);
KPM_EXIT(ramoops_exit);
