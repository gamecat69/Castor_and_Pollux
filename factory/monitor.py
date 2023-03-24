# Copyright (c) 2021 Alethea Katherine Flowers.
# Published under the standard MIT License.
# Full text available at: https://opensource.org/licenses/MIT

"""Monitor Gemini's inputs."""

import pathlib
import time
import sys

from wintertools import fs, git, tui
from wintertools.print import print

from libgemini import gemini


COLOR_U12 = tui.rgb(230, 193, 253)
COLOR_U32 = tui.rgb(228, 255, 154)
COLOR_F16 = tui.rgb(144, 255, 203)
BEHAVIORS = ["Coarse", "Multiply", "Follow", "Fine"]
MODES = ["Normal", "LFO PWM", "LFO FM", "Hard Sync"]
KNOB_CHARS = "🕖🕗🕘🕙🕚🕛🕐🕑🕒🕓🕔"
GRAPH_CHARS = "▁▂▃▄▅▆▇█"
SPINNER_CHARS = "🌑🌒🌓🌔🌕🌖🌗🌘"

spinner_index = 0


def _color_range(v, low, high, color=(66, 224, 245)):
    t = (v - low) / (high - low)
    return tui.rgb(tui.gradient((0.5, 0.5, 0.5), color, t))


def _format_u12(v):
    if v > 4096:
        return f"{tui.reset}{tui.italic}{tui.rgb(0.4, 0.4, 0.4)}", "-", tui.reset
    c = _color_range(v, 0, 4096, (230, 193, 253))
    return f"{tui.reset}{tui.italic}{c}", f"{v}", tui.reset


def _format_knob(v):
    start, mid, end = _format_u12(v)

    knob = ""
    if v <= 4096:
        knob = KNOB_CHARS[round((v / 4095) * (len(KNOB_CHARS) - 1))]

    return start, mid, knob, end


def _format_cv(v, invert=False):
    if invert:
        v = 4095 - v
    start, mid, end = _format_u12(v)
    bar = GRAPH_CHARS[round((v / 4095) * (len(GRAPH_CHARS) - 1))]
    return start, mid, bar, end


