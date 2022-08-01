#!/bin/sh
#

test_description='IMP exec PAM functionality test'

# Append --logfile option if FLUX_TESTS_LOGFILE is set in environment:
test -n "$FLUX_TESTS_LOGFILE" && set -- "$@" --logfile
. `dirname $0`/sharness.sh

flux_imp=${SHARNESS_BUILD_DIRECTORY}/src/imp/flux-imp
sign=${SHARNESS_BUILD_DIRECTORY}/t/src/sign

echo "# Using ${flux_imp}"

$flux_imp version > version.out
if ! grep -q pam version.out; then
	skip_all='Skipping PAM tests. IMP not built with PAM support'
	test_done
fi

#  Check for libpam_wrapper.so
LD_PRELOAD=libpam_wrapper.so $flux_imp version >ld_preload.out 2>&1
if grep -i error ld_preload.out >/dev/null 2>&1; then
	skip_all='libpam_wrapper.so not found. Skipping all tests'
	test_done
fi

fake_imp_input() {
	printf '{"J":"%s"}' $(echo $1 | $sign)
}


test_expect_success 'create config for flux-imp exec and signer' '
	cat >sign-none.toml <<-EOF &&
	allow-sudo = true
	[sign]
	max-ttl = 30
	default-type = "none"
	allowed-types = [ "none" ]
	[exec]
	allowed-users = [ "$(whoami)" ]
	allowed-shells = [ "bash", "echo" ]
	allow-unprivileged-exec = true
	EOF
	export FLUX_IMP_CONFIG_PATTERN="$(pwd)"/sign-none.toml
'
test_expect_success 'create test pam.d/flux and limits.conf' '
	mkdir pam.d &&
	cat >pam.d/flux <<-EOF &&
	auth    required pam_localuser.so
	session required pam_limits.so conf=$(pwd)/limits.conf
	EOF
	cat >limits.conf <<-EOF
	*     soft    nofile    543
	EOF
'

test_expect_success 'create IMP test script for PAM testing' '
	cat >imp-pam.sh <<-EOF &&
	#!/bin/sh
	export PAM_WRAPPER=1
	export LD_PRELOAD=libpam_wrapper.so
	export PAM_WRAPPER_SERVICE_DIR="$(pwd)"/pam.d
	export FLUX_IMP_CONFIG_PATTERN="$(pwd)"/sign-none.toml
	exec $flux_imp "\$@"
	EOF
	chmod +x imp-pam.sh
'
test_expect_success SUDO 'flux-imp exec invokes PAM stack' '
	fake_imp_input foo | \
	    $SUDO ./imp-pam.sh exec bash -c "ulimit -n" >ulimit.out &&
	test_debug "cat ulimit.out" &&
	test "$(cat ulimit.out)" = "543"
'
test_done
