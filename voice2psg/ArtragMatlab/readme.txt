Voice Converter for SN76489
---------------------------

This tool converts WAV files into playback data for the SN76489 and compatible
sound chips, intended to play back at 60Hz. Just 6 bytes per frame are required,
giving a playback rate of 360 bytes per second.

The output files use all three voice channels and volumes (the system does not
currently manipulate the noise channel).

Playback routines for the TI-99/4A and ColecoVision are also provided.

The converter is somewhat no-frills and requires the MATLAB compiler runtime to
operate (which is a fairly large download!) It is designed to perform bulk
conversion as well as give an immediate preview of the output.

At this time, only a 64-bit executable is available. In addition, the souce does
not operate completely correctly under Octave.

INSTALLING
----------

The executable sn76489.exe is all you need, although the source code is also
provided as well as notes on the MATLAB compiler.

You must first install the MATLAB runtime. It MUSt be the exact version R2012b.
You must also select the 32-bit or 64-bit runtime depending on which version you
intend to run - they are not compatible with each other.

You will find it on this page: http://www.mathworks.com/products/compiler/mcr/index.html

Scroll down for R2012b (8.0), and download and install the 64-bit runtime. This is
about a 372MB download for Windows.

RUNNING
-------

Create a folder named 'wavs' at the same level as sn76489.exe, and place the wave
files that you wish to convert into it. Then simply run sn76489.exe. (You may double-
click it, but if you run it in a command-prompt you will see some additional
output).

After a few seconds, if no errors occurred, a window will pop up displaying the first
waveform. The system will process the waveform, then it will first play the original
waveform, and then a simulation of the converted waveform. The output data will be
written to the 'wavs' folder with the suffix "_sn76489.bin"

After the last file is converted, the program will simply stop. You can close the
window at that time.

CONVERSION
----------

Also included is a small C tool (with 32-bit Windows binary) to convert the output
file into a VGM file. No validation is performed, but this may be useful for use
with other tools, or just a quick playback verification in a VGM player. (Mostly
provided to use with my VGM compressor - http://harmlesslion.com/software/vgmcomp -
it manages about 40% savings.)

INFORMATION
-----------

The data file is written in a very simple packed format. Each frame consists of three
little endian 16-bit words, representing one channel of information each.

Each word is packed as follows (shown here in big endian order, remember to account
for endianess):

MSB       LSB
XXVV VVPP PPPP PPPP

XX = 00 always, 11 if the frame has ended (this word is not played).
VVVV = volume in the SN76489 format (0=no attenuation, 15=silent)
PPPPPPPPPP = period of the tone in 1-1023, where 1 = 111860Hz to 1023 = 109Hz

To play back the data, simply parse each word to a separate channel, and
delay 1/60hz of a second (it's convenient to play on the vertical blank).

The conversion tool was written by Artrag.
The TI-99/4A and ColecoVision playback was written by Tursi.
http://harmlesslion.com/software/artplay

UPDATES
-------

8/13/2016 - artrag optimized the ColecoVision playback code