def _draw(update):
    global spinner_index

    spinner_index = (spinner_index + 1) % len(SPINNER_CHARS)
    spinner = SPINNER_CHARS[spinner_index]

    COLUMNS = tui.Columns("<2", "<20", "<14", "<11", ">2")
    COLUMNS_WITH_SYMBOLS = tui.Columns("<2", "<20", "<6", "<8", "<6", "<5", ">2")

    print("┏", "━" * 47, "┓", sep="")
    COLUMNS.draw("┃", "", tui.bold, "Castor", "Pollux", "┃")
    COLUMNS_WITH_SYMBOLS.draw(
        "┃",
        tui.bold,
        "Pitch CV",
        *_format_cv(update.castor_pitch_cv, invert=True),
        *_format_cv(update.pollux_pitch_cv, invert=True),
        "┃",
    )
    COLUMNS_WITH_SYMBOLS.draw(
        "┃",
        tui.bold,
        "Pitch Knob₁",
        *_format_knob(update.castor_pitch_knob),
        *_format_knob(update.pollux_pitch_knob),
        "┃",
    )
    COLUMNS_WITH_SYMBOLS.draw(
        "┃",
        tui.bold,
        "Pitch Knob₂",
        *_format_knob(update.castor_tweak_pitch_knob),
        *_format_knob(update.pollux_tweak_pitch_knob),
        "┃",
    )
    print("┃", " " * 47, "┃", sep="")
    COLUMNS_WITH_SYMBOLS.draw(
        "┃",
        tui.bold,
        "Pulse CV",
        *_format_cv(update.castor_pulse_cv),
        *_format_cv(update.pollux_pulse_cv),
        "┃",
    )
    COLUMNS_WITH_SYMBOLS.draw(
        "┃",
        tui.bold,
        "Pulse Knob₁",
        *_format_knob(update.castor_pulse_knob),
        *_format_knob(update.pollux_pulse_knob),
        "┃",
    )
    COLUMNS_WITH_SYMBOLS.draw(
        "┃",
        tui.bold,
        "Pulse Knob₂",
        *_format_knob(update.castor_tweak_pulse_knob),
        *_format_knob(update.pollux_tweak_pulse_knob),
        "┃",
    )
    print("┃", " " * 47, "┃", sep="")
    COLUMNS.draw(
        "┃",
        tui.bold,
        "Behavior",
        tui.reset,
        tui.italic,
        BEHAVIORS[update.castor_pitch_behavior],
        BEHAVIORS[update.pollux_pitch_behavior],
        "┃",
    )
    print("┃", " " * 47, "┃", sep="")
    COLUMNS.draw(
        "┃",
        tui.bold,
        "Ref Pitch",
        tui.reset,
        tui.italic,
        COLOR_F16,
        "",
        f"{update.pollux_reference_pitch:0.3f}",
        tui.reset,
        "┃",
    )
    COLUMNS.draw(
        "┃",
        tui.bold,
        "Pitch",
        tui.reset,
        tui.italic,
        COLOR_F16,
        f"{update.castor_pitch:0.3f}",
        f"{update.pollux_pitch:0.3f}",
        tui.reset,
        "┃",
    )
    COLUMNS.draw(
        "┃",
        tui.bold,
        "Period",
        tui.reset,
        tui.italic,
        COLOR_U32,
        update.castor_period,
        update.pollux_period,
        tui.reset,
        "┃",
    )
    COLUMNS.draw(
        "┃",
        tui.bold,
        "Pulse",
        tui.reset,
        tui.italic,
        COLOR_U12,
        update.castor_pulse_width,
        update.pollux_pulse_width,
        tui.reset,
        "┃",
    )
    COLUMNS.draw(
        "┃",
        tui.bold,
        "Ramp",
        tui.reset,
        tui.italic,
        COLOR_U12,
        update.castor_ramp,
        update.pollux_ramp,
        tui.reset,
        "┃",
    )
    print("┃", " " * 47, "┃", sep="")
    COLUMNS_WITH_SYMBOLS.draw(
        "┃",
        tui.bold,
        "LFO Knob₂",
        *_format_knob(update.lfo_knob),
        "",
        "",
        "┃",
    )
    COLUMNS_WITH_SYMBOLS.draw(
        "┃",
        tui.bold,
        "LFO Knob₂",
        *_format_knob(update.tweak_lfo_knob),
        "",
        "",
        "┃",
    )
    COLUMNS.draw(
        "┃",
        tui.bold,
        "Tweaking",
        tui.reset,
        tui.italic,
        COLOR_U32,
        "◻️" if update.tweaking else "◼️",
        "",
        tui.reset,
        "┃",
    )
    print("┃", " " * 47, "┃", sep="")
    COLUMNS.draw(
        "┃",
        tui.bold,
        "Mode",
        tui.reset,
        tui.italic,
        MODES[update.mode],
        "",
        tui.reset,
        "┃",
    )
    COLUMNS.draw(
        "┃",
        tui.bold,
        "Loop time",
        tui.reset,
        tui.italic,
        COLOR_U32,
        update.loop_time,
        "",
        tui.reset,
        "┃",
    )
    COLUMNS.draw(
        "┃",
        tui.bold,
        "Animation time",
        tui.reset,
        tui.italic,
        COLOR_U32,
        update.animation_time,
        "",
        tui.reset,
        "┃",
    )
    COLUMNS.draw(
        "┃",
        tui.bold,
        "Sample time",
        tui.reset,
        tui.italic,
        COLOR_U32,
        update.sample_time,
        f"{spinner}",
        tui.reset,
        "┃",
    )
    print("┗", "━" * 47, "┛", sep="")


def _check_firmware_version(gem):
    latest_release = git.latest_tag()
    build_id = gem.get_firmware_version()
    print(f"Firmware build ID: {build_id}")

    if latest_release in build_id:
        return True
    else:
        return False


def _update_firmware(gem):
    print("!! Firmware is out of date, updating it..")

    gem.reset_into_bootloader()

    path = pathlib.Path(fs.wait_for_drive("GEMINIBOOT", timeout=60 * 5))

    fs.copyfile("../firmware/build/gemini-firmware.uf2", path / "firmware.uf2")
    fs.flush(path)

    time.sleep(3)

    print("[green]Firmware updated![/]")

    build_id = gem.get_firmware_version()
    print(f"Firmware build ID: {build_id}")


def main(stats=False):
    gem = gemini.Gemini.get()

    if "--no-update" not in sys.argv and (
        "--force-update" in sys.argv or not _check_firmware_version(gem)
    ):
        _update_firmware(gem)

    settings = gem.read_settings()

    print(settings)

    gem.enable_monitor()

    output = tui.Updateable(clear_all=False)

    with output:
        while True:
            update = gem.monitor()

            _draw(update)

            output.update()


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        if isinstance(e, KeyboardInterrupt):
            sys.exit(0)
