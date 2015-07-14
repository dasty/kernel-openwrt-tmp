/*
 * ath79-i2s-pll.c -- ALSA DAI PLL management for QCA AR71xx/AR9xxx designs
 *
 * Copyright (c) 2012 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/module.h>
#include <linux/clk.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <asm/mach-ath79/ar71xx_regs.h>
#include <asm/mach-ath79/ath79.h>

#include "ath79-i2s.h"
#include "ath79-i2s-pll.h"

static DEFINE_SPINLOCK(ath79_pll_lock);

#define PLL_REFDIV		1
#define PLL_EXTDIV		2
#define PLL_POSTPLDIV	3

static void ath79_pll_set_target_div(u32 div_int, u32 div_frac)
{
	u32 t;

	spin_lock(&ath79_pll_lock);

	t = ath79_pll_rr(AR934X_PLL_AUDIO_MOD_REG);
	t &= ~(AR934X_PLL_AUDIO_MOD_TGT_DIV_INT_MASK
		<< AR934X_PLL_AUDIO_MOD_TGT_DIV_INT_SHIFT);
	t &= ~(AR934X_PLL_AUDIO_MOD_TGT_DIV_FRAC_MASK
		<< AR934X_PLL_AUDIO_MOD_TGT_DIV_FRAC_SHIFT);
	t |= (div_int & AR934X_PLL_AUDIO_MOD_TGT_DIV_INT_MASK)
		<< AR934X_PLL_AUDIO_MOD_TGT_DIV_INT_SHIFT;
	t |= (div_frac & AR934X_PLL_AUDIO_MOD_TGT_DIV_FRAC_MASK)
		<< AR934X_PLL_AUDIO_MOD_TGT_DIV_FRAC_SHIFT;
	ath79_pll_wr(AR934X_PLL_AUDIO_MOD_REG, t);

	spin_unlock(&ath79_pll_lock);
}

static void ath79_pll_set_refdiv(u32 refdiv)
{
	u32 t;

	spin_lock(&ath79_pll_lock);

	t = ath79_pll_rr(AR934X_PLL_AUDIO_CONFIG_REG);
	t &= ~(AR934X_PLL_AUDIO_CONFIG_REFDIV_MASK
		<< AR934X_PLL_AUDIO_CONFIG_REFDIV_SHIFT);
	t |= (refdiv & AR934X_PLL_AUDIO_CONFIG_REFDIV_MASK)
		<< AR934X_PLL_AUDIO_CONFIG_REFDIV_SHIFT;
	ath79_pll_wr(AR934X_PLL_AUDIO_CONFIG_REG, t);

	spin_unlock(&ath79_pll_lock);
}

static void ath79_pll_set_ext_div(u32 ext_div)
{
	u32 t;

	spin_lock(&ath79_pll_lock);

	t = ath79_pll_rr(AR934X_PLL_AUDIO_CONFIG_REG);
	t &= ~(AR934X_PLL_AUDIO_CONFIG_EXT_DIV_MASK
		<< AR934X_PLL_AUDIO_CONFIG_EXT_DIV_SHIFT);
	t |= (ext_div & AR934X_PLL_AUDIO_CONFIG_EXT_DIV_MASK)
		<< AR934X_PLL_AUDIO_CONFIG_EXT_DIV_SHIFT;
	ath79_pll_wr(AR934X_PLL_AUDIO_CONFIG_REG, t);

	spin_unlock(&ath79_pll_lock);
}

static void ath79_pll_set_postpllpwd(u32 postpllpwd)
{
	u32 t;

	spin_lock(&ath79_pll_lock);

	t = ath79_pll_rr(AR934X_PLL_AUDIO_CONFIG_REG);
	t &= ~(AR934X_PLL_AUDIO_CONFIG_POSTPLLPWD_MASK
		<< AR934X_PLL_AUDIO_CONFIG_POSTPLLPWD_SHIFT);
	t |= (postpllpwd & AR934X_PLL_AUDIO_CONFIG_POSTPLLPWD_MASK)
		<< AR934X_PLL_AUDIO_CONFIG_POSTPLLPWD_SHIFT;
	ath79_pll_wr(AR934X_PLL_AUDIO_CONFIG_REG, t);

	spin_unlock(&ath79_pll_lock);
}

static void ath79_pll_bypass(bool val)
{
	u32 t;

	spin_lock(&ath79_pll_lock);

	t = ath79_pll_rr(AR934X_PLL_AUDIO_CONFIG_REG);
	if(val)
		t |= AR934X_PLL_AUDIO_CONFIG_BYPASS;
	else
		t &= ~(AR934X_PLL_AUDIO_CONFIG_BYPASS);
	ath79_pll_wr(AR934X_PLL_AUDIO_CONFIG_REG, t);

	spin_unlock(&ath79_pll_lock);
}

static bool ath79_pll_ispowered(void)
{
	u32 status;

	status = ath79_pll_rr(AR934X_PLL_AUDIO_CONFIG_REG)
			& AR934X_PLL_AUDIO_CONFIG_PLLPWD;
	return ( !status ? true : false);
}

static void ath79_audiodpll_set_gains(u32 kd, u32 ki)
{
	u32 t;

	spin_lock(&ath79_pll_lock);

	if(ath79_pll_ispowered())
		BUG();

	t = ath79_audio_dpll_rr(AR934X_DPLL_REG_2);
	t &= ~(AR934X_DPLL_2_KD_MASK << AR934X_DPLL_2_KD_SHIFT);
	t &= ~(AR934X_DPLL_2_KI_MASK << AR934X_DPLL_2_KI_SHIFT);
	t |= (kd & AR934X_DPLL_2_KD_MASK) << AR934X_DPLL_2_KD_SHIFT;
	t |= (ki & AR934X_DPLL_2_KI_MASK) << AR934X_DPLL_2_KI_SHIFT;
	ath79_audio_dpll_wr(AR934X_DPLL_REG_2, t);

	spin_unlock(&ath79_pll_lock);
}

static void ath79_audiodpll_phase_shift_set(u32 phase)
{
	u32 t;

	spin_lock(&ath79_pll_lock);

	if(ath79_pll_ispowered())
		BUG();

	t = ath79_audio_dpll_rr(AR934X_DPLL_REG_3);
	t &= ~(AR934X_DPLL_3_PHASESH_MASK << AR934X_DPLL_3_PHASESH_SHIFT);
	t |= (phase & AR934X_DPLL_3_PHASESH_MASK)
		<< AR934X_DPLL_3_PHASESH_SHIFT;
	ath79_audio_dpll_wr(AR934X_DPLL_REG_3, t);

	spin_unlock(&ath79_pll_lock);
}

static void ath79_audiodpll_range_set(void)
{
	u32 t;

	spin_lock(&ath79_pll_lock);

	t = ath79_audio_dpll_rr(AR934X_DPLL_REG_2);
	t &= ~(AR934X_DPLL_2_RANGE);
	ath79_audio_dpll_wr(AR934X_DPLL_REG_2, t);
	t = ath79_audio_dpll_rr(AR934X_DPLL_REG_2);
	t |= AR934X_DPLL_2_RANGE;
	ath79_audio_dpll_wr(AR934X_DPLL_REG_2, t);

	spin_unlock(&ath79_pll_lock);
}

static u32 ath79_audiodpll_sqsum_dvc_get(void)
{
	u32 t;

	t = ath79_audio_dpll_rr(AR934X_DPLL_REG_3) >> AR934X_DPLL_3_SQSUM_DVC_SHIFT;
	t &= AR934X_DPLL_3_SQSUM_DVC_MASK;
	return t;
}

static void ath79_pll_powerup(void)
{
	u32 t;

	spin_lock(&ath79_pll_lock);

	t = ath79_pll_rr(AR934X_PLL_AUDIO_CONFIG_REG);
	t &= ~AR934X_PLL_AUDIO_CONFIG_PLLPWD;
	ath79_pll_wr(AR934X_PLL_AUDIO_CONFIG_REG, t);

	spin_unlock(&ath79_pll_lock);
}

static void ath79_pll_powerdown(void)
{
	u32 t;

	spin_lock(&ath79_pll_lock);

	t = ath79_pll_rr(AR934X_PLL_AUDIO_CONFIG_REG);
	t |= AR934X_PLL_AUDIO_CONFIG_PLLPWD;
	ath79_pll_wr(AR934X_PLL_AUDIO_CONFIG_REG, t);

	spin_unlock(&ath79_pll_lock);
}

static void ath79_audiodpll_do_meas_set(void)
{
	u32 t;

	spin_lock(&ath79_pll_lock);

	t = ath79_audio_dpll_rr(AR934X_DPLL_REG_3);
	t |= AR934X_DPLL_3_DO_MEAS;
	ath79_audio_dpll_wr(AR934X_DPLL_REG_3, t);

	spin_unlock(&ath79_pll_lock);
}

static void ath79_audiodpll_do_meas_clear(void)
{
	u32 t;

	spin_lock(&ath79_pll_lock);

	t = ath79_audio_dpll_rr(AR934X_DPLL_REG_3);
	t &= ~(AR934X_DPLL_3_DO_MEAS);
	ath79_audio_dpll_wr(AR934X_DPLL_REG_3, t);

	spin_unlock(&ath79_pll_lock);
}

static bool ath79_audiodpll_meas_done_is_set(void)
{
	u32 status;

	status = ath79_audio_dpll_rr(AR934X_DPLL_REG_4) & AR934X_DPLL_4_MEAS_DONE;
	return ( status ? true : false);
}

int ath79_audio_set_freq(int freq)
{
	struct clk *clk;
	const struct ath79_pll_config *cfg;
	
	u32 clk_rate;
	u32 base_rate;
	u32 div_int, div_frac;

	clk = clk_get(NULL, "ref");

	clk_rate = clk_get_rate(clk);

	if (clk_rate != 25000000 && clk_rate != 40000000) {
		printk("Wrong clk_rate of %u\n", clk_rate);
		return -EINVAL;
	}

	base_rate = clk_rate / (PLL_REFDIV * (1 << PLL_POSTPLDIV) * PLL_EXTDIV);

	div_int = freq / base_rate;
	div_frac = DIV_ROUND_CLOSEST_ULL((u64)(freq - base_rate * div_int) * (1 << 18), base_rate);

	printk("Setting MCLK to %u, clk_rate %u, base_rate %d, div_int %u, div_frac %u", freq, clk_rate, base_rate, div_int, div_frac);

	/* Loop until we converged to an acceptable value */
	do {
		ath79_audiodpll_do_meas_clear();
		ath79_pll_powerdown();
		udelay(100);

		/* Set PLL regs */
		ath79_pll_set_postpllpwd(PLL_POSTPLDIV);
		ath79_pll_bypass(0);
		ath79_pll_set_ext_div(PLL_EXTDIV);
		ath79_pll_set_refdiv(PLL_REFDIV);
		ath79_pll_set_target_div(div_int, div_frac);
		/* Set DPLL regs */
		ath79_audiodpll_range_set();
		ath79_audiodpll_phase_shift_set(6);
		
		if (clk_rate == 25000000)
			ath79_audiodpll_set_gains(61, 4);
		else if (clk_rate == 40000000)
			ath79_audiodpll_set_gains(50, 4);

		ath79_pll_powerup();
		ath79_audiodpll_do_meas_clear();
		ath79_audiodpll_do_meas_set();

		while ( ! ath79_audiodpll_meas_done_is_set()) {
			udelay(10);
		}

	} while (ath79_audiodpll_sqsum_dvc_get() >= 0x40000);

	return 0;
}

