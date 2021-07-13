#!/bin/sh
#

test_description='IMP casign functionality test

Ensure IMP casign works as designed.
'

# Append --logfile option if FLUX_TESTS_LOGFILE is set in environment:
test -n "$FLUX_TESTS_LOGFILE" && set -- "$@" --logfile
. `dirname $0`/sharness.sh

flux_imp=${SHARNESS_BUILD_DIRECTORY}/src/imp/flux-imp
keygen=${SHARNESS_BUILD_DIRECTORY}/t/src/keygen
certutil=${SHARNESS_BUILD_DIRECTORY}/t/src/certutil
ca=${SHARNESS_BUILD_DIRECTORY}/t/src/ca
cf=${SHARNESS_BUILD_DIRECTORY}/t/src/cf

# used by 'ca' utility as well as imp
export FLUX_IMP_CONFIG_PATTERN=${SHARNESS_TRASH_DIRECTORY}/conf.d/*.toml
mkdir -p conf.d
echo 'allow-sudo = true' >${SHARNESS_TRASH_DIRECTORY}/conf.d/imp.toml

new_ca_config() {
	local max_cert_ttl=$1
	local max_sign_ttl=$2

	cat >conf.d/ca.toml <<-EOT
	[ca]
	max-cert-ttl = ${max_cert_ttl}
	max-sign-ttl = ${max_sign_ttl}

	cert-path = "${SHARNESS_TRASH_DIRECTORY}/ca"
	revoke-dir = "${SHARNESS_TRASH_DIRECTORY}/revoke.d"
	revoke-allow = true
	domain = "EXAMPLE.TEST"
	EOT
}

test_expect_success 'create new CA config' '
	new_ca_config 60 30 &&
	$cf ca.max-cert-ttl &&
	$cf ca.max-sign-ttl &&
	$cf ca.cert-path &&
	$cf ca.revoke-dir &&
	$cf ca.revoke-allow
'

test_expect_success 'create new CA cert' '
	$ca keygen &&
	test -f ca &&
	test -f ca.pub
'

test_expect_success 'create new user signing cert' '
	$keygen u &&
	test -f u &&
	test -f u.pub
'

test_expect_success SUDO 'imp casign works under sudo' '
	$SUDO FLUX_IMP_CONFIG_PATTERN=${SHARNESS_TRASH_DIRECTORY}/conf.d/*.toml \
		$flux_imp casign <u.pub >u.pub.signed.sudo
'

test_expect_success 'imp can sign user cert' '
	$flux_imp casign <u.pub >u.pub.signed &&
	mv u.pub u.pub.unsigned &&
	mv u.pub.signed u.pub
'

test_expect_success 'user cert has expected userid' '
	$certutil u get userid i >userid.out &&
	id -u >userid.exp &&
	test_cmp userid.exp userid.out
'

test_expect_success 'user cert has a uuid' '
	$certutil u get uuid s
'

test_expect_success 'user cert has ctime <= now' '
	ctime=$($certutil u get ctime t) &&
	test $ctime -le $(date +%s)
'

test_expect_success 'user cert xtime - ctime = CA max-cert-ttl' '
	max_cert_ttl=$($cf ca.max-cert-ttl) &&
	ctime=$($certutil u get ctime t) &&
	xtime=$($certutil u get xtime t) &&
	test $(($xtime-$ctime)) -eq ${max_cert_ttl}
'

test_expect_success 'user cert max-sign-ttl = CA max-sign-ttl' '
	max_sign_ttl=$($cf ca.max-sign-ttl) &&
	val=$($certutil u get max-sign-ttl i) &&
	test $val -eq ${max_sign_ttl}
'

test_expect_success 'CA verifies signed cert' '
	$ca verify u
'

test_expect_success 'CA revokes cert' '
	uuid=$($certutil u get uuid s) &&
	$ca revoke $uuid
'

test_expect_success 'CA cannot verify revoked cert' '
	test_must_fail $ca verify u
'

test_expect_success NO_ASAN 'imp casign fails on /dev/zero input' '
	test_must_fail $flux_imp casign </dev/zero
'

test_expect_success 'imp casign fails on /dev/null input' '
	test_must_fail $flux_imp casign </dev/null
'

test_done
