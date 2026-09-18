
// ****************************************************************************
//
//                                Overclock
//
// ****************************************************************************

#include "include.h"
#include "hardware/pll.h"   // PICO_PLL_VCO_MIN/MAX_FREQ_HZ

// Search PLL setup
//  reqkhz ... required output frequency in kHz
//  input ... PLL input frequency in kHz (default 12000, or use clock_get_hz(clk_ref)/1000)
//  vcomin ... minimal VCO frequency in kHz (default 400000)
//  vcomax ... maximal VCO frequency in kHz (default 1600000)
//  lowvco ... prefer low VCO (lower power but more jiter)
// outputs:
//  outkhz ... output achieved frequency in kHz (0=not found)
//  outvco ... output VCO frequency in kHz
//  outfbdiv ... output fbdiv (16..320)
//  outpd1 ... output postdiv1 (1..7)
//  outpd2 ... output postdiv2 (1..7)
// Returns true if precise frequency has been found, or near frequency used otherwise.
bool vcocalc(u32 reqkhz, u32 input, u32 vcomin, u32 vcomax, bool lowvco,
		u32* outkhz, u32* outvco, u16* outfbdiv, u8* outpd1, u8* outpd2)
{
	u32 khz, vco, margin;
	u16 fbdiv;
	u8 pd1, pd2;
	u32 margin_best = 100000;
	*outkhz = 0;

	// fbdiv loop
	fbdiv = lowvco ? 16 : 320;
	for (;;)
	{
		// get current vco
		vco = fbdiv * input;

		// check vco range
		if ((vco >= vcomin) && (vco <= vcomax))
		{
			// pd1 loop
			for (pd1 = 7; pd1 >= 1; pd1--)
			{
				// pd2 loop
				for (pd2 = pd1; pd2 >= 1; pd2--)
				{
					// current output frequency
					khz = vco / (pd1 * pd2);

					// check best frequency
					margin = abs((int)(khz - reqkhz));
					if (margin < margin_best)
					{
						margin_best = margin;
						*outkhz = khz;
						*outvco = vco;
						*outfbdiv = fbdiv;
						*outpd1 = pd1;
						*outpd2 = pd2;
					}
				}
			}
		}

		// shift fbdiv
		if (lowvco)
		{
			fbdiv++;
			if (fbdiv > 320) break;
		}
		else
		{
			fbdiv--;
			if (fbdiv < 16) break;
		}
	}

	// check precise frequency
	return (*outkhz == reqkhz) && (*outvco == *outkhz * *outpd1 * *outpd2);
}

// find sysclock setup (use set_sys_clock_pll to set sysclock)
//  reqkhz ... required frequency in kHz
// outputs:
//  outkhz ... output achieved frequency in kHz (0=not found)
//  outvco ... output VCO frequency in kHz
//  outfbdiv ... output fbdiv (16..320)
//  outpd1 ... output postdiv1 (1..7)
//  outpd2 ... output postdiv2 (1..7)
// Returns true if precise frequency has been found, or near frequency used otherwise.
bool FindSysClock(u32 reqkhz, u32* outkhz, u32* outvco, u16* outfbdiv, u8* outpd1, u8* outpd2)
{
	// get reference frequency in kHz (should be 12 MHz)
	u32 input = clock_get_hz(clk_ref)/1000;

	// find PLL setup.  Use the SDK's VCO limits (RP2350 needs >=750 MHz, the
	// RP2040 >=400 MHz) so pll_init() gets a lockable configuration; the old
	// hard-coded 400 MHz minimum left an invalid VCO on RP2350 and the system
	// clock never locked (VGA sync appeared briefly then died).
	return vcocalc(reqkhz, input, PICO_PLL_VCO_MIN_FREQ_HZ/1000, PICO_PLL_VCO_MAX_FREQ_HZ/1000,
	               false, outkhz, outvco, outfbdiv, outpd1, outpd2);
}

// set flash SSI speed (4 default, <4 faster, >4 slower)
void __not_in_flash_func(FlashSpeedSetup)(int baud)
{
#if !(defined(PICO_RP2350) && PICO_RP2350)
	// disable SSI for further config
	ssi_hw->ssienr = 0;

	// set baud rate
	ssi_hw->baudr = baud;

	// Re-enable
	ssi_hw->ssienr = 1;
#else
	(void)baud;   // RP2350 QMI flash timing is handled by the SDK
#endif
}
