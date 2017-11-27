#!/bin/sh
# simple shell (bash) script to test the output of libwhich
# Required tools:
#  grep
#  xargs
#  stat
#  GNU sed
#  libz (zlib1) shared library

if [ "`uname`" = "Darwin" ]; then
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

S=`$SED --version 2> /dev/null | $GREP "GNU sed"`
if [ $? -ne 0 ] || [ -z "$S" ]; then
    echo 'ERROR: need to point the `SED` environment variable a version of GNU sed (`gsed`)'
    exit 1
fi

# print out the script for error reporting
set -v

## tests for failures ##
S=`./libwhich`
[ $? -eq 1 ] || exit 1
echo RESULT: $S
[ "$S" = "expected 1 argument specifying library to open (got 0)" ] || exit 1

S=`./libwhich '' 2`
[ $? -eq 1 ] || exit 1
echo RESULT: $S
[ "$S" = "expected first argument to specify an output option:
  -p  library path
  -a  all dependencies
  -d  direct dependencies" ] || exit 1

S=`./libwhich 1 2 3`
[ $? -eq 1 ] || exit 1
echo RESULT: $S
[ "$S" = "expected 1 argument specifying library to open (got 3)" ] || exit 1

S=`./libwhich not_a_library`
[ $? -eq 1 ] || exit 1
echo RESULT: $S
S1=`echo $S | $SED -e 's!^failed to open library: .*\<not_a_library\>.*$!ok!'`
echo "$S1"
[ "$S1" = "ok" ] || exit 1

## tests for successes ##
set -e # exit on error

### tests for script usages ###

./libwhich -a libz.$SHEXT | xargs -0 stat > /dev/null # make sure the files are valid (suppress stdout)
S=$(./libwhich -a libz.$SHEXT | $GREP -a `./libwhich -p libz.$SHEXT`) # make sure -p appears in the -a list
echo RESULT: $S

S=`./libwhich -a libz.$SHEXT | $SED -e 's/\x00.*//'` # get an existing shared library path
echo RESULT: $S
[ -n "$S" ] || exit 1
stat "$S"
LIB=$S # save it for later usage

S=`./libwhich -p $LIB` # test for identity
echo RESULT: $S
[ "$S" = "$LIB" ] || exit 1

S=`./libwhich -a $LIB | xargs -0 echo` # use xargs echo to turn \0 into spaces
echo RESULT: "$S"
[ -n "$S" ] || exit 1

S=`./libwhich -d $LIB | xargs -0 echo` # check that it didn't load anything else
echo RESULT: $S
[ -z "$S" ] || exit 1

### tests for command line ###

S=`./libwhich $LIB`
echo RESULT: $S
set +e
S1=`echo "$S" | $GREP '^+'`
[ $? -eq 1 ] || exit 1
[ -z "$S1" ] || exit 1
set -e
S2=`echo $S | $SED -e 's!^library: /[^ ]\+ dependencies: /.*$!ok!'`
[ "$S2" = "ok" ] || exit 1

S=`./libwhich libz.$SHEXT`
echo RESULT: $S
S1a=`echo "$S" | $GREP '^+'`
S1b=`echo "$S" | $GREP '^+ /.*libz.*'`
[ -n "$S1a" ] || exit 1
[ "$S1a" = "$S1b" ] || exit 1
S2=`echo $S | $SED -e 's!^library: /[^ ]*libz[^ ]* dependencies: /.*libz.*$!ok!'`
[ "$S2" = "ok" ] || exit 1

## finished successfully ##
echo "SUCCESS"
