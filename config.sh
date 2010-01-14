##
## Midnight commander testfarm config
##

## Directory where the master repository is held
TESTFARM_REPO_DIR=___repo.git

## Parent directory of the build dirs
TESTFARM_BUILD_ROOT=`pwd`/__build

## --prefix parameter
TESTFARM_CF_PREFIX=~/.usr/mc-testing.$ID

## (temporary) local branchname for checkout
TESTFARM_LOCALBRANCH="__testing"

## Output directory for build logs
TESTFARM_OUTPUT_DIR=`pwd`/out/$ID

## Where the option files come from
TESTFARM_OPT_DIR=`pwd`/opts

## Root directory for DESTDIR install test
TESTFARM_DESTDIR_ROOT=`pwd`/__install

## Set to "yes" if logs of succeeded builds should be removed automatically
TESTFARM_REMOVE_SUCCESS_LOGS="yes"

## local configuration
if [ "$TESTFARM_CFLAGS" ]; then
    export CFLAGS="$TESTFARM_CFLAGS"
else
    export CFLAGS="$CFLAGS -O -Wwrite-strings -Wempty-body"
#    export CFLAGS="$CFLAGS -O -Wwrite-strings -Wempty-body -Werror"
fi
