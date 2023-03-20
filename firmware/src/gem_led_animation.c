/*
    Copyright (c) 2021 Alethea Katherine Flowers.
    Published under the standard MIT License.
    Full text available at: https://opensource.org/licenses/MIT
*/

#include "gem_led_animation.h"
#include "fix16.h"
#include "gem_config.h"
#include "gem_dotstar.h"
#include "wntr_colorspace.h"
#include "wntr_random.h"
#include "wntr_ticks.h"
#include "wntr_waveforms.h"
#include <stdint.h>

struct GemLEDTweakData gem_led_tweak_data = {.lfo_value = F16(0)};

// TODO: This probably needs to be adjusted for rev 5.
static const uint32_t hue_offsets_[GEM_MAX_DOTSTAR_COUNT] = {
    65355 / GEM_MAX_DOTSTAR_COUNT * 2,
    65355 / GEM_MAX_DOTSTAR_COUNT * 2,
    65355 / GEM_MAX_DOTSTAR_COUNT * 6,
    0,
    65355 / GEM_MAX_DOTSTAR_COUNT,
    65355 / GEM_MAX_DOTSTAR_COUNT * 4,
    65355 / GEM_MAX_DOTSTAR_COUNT * 3,
};
static enum GemMode mode_ = GEM_MODE_NORMAL;
static uint32_t last_update_;
static fix16_t phase_a_ = F16(0);
static uint32_t hue_accum_;
static uint8_t sparkles_[GEM_MAX_DOTSTAR_COUNT];
static bool transitioning_ = false;

/* Forward declarations. */

static void animation_step_transition_(const struct GemDotstarCfg* dotstar, uint32_t delta) RAMFUNC;
static void animation_step_sparkles_(const struct GemDotstarCfg* dotstar, uint32_t delta) RAMFUNC;
static void animation_step_normal_(const struct GemDotstarCfg* dotstar, uint32_t delta) RAMFUNC;
// static void animation_step_single_hue_(const struct GemDotstarCfg* dotstar, uint16_t hue, uint32_t delta) RAMFUNC;
static void animation_step_calibration_(const struct GemDotstarCfg* dotstar, uint32_t ticks);
static void animation_step_tweak_(const struct GemDotstarCfg* dotstar, uint32_t ticks) RAMFUNC;

/* Public functions. */

void gem_led_animation_init() { last_update_ = wntr_ticks(); }

void gem_led_animation_set_mode(enum GemMode mode) {
    mode_ = mode;
    phase_a_ = F16(0);
    transitioning_ = true;
}

bool gem_led_animation_step(const struct GemDotstarCfg* dotstar) {
    uint32_t ticks = wntr_ticks();
    uint32_t delta = ticks - last_update_;
    if (delta < GEM_ANIMATION_INTERVAL) {
        return false;
    }

    last_update_ = ticks;

    if (transitioning_) {
        animation_step_transition_(dotstar, delta);
    } else {
        switch (mode_) {
            case GEM_MODE_NORMAL:
                animation_step_normal_(dotstar, delta);
                animation_step_sparkles_(dotstar, delta);
                break;
            case GEM_MODE_LFO_PWM:
                animation_step_normal_(dotstar, delta);
                animation_step_sparkles_(dotstar, delta);
                break;
            case GEM_MODE_LFO_FM:
                animation_step_normal_(dotstar, delta);
                animation_step_sparkles_(dotstar, delta);
                break;
            case GEM_MODE_HARD_SYNC:
                animation_step_normal_(dotstar, delta);
                animation_step_sparkles_(dotstar, delta);
                break;
            case GEM_MODE_CALIBRATION:
                animation_step_calibration_(dotstar, ticks);
                break;
            case GEM_MODE_FLAG_TWEAK:
                animation_step_tweak_(dotstar, delta);
                break;
            default:
                break;
        }
    }

    gem_dotstar_update(dotstar);

    return true;
}

/* Private functions. */

static void animation_step_transition_(const struct GemDotstarCfg* dotstar, uint32_t delta) {
    phase_a_ += fix16_div(fix16_from_int(delta), F16(1000.0));
    if (phase_a_ > F16(1.0)) {
        transitioning_ = false;
    }

    uint16_t hue = 0;
    uint8_t sat = 255;

    switch (mode_) {
        case GEM_MODE_NORMAL:
            hue = 13107;
            break;
        case GEM_MODE_LFO_FM:
            hue = 21845;
            break;
        case GEM_MODE_LFO_PWM:
            hue = 39321;
            break;
        case GEM_MODE_HARD_SYNC:
            hue = 52428;
            break;
        default:
            break;
    }

    for (size_t i = 0; i < dotstar->count; i++) {
        uint8_t value = (uint8_t)(fix16_to_int(fix16_mul(fix16_sub(F16(1), phase_a_), F16(255))));
        uint32_t color = wntr_colorspace_hsv_to_rgb(hue, sat, value);
        gem_dotstar_set32(i, color);
    }
}

