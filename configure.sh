#! /bin/sh

# local options:  ac_help is the help message that describes them
# and LOCAL_AC_OPTIONS is the script that interprets them.  LOCAL_AC_OPTIONS
# is a script that's processed with eval, so you need to be very careful to
# make certain that what you quote is what you want to quote.

TARGET=secho
. ./configure.inc

AC_INIT $TARGET

AC_PROG_CC

case "$AC_CC $AC_CFLAGS" in
*-Wall*)    AC_DEFINE 'while(x)' 'while( (x) != 0 )'
	    AC_DEFINE 'if(x)' 'if( (x) != 0 )' ;;
esac

if AC_CHECK_BASENAME; then
    AC_DEFINE HAVE_BASENAME 1
    if AC_CHECK_HEADERS libgen.h; then
	echo "#include <libgen.h>" >> $__cwd/config.h
    fi
fi
AC_CHECK_HEADERS regex.h || AC_FAIL "Cannot build secho without <regex.h>"

echo > /tmp/ngc$$.c << EOF
#include <regex.h>

main()
{
    int i = REG_BASIC;
}
EOF

LOGN "Is REG_BASIC defined in regex.h? "
if $AC_CC -c -o /tmp/ngc$$ /tmp/ngc$$.c $LIBS; then
    LOG " (yes)"
else
    LOG " (no)"
    AC_DEFINE	REG_BASIC	0
fi
rm -f /tmp/ngc$$ /tmp/ngc$$.c

AC_OUTPUT Makefile
