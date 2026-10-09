// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2013-2014, 2016-2018, The Linux Foundation. All rights
 */

#include <linux/kernel.h>
#include <linux/hw_random.h>
#include <linux/random.h>
#include <linux/io.h>

#include <soc/qcom/scm.h>

#include <asm/cacheflush.h>

#define TZ_SVC_CRYPTO	10
#define PRNG_CMD_ID	0x01

struct tz_prng_data {
	uint8_t		*out_buf;
	uint32_t	out_buf_sz;
} __packed;

#define RANDOM_BUFFER_SIZE	PAGE_SIZE
char random_buffer[RANDOM_BUFFER_SIZE] __aligned(PAGE_SIZE);

void __init init_random_pool(void)
{
	struct tz_prng_data data;
	int ret;
	struct scm_desc desc;

	data.out_buf = (uint8_t *) virt_to_phys(random_buffer);
	desc.args[0] = (unsigned long) data.out_buf;
	desc.args[1] = data.out_buf_sz = SZ_512;
	desc.arginfo = SCM_ARGS(2, SCM_RW, SCM_VAL);

	/* Clean the buffer to DRAM so the TZ PRNG DMA sees/maintains it. */
	dmac_flush_range(random_buffer, random_buffer + RANDOM_BUFFER_SIZE);

	ret = scm_call2(SCM_SIP_FNID(TZ_SVC_CRYPTO, PRNG_CMD_ID), &desc);

	if (!ret) {
		u64 bytes_received = desc.ret[0];

		if (bytes_received != SZ_512)
			pr_warn("Did not receive the expected number of bytes from PRNG: %llu\n",
				bytes_received);

		bytes_received = (bytes_received <= RANDOM_BUFFER_SIZE) ?
					bytes_received : RANDOM_BUFFER_SIZE;
		/*
		 * Invalidate the CPU's stale BSS-zero cache lines so the
		 * bytes the TZ PRNG wrote via DMA are what we read back.
		 * The .325-era freeze bracketed this together with
		 * add_hwgenerator_randomness and the then-lost RNG init;
		 * re-proven benign under the fixed environment.
		 */
		dmac_inv_range(random_buffer, random_buffer + bytes_received);
		/*
		 * add_device_randomness, NOT add_hwgenerator_randomness: the
		 * .325 add_hwgenerator_randomness gained a throttle that
		 * sleeps 10s when crng is not ready - fatal in setup_arch
		 * context. This early seed is an optimization, never credited.
		 */
		add_device_randomness(random_buffer, bytes_received);
		pr_info("init_random_pool: injected %llu bytes from TZ PRNG\n",
			bytes_received);
	}
}

