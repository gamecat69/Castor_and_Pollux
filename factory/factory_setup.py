import argparse

from hubble import Hubble
from wintertools import fw_fetch, jlink, reportcard, thermalprinter
from wintertools.print import print

from libgemini import adc_calibration, cv_calibration, gemini, ramp_calibration

DEVICE_NAME = "winterbloom_gemini"
JLINK_DEVICE = "ATSAMD21G18"
JLINK_SCRIPT = "scripts/flash.jlink"
REPORT = reportcard.Report(name="Castor & Pollux")


def erase_nvm():
    print("# Erasing NVM")
    gem = gemini.Gemini.get()
    gem.erase_lut()
    print("✓ Erased ramp look-up-table")
    gem.reset_settings()
    print("✓ Erased user settings")
    gem.soft_reset()
    print("Reset")


def get_firmware_and_serial():
    print("# Firmware & serial")

    gem = gemini.Gemini.get()
    fw_version = gem.get_firmware_version()
    serial = gem.get_serial_number()

    print(f"Firmware version: {fw_version}")
    print(f"Serial number: {serial}")

    REPORT.ulid = serial
    REPORT.sections.append(
        reportcard.Section(
            name="Firmware",
            items=[
                reportcard.LabelValueItem(
                    label="Version", value=fw_version, class_="stack"
                ),
                reportcard.LabelValueItem(
                    label="Serial number", value=serial, class_="stack"
                ),
            ],
        )
    )


def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "--stages",
        type=str,
        nargs="*",
        default=["firmware", "ramp", "adc", "cv"],
        help="Select which setup stages to run.",
    )

    args = parser.parse_args()

    hubble = Hubble.get()
    hubble.start()

    if "firmware" in args.stages:
        print("# Programming firmware")
        fw_fetch.latest_bootloader(DEVICE_NAME)
        jlink.run(JLINK_DEVICE, JLINK_SCRIPT)

    if "erase_nvm" in args.stages:
        erase_nvm()

    get_firmware_and_serial()

    if "adc" in args.stages:
        print("# Calibrating ADC")
        REPORT.sections.append(adc_calibration.run())

    if "cv" in args.stages:
        print("# Calibrating pitch CV")
        REPORT.sections.append(cv_calibration.run())

    if "ramp" in args.stages:
        print("# Calibrating ramps")
        REPORT.sections.append(ramp_calibration.run(save=True))

    gem = gemini.Gemini.get()
    gem.soft_reset()

    if args.stages != ["firmware", "ramp", "adc", "cv"]:
        print("!! Not all stages run, not creating report.")
        return

    print(REPORT)
    REPORT.save()
    reportcard.render_html(REPORT)

    if REPORT.succeeded:
        hubble.success()
        print.success()
    else:
        hubble.fail()
        print.failure()

    thermalprinter.print_me_maybe(reportcard.render_image(REPORT))


if __name__ == "__main__":
    main()
