#!/bin/sh
set -eu
# Development-only toolchain setup. The actual build/test container has no network.
docker build -t agency-linux-toolchain -f tests/Dockerfile .
docker run --rm --init --network none --mount "type=bind,source=$(pwd),target=/source,readonly" agency-linux-toolchain sh -c '
    cp -R /source /tmp/agency
    cd /tmp/agency
    rm -rf build
    uname -a
    cat /etc/os-release
    cc --version
    make clean && make && make test
'
