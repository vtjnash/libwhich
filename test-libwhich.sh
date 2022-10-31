#!/bin/sh
# simple shell (bash) script to test the output of libwhich
# Required tools:
#  grep
#  xargs
#  stat
#  GNU sed (normal sed doesn't support escape sequences)
#  libz (aka zlib1) shared library (or a stub built from `make libz.so`)

# This can be run in emulation environments by setting the appropriate flags
# make check TARGET=WINNT CC=x86_64-w64-mingw32-gcc \
#            STAT="winepath -u" LIBWHICH="wine ./libwhich.exe"


if [ -z "$TARGET" ]; then
TARGET=`uname`
fi
if [ "$TARGET" = "Darwin" ]; then
SHEXT=dylib
else
SHEXT=so
fi
if [ -z "$SED" ]; then
SED=sed
fi
if [ -z "$GREP" ]; then
GREP=grep
fi
if [ -z "$XARGS" ]; then
XARGS=xargs
fi
if [ -z "$STAT" ]; then
STAT=stat
fi
if [ -z "$LIBWHICH" ]; then
LIBWHICH=./libwhich
fi


S=`$SED --version 2> /dev/null | $GREP "GNU sed"`
if [ $? -ne 0 ] || [ -z "$S" ]; then
    echo 'ERROR: need to point the `SED` environment variable a version of GNU sed (`gsed`)'
    exit 1
fi

# print out the script for error reporting
set -v
# set pipefail, if possible (running in bash), for additional error detection
(set -o pipefail) 2>/dev/null && set -o pipefail

dotest() { "$@"; }
echo() { `which echo` "$@"; }

## tests for failures ##
S=`dotest $LIBWHICH`
[ $? -eq 1 ] || exit 1
echo RESULT: $S
[ "$S" = "expected 1 argument specifying library to open (got 0)" ] || exit 1

S=`dotest $LIBWHICH '' 2`
[ $? -eq 1 ] || exit 1
echo RESULT: $S
[ "$S" = "expected first argument to specify an output option:
  -p  library path
  -a  all dependencies
  -d  direct dependencies" ] || exit 1

S=`dotest $LIBWHICH 1 2\ 3 '4 5'`
[ $? -eq 1 ] || exit 1
echo RESULT: $S
[ "$S" = "expected 1 argument specifying library to open (got 3)" ] || exit 1

S=`dotest $LIBWHICH not_a_library`
[ $? -eq 1 ] || exit 1
echo RESULT: $S
S1=`echo $S | $SED -e 's!^failed to open library: .*\<not_a_library\>.*$!ok!'`
echo "$S1"
[ "$S1" = "ok" ] || exit 1

## tests for successes ##
set -e # exit on error

### tests for script usages ###

dotest $LIBWHICH -a libz.$SHEXT | $XARGS -0 echo > /dev/null # make sure the files are valid (suppress stdout)

S=`dotest $LIBWHICH -p libz.$SHEXT`
echo RESULT: $S
[ -n "$S" ] || exit 1

S=$(dotest $LIBWHICH -a libz.$SHEXT | $GREP -aF "$S") # make sure -p appears in the -a list
echo RESULT: $S
[ -n "$S" ] || exit 1

S=`dotest $LIBWHICH -a libz.$SHEXT | $SED -e 's/^[^\\/]*\x00//' | $SED -e 's/\x00.*//'` # get an existing (the first real) shared library path
echo RESULT: $S
[ -n "$S" ] || exit 1
if [ "$TARGET" = "Darwin" ]; then
[ "$S" = "/usr/lib/libSystem.B.dylib" ] || exit 1
else
$STAT "$S"
fi
LIB=$S # save it for later usage

S=`dotest $LIBWHICH -p "$LIB"` # test for identity
echo RESULT: $S
[ "$S" = "$LIB" ] || exit 1

S=`dotest $LIBWHICH -a "$LIB" | ${XARGS} -0 echo` # use xargs echo to turn \0 into spaces
echo RESULT: "$S"
[ -n "$S" ] || exit 1

S=`dotest $LIBWHICH -d "$LIB" | ${XARGS} -0 echo` # check that it didn't load anything else
echo RESULT: $S
[ -z "$S" ] || exit 1

### tests for command line ###

S=`dotest $LIBWHICH "$LIB"`
echo RESULT: $S
S1a=`! echo "$S" | $GREP '^+'` # see that we didn't get a line starting with +
[ $? -eq 0 ] || exit 1
[ -z "$S1a" ] || exit 1
if [ "$TARGET" = "WINNT" ]; then
S1b=`echo "$S" | $GREP '^  [A-Za-z]:\\\\'`
else
S1b=`echo "$S" | $GREP '^  /'`
fi
[ -n "$S1b" ] || exit 1 # see that we did get an absolute path
if [ "$TARGET" = "WINNT" ]; then
S2=`echo $S | $SED -e 's!^library: [A-Za-z]:\\\\[^ ]\+ dependencies: [A-Za-z]:\\\\.*!ok!'`
else
S2=`echo $S | $SED -e 's!^library: [^ ]\+ dependencies: .*$!ok!'`
fi
[ "$S2" = "ok" ] || exit 1 # see that the general format is as expected

S=`dotest $LIBWHICH libz.$SHEXT`
echo RESULT: $S
S1a=`echo "$S" | $GREP '^+'`
if [ "$TARGET" = "WINNT" ]; then
S1b=`echo "$S" | $GREP '^+ [A-Za-z]:\\\\.*\(libz\|msvcrt\).*'`
else
S1b=`echo "$S" | $GREP '^+ /.*libz.*'`
fi
[ -n "$S1a" ] || exit 1 # see that we did get a path starting with +,
[ "$S1a" = "$S1b" ] || exit 1 # and that it's an absolute path to libz (or dependency)
if [ "$TARGET" = "WINNT" ]; then
S2=`echo $S | $SED -e 's!^library: [A-Za-z]:\\\\[^ ]*libz[^ ]* dependencies: [A-Za-z]:\\\\.*libz.*$!ok!'`
else
S2=`echo $S | $SED -e 's!^library: /[^ ]*libz[^ ]* dependencies: .*libz.*$!ok!'`
fi
[ "$S2" = "ok" ] || exit 1 # see the general format is as expected

## finished successfully ##
echo "SUCCESS"
