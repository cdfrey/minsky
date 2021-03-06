#!/bin/bash
set -e
git submodule update --init --recursive

make -j2 DEBUG=1
export TRAVIS=1
xvfb-run make -j2 AEGIS=1 sure
# currently failing on Travis due to OpenSUSE Tumbleweed bug. Ticket #1170
make doc
