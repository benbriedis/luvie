#! /bin/bash

#TODO --branch arg. Works on tags too...

# LV2
git clone --depth 1 https://github.com/lv2/lv2.git sources/lv2

# Lilv + its dependencies
git clone --depth 1 https://github.com/lv2/lilv.git sources/lilv
git clone --depth 1 https://github.com/drobilla/serd.git sources/serd
git clone --depth 1 https://github.com/drobilla/sord.git sources/sord

# Fltk
git clone --depth 1 https://github.com/fltk/fltk.git sources/fltk
