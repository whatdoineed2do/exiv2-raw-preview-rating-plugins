#!/bin/sh

mkdir m4 2>/dev/null
autoreconf --install
autoconf
echo "run configure"
