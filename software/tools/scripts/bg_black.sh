#!/bin/sh
# Find the color at pixel 0,0 and replace it with black.
# This assumes the pixel at 0,0 is part of the background.
set -x

BG=`magick $1 -quiet -format '%[pixel:p{0,0}]' info:-`
convert $1 -quiet -fuzz 7% -fill black -opaque $BG $2
