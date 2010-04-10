#!/bin/sh

touch ltconfig
libtoolize --force --copy --automake
aclocal
autoconf
autoheader
automake --add-missing
