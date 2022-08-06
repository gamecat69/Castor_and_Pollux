/*
    Copyright (c) 2021 Alethea Katherine Flowers.
    Published under the standard MIT License.
    Full text available at: https://opensource.org/licenses/MIT
*/

#include "gem_pulseout.h"
#include "wntr_ramfunc.h"

static bool hard_sync_ = false;

/* Public functions */

void gem_pulseout_init(const struct GemPulseOutConfig* po) {
    /* Enable the APB clock for TCC0 & TCC1. */
    PM->APBCMASK.reg |= PM_APBCMASK_TCC0 | PM_APBCMASK_TCC1;

    /* Enable GCLK1 and wire it up to TCC0 & TCC1 */
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | po->gclk | GCLK_CLKCTRL_ID_TCC0_TCC1;
    while (GCLK->STATUS.bit.SYNCBUSY) {};

    /* Reset TCCs. */
    TCC0->CTRLA.bit.ENABLE = 0;
    while (TCC0->SYNCBUSY.bit.ENABLE) {};
    TCC0->CTRLA.bit.SWRST = 1;
    while (TCC0->SYNCBUSY.bit.SWRST || TCC0->CTRLA.bit.SWRST) {};
    TCC1->CTRLA.bit.ENABLE = 0;
    while (TCC1->SYNCBUSY.bit.ENABLE) {};
    TCC1->CTRLA.bit.SWRST = 1;
    while (TCC1->SYNCBUSY.bit.SWRST || TCC1->CTRLA.bit.SWRST) {};

    /* Configure the clock prescaler for each TCC.
        This lets you divide up the clocks frequency to make the TCC count slower
        than the clock. In this case, I'm dividing the 8MHz clock by 16 making the
        TCC operate at 500kHz. This means each count (or "tick") is 2us.
    */
    TCC0->CTRLA.reg |= po->gclk_div;
    TCC1->CTRLA.reg |= po->gclk_div;

    /* Use "Normal PWM" */
    TCC0->WAVE.reg = TCC_WAVE_WAVEGEN_NPWM;
    while (TCC0->SYNCBUSY.bit.WAVE) {};
    TCC1->WAVE.reg = TCC_WAVE_WAVEGEN_NPWM;
    while (TCC1->SYNCBUSY.bit.WAVE) {};

    /* We have to set some sort of period to begin with, otherwise the
        double-buffered writes won't work. */
    TCC0->PER.reg = 100;
    TCC1->PER.reg = 100;

    /* Configure pins. */
    WntrGPIOPin_configure_alt(po->tcc0_pin);
    WntrGPIOPin_configure_alt(po->tcc1_pin);

    /* Enable output */
    TCC0->CTRLA.reg |= (TCC_CTRLA_ENABLE);
    while (TCC0->SYNCBUSY.bit.ENABLE) {};
    TCC1->CTRLA.reg |= (TCC_CTRLA_ENABLE);
    while (TCC1->SYNCBUSY.bit.ENABLE) {};

    /* Enable interrupt. */
    TCC0->INTENSET.bit.OVF = 1;
    NVIC_SetPriority(TCC0_IRQn, 1);
    NVIC_EnableIRQ(TCC0_IRQn);
}

void gem_pulseout_set_period(const struct GemPulseOutConfig* po, uint8_t channel, uint32_t period) {
    /* Configure the frequency for the PWM by setting the PER register.
        The value of the PER register determines the frequency in the following
        way:

            frequency = GLCK frequency / (TCC prescaler * (1 + PER))

        For example if PER is 512 then frequency = 8Mhz / (16 * (1 + 512))
        so the frequency is 947Hz.
    */
    switch (channel) {
        case 0:
            TCC0->PERB.bit.PERB = period;
            TCC0->CCB[po->tcc0_wo % 4].reg = (uint32_t)(period / 2);
            break;

        case 1:
            TCC1->PERB.bit.PERB = period;
            TCC1->CCB[po->tcc1_wo % 4].reg = (uint32_t)(period / 2);
            break;

        default:
            break;
    }
}

void gem_pulseout_hard_sync(bool state) { hard_sync_ = state; }

void TCC0_Handler(void) RAMFUNC;

void TCC0_Handler(void) {
    TCC0->INTFLAG.reg = TCC_INTFLAG_OVF;

    if (hard_sync_) {
        TCC1->CTRLBSET.reg = TCC_CTRLBSET_CMD_RETRIGGER;
    }
}
