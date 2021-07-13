#!/usr/bin/env python3
#
#  Generate a build matrix for use with github workflows
#

import json
import os
import re

docker_run_checks = "src/test/docker/docker-run-checks.sh"

default_args = (
    "--prefix=/usr"
    " --sysconfdir=/etc"
    " --with-systemdsystemunitdir=/etc/systemd/system"
    " --localstatedir=/var"
)

class BuildMatrix:
    def __init__(self):
        self.matrix = []
        self.branch = None
        self.tag = None

        #  Set self.branch or self.tag based on GITHUB_REF
        if "GITHUB_REF" in os.environ:
            self.ref = os.environ["GITHUB_REF"]
            match = re.search("^refs/heads/(.*)", self.ref)
            if match:
                self.branch = match.group(1)
            match = re.search("^refs/tags/(.*)", self.ref)
            if match:
                self.tag = match.group(1)

    def add_build(
        self,
        name=None,
        image="bionic",
        args=default_args,
        jobs=2,
        env=None,
        coverage=False,
        platform=None,
        command_args="",
    ):
        """Add a build to the matrix.include array"""

        # Extra environment to add to this command:
        env = env or {}

        needs_buildx = False
        if platform:
            command_args += f"--platform={platform}"
            needs_buildx = True

        # The command to run:
        command = f"{docker_run_checks} -j{jobs} --image={image} {command_args}"

        if coverage:
            env["COVERAGE"] = "t"

        create_release = False
        if self.tag and "DISTCHECK" in env:
            create_release = True

        self.matrix.append(
            {
                "name": name,
                "env": env,
                "command": command,
                "image": image,
                "tag": self.tag,
                "branch": self.branch,
                "coverage": coverage,
                "needs_buildx": needs_buildx,
                "create_release": create_release,
            }
        )

    def __str__(self):
        """Return compact JSON representation of matrix"""
        return json.dumps(
            {"include": self.matrix}, skipkeys=True, separators=(",", ":")
        )


matrix = BuildMatrix()

# Ubuntu: no args
matrix.add_build(name="bionic")

# Ubuntu: 32b
matrix.add_build(
    name="bionic - 32 bit",
    platform="linux/386",
)

# Ubuntu: gcc-8, content-s3, distcheck
matrix.add_build(
    name="bionic - gcc-8,distcheck",
    env=dict(
        CC="gcc-8",
        CXX="g++8",
        DISTCHECK="t",
    ),
)

# Ubuntu: clang-6.0
matrix.add_build(
    name="bionic - clang-6.0",
    env=dict(
        CC="clang-6.0",
        CXX="clang++-6.0",
        chain_lint="t",
    ),
    command_args="--workdir=/usr/src/" + "workdir/" * 15,
)

# Ubuntu: coverage
matrix.add_build(
    name="coverage",
    coverage=True,
    jobs=2,
)

# Ubuntu 20.04: py3.8
matrix.add_build(
    name="focal",
    image="focal",
)

# Centos7
matrix.add_build(
    name="centos7",
    image="centos7",
)

# Centos8
matrix.add_build(
    name="centos8",
    image="centos8",
)

# Fedora 33
matrix.add_build(
    name="fedora33",
    image="fedora33",
)

# Fedora 33 ASan
matrix.add_build(
    name="fedora33 - asan",
    image="fedora33",
    args="--enable-sanitizers"
)
print(matrix)
