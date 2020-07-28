Bloodlight Firmware
===================

This repository contains firmware for the Bloodlight Medical Plethysmograph
(PPG) testbed hardware.
It is a [libopencm3](https://github.com/libopencm3/libopencm3) based project.

The aim of the project is to provide a testbed for researching what sort
of information can be obtained with different wavelengths of light for
[heart rate monitors](https://en.wikipedia.org/wiki/Heart_rate_monitor) and
[photoplethysmography devices](https://en.wikipedia.org/wiki/Photoplethysmogram).

The open-source hardware is available in the
[bloodlight-hardware](https://github.com/CodethinkLabs/bloodlight-hardware)
repository.

Building
--------

First, ensure you have the `libopencm3` submodule:

```bash
git submodule init
git submodule update
```

And build it:

```bash
make -C libopencm3/
```

Now you can build the `mpd-firmware.elf`:

```bash
make
```

And the MPD host tools:

```bash
make -C tools/
```

Installation
------------

There are various ways to install the firmware onto the device.
Here we will cover using [OpenOCD](http://openocd.org/) with
`gdb-multiarch`, using a Nucleo-F303ZE board or the STLINK-V3SET
to provide the ST-LINK interface.

### Hardware setup

1. Consult the
   [UM1974](https://www.st.com/en/evaluation-tools/nucleo-f303ze.html#resource) datasheet.
   The pin connections for STLINK-V3 are similar, but with slightly different name.
2. Locate the ST-LINK jumper pins on the programmer boards and make sure they are
   open.  See Figure 10: Using ST-LINK/V2-1 to program the STM32 on an
   external application in UM1974.
3. Connect the SWD pins on the Nucleo board to the pins on the MPD
   bloodlight board.  The pins on the MPD bloodlight board are named on the
   board.  For the Nucleo board, check see Table 5. Debug connector CN6 (SWD).
   There is a small dot at one end of the SWD connector which marks pin 1.

   | Nucleo board  | STLINK-V3 | MPD board |
   | ------------- | --------- | --------- |
   | VDD_TARGET    | T_VCC     | 3.3V      |
   | SWCLK         | CLK       | SWCLK     |
   | GND           | GND       | GND       |
   | SWDIO         | DIO       | SWDIO     |
   | NRST          | NRST      | NRST      |
   | SWO           | SWO       | SWO       |

4. Connect both the programmer and the MPD bloodlight boards to your computer using
   micro-USB cables as both require power to function. The order in which they're
   connected doesn't matter.

### Flashing

First run OpenOCD with an appropriate config:

```bash
openocd -f board/st_nucleo_f3.cfg
```

With that running, in another terminal, run:

```bash
gdb-multiarch mpd-firmware.elf
```

At the GDB command prompt connect to the OpenOCD interface to the device:

```
(gdb) target extended-remote localhost:3333
```

Reset the device and load on the firmware:

```
(gdb) monitor reset halt
(gdb) load
```

Now the device is flashed with the `mpd-firmware.elf` firmware.

Run the firmware with:

```
(gdb) c
```

Using the device
----------------

With the device flashed, we can plug it into the host and see what happens.
Connect the MPD device's Micro-USB port to the host system.

If you follow `dmesg` on the host with:

```bash
dmesg -w
```

You should see the MPD device enumerate when it gets plugged into the host
USB:

```
[877656.001199] usb 1-1: Product: Medical Plethysmograph Device
[877656.001202] usb 1-1: Manufacturer: Codethink
[877656.001204] usb 1-1: SerialNumber: ct-mpd:000000
[877656.004648] cdc_acm 1-1:1.0: ttyACM2: USB ACM device
```

Take note of the `ttyACM2` if you want to manually provide the device name
later in the command.  The `2` might not be a `2` in your case,
and we need to know this in order to communicate with the device.

The device comes up as a Communications Device Class (CDC) device.
It is controlled by sending messages to it over a USB serial connection.

The message protocol is defined in [src/msg.h](src/msg.h).

To control the device, use the `bl` host helper tool, at `tools/bl`.

Running it without any parameters will list the commands it supports:

```
./tools/bl

Usage:
  ./tools/bl CMD [params]

Available CMDs:
  led       Turn LEDs on/off
  chancfg   Set configuration for a given channel
  start     Start an acquisition
  abort     Abort an acquisition
```

Running a command will show if you need to pass any parameters to the
command:

```
./tools/bl led

Usage:
  ./tools/bl led \
  	<DEVICE_PATH|--auto|-a> \
  	<LED_MASK>
```

The DEVICE_PATH is the tty device reported in `dmesg` when the device was
connected. When `--auto` or `-a` are provided, it will try to guess the
tty deivce name based on the `manufactuer` and `product` fields of USB
devices.

The LED_MASK is a 16-bit mask of the 16 LEDs on the device.  If a bit is
set, then the corresponding LED is turned on:

```
./tools/bl led /dev/ttyACM2 0x8000

Failed to open '/dev/ttyACM2': Permission denied
```

This didn't work because we need to have permission to open `/dev/ttyACM2`.
Running with `sudo` it should work:

```
sudo tools/bl led /dev/ttyACM2 0x8000

- LED:
    LED Mask: 0x8000
- Response:
    Response to: LED
    Error code: 0
```

The output shows the message that was sent, followed by the response message
from the device.

The above command would turn on one of the LEDs.  To turn off all the LEDs,
set all the bits to zero:

```
sudo ./tools/bl led /dev/ttyACM2 0x0
```

To run an acquisition, it is simplest to use the [run.sh](run.sh) script
either directly or as an example:

```
sudo ./run.sh cal
sudo ./run.sh acq
sudo ./run.sh cal_acq
sudo ./run.sh off
```

This will show the messages being sent and received to run an acquisition,
and also the sample data. See the comments in the [run.sh](run.sh) script
for further details on how to configure an acquisition.

Overridding the default acquistion parameters is possible too, for example:

```
sudo FREQUENCY=1000 OVERSAMPLE=128 SRC_MASK=0x5 ./run.sh acq
```

Device info
-----------

There are 16 LEDs.  Each of the LEDs emits a different light wavelength.
The LEDs were chosen to cover a range of the visible spectrum and into
infra-red.

There are four photodiodes, which have different ranges of the spectrum
that they are sensitive to.

When setting up an acquisition, one of the parameters is the SOURCE_MASK.
This is a mask of the sources to enable for the acquisition.

Sources are things that we sample with the ADCs on the device.  They
include the four photodiodes, and also other sources which may be useful
for debug or reference.

See the `enum bl_acq_source` in [src/acq.h](src/acq.h) for a complete
list of acquisition sources.

If an acquisition is set up with a SOURCE_MASK of `0x1` only the first
source (`BL_ACQ_PD1`: photodiode 1) would be enabled for the acquisition.

If an acquisition is set up with a SOURCE_MASK of `0x7`, with the bottom
three bits set, then the first three sources (photodiodes 1, 2 and 3)
would be enabled.

Note that there are four ADCs on the device, and each one can have
multiple sources connected to it.

To see which sources are connected to which ADCs, see `acq_source_table[]`
in [src/acq.c](src/acq.c).

Of particular note is that the `sample_data` message
(see [src/msg.h](src/msg.h)) contains a `src_mask` field.
This is because the way the firmware works is that each of
the ADCs builds its sample data messages separately, and sends
them when they are filled.

Inspecting acquisition data
---------------------------

The raw sample values from an acquisition don't really give much of an
insight into what is happening.

The output from an acquisition is in YAML format, so it should be easy to
write code to load it for analysis.

Currently there is a simple conversion tool (`tools/convert`) which can
turn the sample value data into a WAV file for loading into
[Audacity](https://www.audacityteam.org/).

```
./tools/convert 

Usage:
  ./tools/convert CMD [params]

Available CMDs:
  wav     Convert to WAVE format
  raw     Convert to RAW binary data
  csv     Convert to CSV
  relay   Relay stdin to stdout
```

The `relay` command is really intended for testing the message parsing.

The following command pipes the output from `tools/bl` into `tools/convert`
to create the file `out.wav`:

```
sudo ./run.sh /dev/ttyACM2 | ./tools/convert wav out.wav
```

Hit `ctrl+c` to end the acquisition and load the WAV file for inspection
with:

```bash
audacity out.wav
```

Audacity tips
-------------

* You increase the vertical space given to a data channel by dragging the
  bottom of its box downwards.  This stretches the wave vertically.
* Using a mouse scroll-wheel with `ctrl` zooms the signal along the X axis.
* With "Menu > View > Zoom > Advanced Vertical Zooming" ticked, you can click
  on the area to the left of the vertical axis, showing the values to zoom in
  on that part.
* You can use "Menu > Effect > Filter Curve" to filter out high frequencies,
  e.g. filter out above 20 Hz.
* You can select a region of the signal and use
  "Menu > Analyze > Plot Spectrum..." to see the signal in the frequency
  domain.
