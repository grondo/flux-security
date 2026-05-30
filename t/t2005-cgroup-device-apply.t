#!/bin/sh
#

test_description='cgroup-device-apply tests

Tests for the cgroup-device-apply External Containment Helper (RFC 15).
Unprivileged tests cover argument validation and no-op paths.
Privileged tests confirm end-to-end BPF attachment.
'

# Append --logfile option if FLUX_TESTS_LOGFILE is set in environment:
test -n "$FLUX_TESTS_LOGFILE" && set -- "$@" --logfile
. `dirname $0`/sharness.sh

cgroup_device_apply=${SHARNESS_BUILD_DIRECTORY}/src/imp/cgroup-device-apply
bpf_cgroup_probe=${SHARNESS_BUILD_DIRECTORY}/t/src/bpf_cgroup_probe

echo "# Using ${cgroup_device_apply}"

CGROUP_MOUNT=$(awk '$3 == "cgroup2" {print $2}' /proc/self/mounts)
test -n "$CGROUP_MOUNT" || bail_out "Failed to get cgroup2 mount dir!"
CURRENT_CGROUP_PATH=$(cat /proc/self/cgroup | sed -n s/^0:://p)
CGROUP_PATH="${CGROUP_MOUNT}${CURRENT_CGROUP_PATH}/cgroup-device-apply.$$"
echo "# using CGROUP_PATH=$CGROUP_PATH"

json_empty='{}'
json_policy_only='{"DevicePolicy":"auto"}'
json_closed_only='{"DevicePolicy":"closed"}'
json_strict_empty='{"DevicePolicy":"strict","DeviceAllow":[]}'
json_closed_null='{"DevicePolicy":"closed","DeviceAllow":[["/dev/null","rw"]]}'

if test_have_prereq SUDO; then
    if $SUDO mkdir $CGROUP_PATH 2>/dev/null; then
        test_set_prereq CGROUPFS
        cleanup "$SUDO rmdir $CGROUP_PATH"
    fi
    if test_have_prereq CGROUPFS; then
        if $SUDO $bpf_cgroup_probe; then
            test_set_prereq BPF_CGROUP
        fi
    fi
fi

# ---- Argument validation tests (exit 2) ----

test_expect_success 'no arguments: exits with status 2' '
    test_expect_code 2 $cgroup_device_apply
'

test_expect_success 'too many arguments: exits with status 2' '
    test_expect_code 2 $cgroup_device_apply /sys/fs/cgroup/user.slice extra
'

test_expect_success 'relative path: exits with status 2' '
    test_expect_code 2 $cgroup_device_apply sys/fs/cgroup/user.slice
'

test_expect_success 'path outside /sys/fs/cgroup/: exits with status 2' '
    test_expect_code 2 $cgroup_device_apply /sys/fs/other/user.slice
'

# ---- No-op tests (exit 0) ----

test_expect_success 'empty JSON object: exits 0 (no-op)' '
    echo "$json_empty" | $cgroup_device_apply /sys/fs/cgroup/user.slice
'

test_expect_success 'DevicePolicy=auto with no DeviceAllow: exits 0 (no-op)' '
    echo "$json_policy_only" | $cgroup_device_apply /sys/fs/cgroup/user.slice
'

test_expect_success 'DevicePolicy=closed with no DeviceAllow is not a no-op' '
    echo "$json_closed_only" |
        test_expect_code 1 \
            $cgroup_device_apply /sys/fs/cgroup/nonexistent.$$
'

# ---- Runtime failure tests (exit 1) ----

test_expect_success 'bad JSON on stdin: exits with status 1' '
    echo "not json" |
        test_expect_code 1 $cgroup_device_apply /sys/fs/cgroup/user.slice
'

test_expect_success 'nonexistent cgroup path with DeviceAllow: exits with status 1' '
    echo "$json_closed_null" |
        test_expect_code 1 \
            $cgroup_device_apply /sys/fs/cgroup/nonexistent.$$
'

# ---- Privileged end-to-end tests ----

cat <<'EOF' >run-in-cgroup.sh
#!/bin/sh
path=$1; shift
test -d $path || mkdir -p $path &&
echo $$ >${path}/cgroup.procs &&
exec "$@"
EOF
chmod +x run-in-cgroup.sh

test_expect_success BPF_CGROUP \
    'strict policy with empty DeviceAllow denies /dev/null in cgroup' '
    echo "$json_strict_empty" | $SUDO $cgroup_device_apply $CGROUP_PATH &&
    test_must_fail $SUDO ./run-in-cgroup.sh $CGROUP_PATH \
        cat /dev/null >deny-null.out 2>&1 &&
    test_debug "cat deny-null.out" &&
    grep -q "Operation not permitted" deny-null.out
'

test_expect_success BPF_CGROUP \
    'closed policy with /dev/null:rw permits /dev/null in cgroup' '
    echo "$json_closed_null" | $SUDO $cgroup_device_apply $CGROUP_PATH &&
    $SUDO ./run-in-cgroup.sh $CGROUP_PATH cat /dev/null
'

test_done
