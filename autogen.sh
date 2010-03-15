#!/bin/sh

touch ltconfig
libtoolize --force --copy --automake
aclocal
autoconf
automake --add-missing
