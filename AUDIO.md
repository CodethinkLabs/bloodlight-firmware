Please refer to README.md for instructions on how to build the project,
flash firmware and other general uses. This document describes the specifics
of using Bloodlight to capture audio/vibration data.

# Test Environment

To get the strongest audio signal, it's required to mask as much ambient
light from the sensor as possible. We suggest draping black fabric over the
device during recording so that it's only recording light fluctuations
caused by the motion of the subject and not from the environment.

Bloodlight is a VERY sensitive device and without good shielding it will
record the tiniest fluctuation in light such as a bird passing a window, or
ANY motion in a lit room.

We suggest taping the bloodlight device down when recording strong bass
vibrations to avoid reverberation effects.

# Running Bloodview

To run Bloodview with good default settings for audio capture:
```
make -C host/ run BV_ARGS="-c rev2-audio.yaml"
```

This sets the device in continuous measurement mode at 48kHz, with the orange
LED enabled and PD1 enabled with 4-bit oversampling and no offset.

# Automatic Calibration

There's an auto-calibration feature in bloodview which will calibrate analog
and digital offset/amplification to provide the strongest possible signal.

Calibration must be run for at least 2 seconds as the device has a warm-up
period on initial run, but we'd recommend calibrating for at least 10 seconds
and ensuring that the measurements during the calibration period represent the
data you'll be recording.

If after calibration you see a flat-line in the output then it's likely that
your output is out-of-range compared to the calibration, in-which case you can
tweak the output. It's also possible that the device has run into some kind of
error, so try unplugging and starting again.

# Getting a WAV file

Whenever bloodview runs an aquisition (or calibration) a yaml file is stored
in the host/ directory, in the form of:
```
YYYY-MM-DD.hh:mm:ss-acq.yaml
```

To convert these captures into a wav file:
```
cat YYYY-MM-DD.hh:mm:ss-acq.yaml | host/build/convert wav > output.wav
```

You can then open the resulting WAV file in any audio editor, we suggest
using Audacity as it's free and has useful plugins:
```
audacity output.wav
```

# Manual Calibration

If automatic calibration is failing to provide a good result then it's possible
to manually calibrate.

## Opamp Offset

The opamp offset is the DC offset around which the waveform is expected,
if you don't know what the waveform should look like then a value of 2048
represents half of the range (1.65V).

Once you have a recording you can calculate the offset from the mid-point
of the waveform you're interested in. If the mid-point in audacity is 0.56
in audacity, we can calculate the proper offset as:
4095 - ((0.56 + 1) * 2048)

You'll notice that the offset is inverted (hence subtraction) and that the
range in audacity is -1..1 (which we convert to 0..2).

## Opamp Amplification

Once the correct opamp offset has been found either through calculation or
trial-and-error, we can apply an amplification.

Supported amplification values for rev2 are:
    - 1x
    - 3x
    - 7x
    - 15x
    - 31x
    - 63x

## Other Configuration Choices

We suggest a freqeuency of 48kHz, but lower frequencies may be selected,
there's a protocol limit of 65535 Hz currently despite the hardware
supporting slightly more.

We suggest using the Orange (590nm) LED for audio as this isn't too bright,
it seems that in most cases Blue or Green will saturate as they're designed
for use against/through skin rather than direct reflection from an object.

We suggest using PD1 as it's the most sensitive diode due to the largest
surface area, however PD2 should also work. PD3 currently doesn't support
hardware amplficiation/offset so while it would work; the signal would be
highly degraded in comparison.

It's possible to use software oversample to accumulate values higher than
0xFFF0, to do this simply increase the software oversampling value. Note that
at some point you'll hit the maximum frequency currently supported by the
device/hardware.

Hardware oversample is suggested to be 4 at all times because this provides
a low CPU overhead of accumulating 16-bit values, any more oversample will
require a hardware shift which will lead to bits being lost. Lower oversample
is possible but not useful. In the rare case that the amplified signal
occupies less than 50% of the range, it's possible to increase hardware
oversample by 1 to provide an additional 2x amplification without data loss.


