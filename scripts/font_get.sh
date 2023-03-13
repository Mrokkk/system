#!/usr/bin/env bash

base_dir=$(dirname $0)
if [ ! -f font.psf ] || [ "${base_dir}/font_get.sh" -nt "font.psf" ]; then
    rm -f font.psf
    cp /usr/share/kbd/consolefonts/Lat2-Terminus16.psfu.gz font.psf.gz
    gzip -d font.psf.gz
fi
