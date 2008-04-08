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

AC_CHECK_BASENAME

if AC_CHECK_HEADERS basis/options.h && LIBS="-lbasis" AC_CHECK_FUNCS x_getopt; then
    AC_LIBS="$AC_LIBS -lbasis"
    AC_SUB XGETOPT ''
    echo "#include <basis/getopts.h>" >> $__cwd/config.h
else
    AC_SUB XGETOPT options.o
    echo '#include "basis/getopts.h"' >> $__cwd/config.h
fi

LOGN "Looking for <regex.h>"
if AC_QUIET AC_CHECK_HEADERS regex.h; then
    LOG " ok"
elif AC_QUIET AC_CHECK_HEADERS sys/types.h regex.h; then
    LOG " (requires <sys/types.h>)"
    echo "#include <sys/types.h>" >> $__cwd/config.h
    sys_types_needed=1
else
    LOG " (not found)"
    AC_FAIL "Cannot build secho without <regex.h>"
fi

( 
test "$sys_types_needed" && echo "#include <sys/types.h>"
cat - << EOF
#include <regex.h>

main()
{
    int i = REG_BASIC;
}
EOF
) > /tmp/ngc$$.c

LOGN "Is REG_BASIC defined in regex.h? "
if $AC_CC -c -o /tmp/ngc$$ /tmp/ngc$$.c $LIBS; then
    LOG " (yes)"
else
    LOG " (no)"
    AC_DEFINE	REG_BASIC	0
fi
rm -f /tmp/ngc$$ /tmp/ngc$$.c

AC_OUTPUT Makefile
