#!/bin/sh
#
# SPQR/Ides of March Asset Build System
# 1/2017
#
# John Adams <jna@retina.net> @netik
# Bill Paul <wpaul@windriver.com>
#
# Script to convert a video file into the format used by the
# the video player on the Ides of DEF CON badge. The format isn't
# very complicated: a .VID file contains compressed jpeg video frames
# and raw 16-bit audio sample data. The player just loads the data,
# decompresses the video frames and transfers them to the screen
# and the audio to the I2S interface as fast as it can.
#
# Usage is:
# encode_video.sh file.mp4 destination/directory/path
#
# Required utilities:
# ffmpeg
# sox
#
# The destination directory must already exist
#
# The video will have an absolute resolution of 320x240 pixels and
# a frame rate of 21.675 frames per second. These values have been
# chosen to coincide with the audio sample rate of 15625Hz. The video
# and audio will be synchronized during playback.
#
# You may be wondering why we chose a video rate of 21.675 frames/second
# and an audio sample rate of 15.6KHz here. (Or maybe not. But I'm going
# to tell you anyway.)
#
# In order to keep the audio and video in sync, we need the ratio of audio
# samples per video scanline to be a whole number. (That is, if you divide
# the number of audio samples per second by the number of video scanlines
# per second, you should end up with a whole number as a result.)
# 
# With the DC25 badge design, we used a simple DAC for audio output and
# controlled the sample rate using an interval timer. This allowed the
# audio sample rate to be set to any value that we wanted, which in turn
# allowed us to choose a sample rate that yielded a whole number of samples
# per scaline.
#
# For the DC28 design, we're using the SAI controller in the ST Micro
# STM32F746 in I2S mode as the audio output device. To transfer data
# to the I2S codec, we need to use a master clock and a left/right
# clock (also known as a frame clock). With the ST Micro SAI design
# the left/right clock is always derived from the master clock using
# a divisor ratio of 256, and the master clock is derived from the SAI
# input clock. The SAI input clock is in turn from the PLL subsystem.
#
# The default PLL setup takes the external crystal value of 25MHz and
# divides it down to 1MHz before feeding it a number of PLL VCOs with
# configurable multipliers for their outputs. We use the I2S PLL,
# which is configured for 256MHz and then divided down to 16MHz. This
# is the SAI input clock.
#
# This allows us some flexibility but does not quite allow us to choose
# any arbitrary values. As with the DC27 design, we'd like a sample rate
# of 16KHz, With a 16MHz input clock, the closest you can get is
# 15.625KHz. (16 / 4 = 4MHz, 4MHz / 256 = 15.625KHz.)
#
# We have enough bandwidth for about 16 frames per second, but at that
# frame rate, you can't quite get a whole number of samples per scanline.
#
# For some reason I had become fixated on the idea of having a whole
# number video frame rate, but then I remembered that fractional frame
# rates are a perfectly acceptable thing and ffmpeg supports them just
# fine. (The NTSC frame rate is 29.97 fps after all.)
#
# The problem though is that even with 21.675 frames/second, we get
# close to a whole number ratio, but not quite:
#
# 21.675 x 240 = 5202 scanlines/second
# 15625 / 5202 = 3.00365244136870434448 samples/scanline
#
# Again, we're stuck with the LRCLK frequency of 15.625KHz. Our only
# remaining recourse is to cheat a little, and encode our audio tracks
# at a rate of 15.606KHz:
#
# 15606 / 5202 = 3.00
#
# This is slightly slower than the rate expected by the SAI controller,
# and it effectively results in the audio playback being slowed down
# compared to the original. However it amounts to a difference of
# only 0.16%, which is so small that a human listener can't really
# notice it.
#

MYDIR=`dirname "$0"`
FPS=21.675
ASAMPLERATE=15606
filename=`basename "$1"`
newfilename=${filename%.*}.vid

rm -f $2/video.bin

# Convert video to raw rgb565 pixel frames at $FPS frames/sec
mkdir $2/vidtmp
ffmpeg -i "$1" -r $FPS -s 320x240 -q:v 7 $2/vidtmp/vidout%08d.jpg

# Extract audio in stereo
ffmpeg -i "$1" -ac 2 -ar $ASAMPLERATE $2/sample.wav

# Convert WAV to 2's complement signed 16 bit samples
sox $2/sample.wav $2/sample.s16 channels 2 rate $ASAMPLERATE loudness 12

# Now merge the video and audio into a single file
${MYDIR}/../bin/videomerge $2/vidtmp $2/sample.s16 $2/"${newfilename}"

rm -fr $2/vidtmp $2/sample.wav $2/sample.s16