static void animation_step_sparkles_(const struct GemDotstarCfg* dotstar, uint32_t delta) {
    for (size_t i = 0; i < dotstar->count; i++) {

        if (wntr_random32() % 400 == 0)
            sparkles_[i] = 255;

        if (sparkles_[i] == 0) {
            continue;
        }

        uint16_t hue = (hue_accum_ + hue_offsets_[i]) % UINT16_MAX;
        uint32_t color = wntr_colorspace_hsv_to_rgb(hue, 255 - sparkles_[i], 127);
        gem_dotstar_set32(i, color);

        if (sparkles_[i] <= delta / 4)
            sparkles_[i] = 0;
        else
            sparkles_[i] -= delta / 4;
    }
}

static void animation_step_normal_(const struct GemDotstarCfg* dotstar, uint32_t delta) {
    phase_a_ += fix16_div(fix16_from_int(delta), F16(2200.0));
    if (phase_a_ > F16(1.0))
        phase_a_ = fix16_sub(phase_a_, F16(1.0));

    hue_accum_ += delta * 5;

    for (size_t i = 0; i < dotstar->count; i++) {
        fix16_t phase_offset = fix16_div(fix16_from_int(i), fix16_from_int(dotstar->count));
        fix16_t sin_a = wntr_sine_normalized(phase_a_ + phase_offset);
        uint8_t value = 20 + fix16_to_int(fix16_mul(sin_a, F16(235)));
        uint16_t hue = (hue_accum_ + hue_offsets_[i]) % UINT16_MAX;
        uint32_t color = wntr_colorspace_hsv_to_rgb(hue, 255, value);
        gem_dotstar_set32(i, color);
    }
}

// static void animation_step_single_hue_(const struct GemDotstarCfg* dotstar, uint16_t hue, uint32_t delta) {
//     phase_a_ += fix16_div(fix16_from_int(delta), F16(2200.0));
//     if (phase_a_ > F16(1.0))
//         phase_a_ = fix16_sub(phase_a_, F16(1.0));

//     hue_accum_ = hue;
//     for (size_t i = 0; i < dotstar->count; i++) {
//         fix16_t phase_offset = fix16_div(fix16_from_int(i), fix16_from_int(dotstar->count));
//         fix16_t sin_a = wntr_sine_normalized(phase_a_ + phase_offset);
//         uint8_t value = 20 + fix16_to_int(fix16_mul(sin_a, F16(235)));
//         uint32_t color = wntr_colorspace_hsv_to_rgb(hue, 255, value);
//         gem_dotstar_set32(i, color);
//     }
// }

static void animation_step_calibration_(const struct GemDotstarCfg* dotstar, uint32_t ticks) {
    fix16_t bright_time = fix16_div(fix16_from_int(ticks / 2), F16(2000.0));
    fix16_t sinv = wntr_sine_normalized(bright_time);
    uint8_t value = fix16_to_int(fix16_mul(F16(255.0), sinv));
    uint32_t colora = wntr_colorspace_hsv_to_rgb(50000, 255, value);
    uint32_t colorb = wntr_colorspace_hsv_to_rgb(10000, 255, 255 - value);

    for (uint8_t i = 0; i < dotstar->count; i++) {
        if (i % 2 == 0) {
            gem_dotstar_set32(i, colora);
        } else {
            gem_dotstar_set32(i, colorb);
        }
    }
}

static void animation_step_tweak_(const struct GemDotstarCfg* dotstar, uint32_t delta) {
    hue_accum_ += delta;

    for (uint8_t i = 0; i < dotstar->count; i++) { gem_dotstar_set32(i, 0); }

    if (gem_led_tweak_data.castor_pwm) {
        gem_dotstar_set(0, 0, 255, 255);
        gem_dotstar_set(1, 0, 255, 255);
    }

    if (gem_led_tweak_data.pollux_pwm) {
        gem_dotstar_set(2, 255, 0, 255);
        gem_dotstar_set(3, 255, 0, 255);
    }

    fix16_t lfoadj = fix16_div(fix16_add(gem_led_tweak_data.lfo_value, F16(1.0)), F16(2.0));
    uint8_t lfo_value = fix16_to_int(fix16_mul(F16(255.0), lfoadj)) & 0xFF;
    gem_dotstar_set32(4, wntr_colorspace_hsv_to_rgb(UINT16_MAX / 12 * 2, 255, lfo_value));
    gem_dotstar_set32(5, wntr_colorspace_hsv_to_rgb(UINT16_MAX / 12 * 2, 255, lfo_value));
    gem_dotstar_set32(6, wntr_colorspace_hsv_to_rgb(UINT16_MAX / 12 * 2, 255, lfo_value));
}
