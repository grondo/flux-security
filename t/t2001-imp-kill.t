#!/bin/sh
#

test_description='IMP kill basic functionality test

Basic flux-imp kill failure handling
'

# Append --logfile option if FLUX_TESTS_LOGFILE is set in environment:
test -n "$FLUX_TESTS_LOGFILE" && set -- "$@" --logfile
. `dirname $0`/sharness.sh

flux_imp=${SHARNESS_BUILD_DIRECTORY}/src/imp/flux-imp

echo "# Using ${flux_imp}"

test_expect_success 'flux-imp kill: returns error when run with no args' '
	test_must_fail $flux_imp kill
'
test_expect_success 'flux-imp kill: returns error when run with 1 arg' '
	test_must_fail $flux_imp kill 15
'
test_expect_success 'flux-imp kill: returns error with pid=0' '
	test_must_fail $flux_imp kill 15 0
'
test_expect_success 'flux-imp kill: returns error with invalid signal' '
	test_must_fail $flux_imp kill -15 0
'
test_expect_success 'flux-imp kill: checks exec.allowed-users' '
	name=wronguser &&
	cat <<-EOF >${name}.toml &&
	[exec]
	allowed-users = []
	EOF
	( export FLUX_IMP_CONFIG_PATTERN=${name}.toml &&
	  test_must_fail $flux_imp kill 15 1234 >${name}.log 2>&1
	) &&
	test_debug "cat ${name}.log" &&
	grep "kill command not allowed" ${name}.log && unset name
'
test_expect_success SUDO 'flux-imp kill: sudo: user must be in allowed-shells' '
	name=wrongusersudo &&
	cat <<-EOF >${name}.toml &&
	allow-sudo = true
	[exec]
	allowed-users = []
	EOF
	test_must_fail $SUDO FLUX_IMP_CONFIG_PATTERN=${name}.toml \
	      $flux_imp kill 15 1234 >${name}.log 2>&1 &&
	test_debug "cat ${name}.log" &&
	grep -i "kill command not allowed" ${name}.log && unset name
'

test "$chain_lint" = "t" || test_set_prereq NO_CHAIN_LINT
test -d /sys/fs/cgroup/systemd && test_set_prereq SYSTEMD_CGROUP

test_expect_success NO_CHAIN_LINT,SYSTEMD_CGROUP 'flux-imp kill: works in unpriv mode' '
	name=allowed-user
	cat <<-EOF >${name}.toml
	allow-sudo = true
	[exec]
	allowed-users = [ "$(whoami)" ]
	EOF
	sleep 300 & pid=$! &&
	FLUX_IMP_CONFIG_PATTERN=${name}.toml \
	    $flux_imp kill 15 $pid >${name}.log 2>&1 &&
	test_debug "cat ${name}.log" &&
	test_expect_code 143 wait $pid
'
test_expect_success NO_CHAIN_LINT,SYSTEMD_CGROUP,SUDO 'flux-imp kill: works with sudo' '
	name=allowed-user
	cat <<-EOF >${name}.toml
	allow-sudo = true
	[exec]
	allowed-users = [ "$(whoami)" ]
	EOF
	sleep 300 & pid=$! &&
	$SUDO FLUX_IMP_CONFIG_PATTERN=${name}.toml \
	    $flux_imp kill 15 $pid >${name}.log 2>&1 &&
	test_expect_code 143 wait $pid
'
test_expect_success SYSTEMD_CGROUP 'flux-imp kill: fails for nonexistent pid' '
	name=allowed-user &&
	cat <<-EOF >${name}.toml &&
	allow-sudo = true
	[exec]
	allowed-users = [ "$(whoami)" ]
	EOF
	for pid in `seq 10000 12000`; do
            kill -s 0 ${pid} >/dev/null 2>&1 || break
        done &&
	( export FLUX_IMP_CONFIG_PATTERN=${name}.toml &&
	    test_must_fail $flux_imp kill 15 $pid >${name}.log 2>&1
	) &&
	test_debug "cat ${name}.log" &&
	grep "No such file or directory"  ${name}.log
'
test_expect_success SUDO,SYSTEMD_CGROUP 'flux-imp kill: fails for nonexistent pid under sudo' '
	name=allowed-user &&
	cat <<-EOF >${name}.toml &&
	allow-sudo = true
	[exec]
	allowed-users = [ "$(whoami)" ]
	EOF
	for pid in `seq 10000 12000`; do
            kill -s 0 ${pid} >/dev/null 2>&1 || break
        done &&
	test_must_fail $SUDO FLUX_IMP_CONFIG_PATTERN=${name}.toml \
	    $flux_imp kill 15 $pid >${name}.log 2>&1 &&
	test_debug "cat ${name}.log" &&
	grep "No such file or directory"  ${name}.log
'

test_done
