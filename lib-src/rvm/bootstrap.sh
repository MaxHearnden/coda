#!/bin/sh
#
# Running this should create the following files:
#
# touch      - AUTHORS ChangeLog README
# autoheader - config.h.in
# aclocal    - aclocal.m4
# automake   - COPYING INSTALL Makefile.in config.guess config.sub install-sh
#              libtool ltconfig ltmain.sh missing mkinstalldirs stamp-h.in
# autoconf   - configure
#

rm -rf autom4te.cache
touch AUTHORS ChangeLog README
autoreconf --verbose --install --force
#autoheader
#aclocal
#libtoolize --copy --force
#automake --copy --add-missing
#autoconf
