#!/bin/sh
#

test_description='sign-munge tests

Start munge daemon if available and test basic functionality
of sign-munge mechanism.
'

# Append --logfile option if FLUX_TESTS_LOGFILE is set in environment:
test -n "$FLUX_TESTS_LOGFILE" && set -- "$@" --logfile
. `dirname $0`/sharness.sh

# If system munge is configured and running, use it.
# If not, attempt to start munged on the side using functions defined
# in sharness.d/03-munge.sh.  If that doesn't work, skip the whole test.
if munge </dev/null | unmunge >/dev/null; then
	echo "System munge works"
	export MUNGE_SOCKET=$(munged --help \
	                      | sed -n '/-S, --socket/s/.*\[\(.*\)\]$/\1/p')
elif munged --version && (munged --help | grep pid-file); then
	test_set_prereq SIDEMUNGE
	export MUNGED=munged  # needed by 03-munge.sh
	export MUNGE_SOCKET   # set by 03-munge.sh, used below and xsign_munge
else
	skip_all="Could not find working munge.  Skipping all tests"
	test_done
fi

sign=${SHARNESS_BUILD_DIRECTORY}/t/src/sign
xsign=${SHARNESS_BUILD_DIRECTORY}/t/src/xsign_munge
verify=${SHARNESS_BUILD_DIRECTORY}/t/src/verify
export FLUX_IMP_CONFIG_PATTERN=${SHARNESS_TRASH_DIRECTORY}/sign.toml


test_expect_success SIDEMUNGE 'start munged' '
	munged_start_daemon
'

test_expect_success 'munge works' '
	munge --socket ${MUNGE_SOCKET} </dev/null \
		| unmunge --socket ${MUNGE_SOCKET} >/dev/null
'

test_expect_success 'create sign.toml' '
	cat >sign.toml <<-EOT
	[sign]
	max-ttl = 60
	default-type = "munge"
	allowed-types = [ "munge" ]
	[sign.munge]
	socket-path = "${MUNGE_SOCKET}"
	EOT
'

test_expect_success 'sign/verify zero length payload' '
	cat /dev/null >zsign.in &&
	${sign} <zsign.in >zsign.out &&
	${verify} <zsign.out >zverify.out &&
	test_cmp zsign.in zverify.out
'

test_expect_success 'sign/verify a short message' '
	echo Hello >sign.in &&
	${sign} <sign.in >sign.out &&
	${verify} <sign.out >verify.out &&
	test_cmp sign.in verify.out
'

test_expect_success 'verify a hand-created test message' '
	${xsign} good </dev/null >good.out &&
	${verify} <good.out
'

test_expect_success 'message with wrong userid fails verify' '
	${xsign} xuser </dev/null >xuser.out &&
	test_must_fail ${verify} <xuser.out 2>xuser.err &&
	grep -q "uid mismatch" xuser.err
'

test_expect_success 'message with unknown hash type fails verify' '
	${xsign} xhashtype </dev/null >xhashtype.out &&
	test_must_fail ${verify} <xhashtype.out 2>xhashtype.err &&
	grep -q "unknown hash type" xhashtype.err
'

test_expect_success 'message with truncated hash fails verify' '
	${xsign} xhashtrunc </dev/null >xhashtrunc.out &&
	test_must_fail ${verify} <xhashtrunc.out 2>xhashtrunc.err &&
	grep -q "hash mismatch" xhashtrunc.err
'

test_expect_success 'message with altered hash fails verify' '
	${xsign} xhashchg </dev/null >xhashchg.out &&
	test_must_fail ${verify} <xhashchg.out 2>xhashchg.err &&
	grep -q "hash mismatch" xhashchg.err
'

test_expect_success 'message with altered payload fails verify' '
	${xsign} xpaychg </dev/null >xpaychg.out &&
	test_must_fail ${verify} <xpaychg.out 2>xpaychg.err &&
	grep -q "hash mismatch" xpaychg.err
'

test_expect_success 'message with altered munge cred fails verify' '
	${xsign} xcredchg </dev/null >xcredchg.out &&
	test_must_fail ${verify} <xcredchg.out 2>xcredchg.err &&
	grep -q "munge_decode" xcredchg.err
'

# N.B. max-ttl = (exactly) -100 is allowed for testing
test_expect_success 'create sign.toml with max-ttl=-100' '
	cat >sign.toml <<-EOT
	[sign]
	max-ttl = -100
	default-type = "munge"
	allowed-types = [ "munge" ]
	[sign.munge]
	socket-path = "${MUNGE_SOCKET}"
	EOT
'

test_expect_success 'message with expired TTL fails verify' '
	cat /dev/null >zttl.in &&
	${sign} <zttl.in >zttl.out &&
	test_must_fail ${verify} <zttl.out 2>zttl.err &&
	grep -q ttl zttl.err
'

test_expect_success 'create sign.toml with bogus entry' '
	cat >sign.toml <<-EOT
	[sign]
	max-ttl = 60
	default-type = "munge"
	allowed-types = [ "munge" ]
	[sign.munge]
	socket-path = "${MUNGE_SOCKET}"
	bogus = 42
	EOT
'

test_expect_success 'init fails with bad [sign.munge] config' '
	test_must_fail ${sign} </dev/null 2>bogussign.err &&
	grep -q bogus bogussign.err
'

test_expect_success 'create sign.toml with none default-type' '
	cat >sign.toml <<-EOT
	[sign]
	max-ttl = 60
	default-type = "none"
	allowed-types = [ "munge" ]
	[sign.munge]
	socket-path = "${MUNGE_SOCKET}"
	EOT
'

test_expect_success 'message encoded with mechanism != munge fails' '
	cat /dev/null >sign_none.in &&
        ${sign} <sign_none.in >sign_none.out &&
	test_must_fail ${verify} <sign_none.out 2>sign_none.err &&
	test_debug cat sign_none.err &&
	grep -q "mechanism=none not allowed" sign_none.err
'

test_expect_success SIDEMUNGE 'stop munged' '
	munged_stop_daemon
'

test_done
