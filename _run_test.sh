
## these must be set by the test scripts
[ "$TESTS" ]                || ( echo "Missing TESTS"  ; exit 2 )
[ "$BRANCH" ]               || ( echo "Missing BRANCH" ; exit 3 )
[ "$ID" ]                   || ( echo "Missing ID"     ; exit 4 )

## load global configuration
. ./config.sh               || ( echo "Cannot load config.sh"        ; exit 1 )
[ "$TESTFARM_CF_PREFIX" ]   || ( echo "Missing TESTFARM_CF_PREFIX"   ; exit 5 )
[ "$TESTFARM_LOCALBRANCH" ] || ( echo "Missing TESTFARM_LOCALBRANCH" ; exit 6 )
[ "$TESTFARM_OUTPUT_DIR" ]  || ( echo "Missing TESTFARM_OUTPUT_DIR"  ; exit 7 )
[ "$TESTFARM_OPT_DIR" ]     || ( echo "Missing TESTFARM_OPT_DIR"     ; exit 8 )
[ "$TESTFARM_BUILD_ROOT" ]  || ( echo "Missing TESTFARM_BUILD_ROOT"  ; exit 9 )

BUILDDIR="$TESTFARM_BUILD_ROOT/$ID.git"

function prepare_builddir() {

    mkdir -p $TESTFARM_BUILD_ROOT
    mkdir -p $TESTFARM_OUTPUT_DIR

    ## checkout
    rm -Rf $BUILDDIR
    if ! cp -R $TESTFARM_REPO_DIR $BUILDDIR ; then
	echo "Copying repo from $TESTFARM_REPO_DIR to $BUILDDIR failed"
	exit 23
    fi

    if ! cd $BUILDDIR ; then
	echo "Cannot change to $BUILDDIR"
	exit 24
    fi

    if ! (git checkout -f $BRANCH -b $TESTFARM_LOCALBRANCH 2>&1 ) | \
	    (grep -v "Branch $TESTFARM_LOCALBRANCH set up to track remote branch" || true) |
	    (grep -v "Switched to a new branch '$TESTFARM_LOCALBRANCH'" || true) ; then
	echo "Checkout failed"
	exit 25
    fi

    ## repare the build files
    if ! (./autogen.sh 2>&1 ) > $TESTFARM_OUTPUT_DIR/autogen.out ; then
	echo "Autogen stage failed"
	exit 26
    else
	if [ "$TESTFARM_REMOVE_SUCCESS_LOGS" == "yes" ]; then
	    rm -f $TESTFARM_OUTPUT_DIR/autogen.out
	fi
    fi
}

function run_test() {
    T_NAME="$1"

    if [ -f "$TESTFARM_OUTPUT_DIR/$T_NAME.OK" ]; then
	echo "Skipping succeed test: $T_NAME"
	return
    fi

    echo "Running test: $T_NAME"

    if ! cd $BUILDDIR ; then
	echo "Cannot change to $BUILDDIR"
	exit 24
    fi

    echo "    [$T_NAME] Distclean"
    ( make distclean 2>&1 ) > $TESTFARM_OUTPUT_DIR/$T_NAME.distclean.out
    rm -f $TESTFARM_OUTPUT_DIR/$T_NAME.distclean.out

    echo "    [$T_NAME] Configure"
    if ! ( ./configure --prefix=$TESTFARM_CF_PREFIX `cat $TESTFARM_OPT_DIR/$t.opt` 2>&1 ) > $TESTFARM_OUTPUT_DIR/$t.configure.out ; then
	echo "==> ERROR: Configure failed for test $T_NAME"
	return
    fi

    echo "    [$t] Build"
    if ! ( make 2>&1 ) > $TESTFARM_OUTPUT_DIR/$T_NAME.build.out ; then
	echo "==> ERROR: Build failed for test $T_NAME"
	return
    fi

    echo "    [$t] Install"
    if ! ( DESTDIR="$TESTFARM_DESTDIR_ROOT/$ID/" make install 2>&1 ) > $TESTFARM_OUTPUT_DIR/$t.install.out ; then
	echo "==> ERROR: Install failed for test $T_NAME"
	return
    fi

    touch $TESTFARM_OUTPUT_DIR/$T_NAME.OK

    if [ "$TESTFARM_REMOVE_SUCCESS_LOGS" == "yes" ]; then
	rm -f 	\
		$TESTFARM_OUTPUT_DIR/$T_NAME.build.out 		\
		$TESTFARM_OUTPUT_DIR/$T_NAME.configure.out	\
		$TESTFARM_OUTPUT_DIR/$T_NAME.distclean.out	\
		$TESTFARM_OUTPUT_DIR/$T_NAME.install.out
    fi
}


prepare_builddir

for t in $TESTS ; do
    run_test $t
done
