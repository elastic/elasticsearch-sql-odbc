#!/usr/bin/env bash
set -euo pipefail

STACK_VERSION=$(grep 'set(DRV_VERSION' CMakeLists.txt | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+')
sed "s/\${STACK_VERSION}/${STACK_VERSION}/g" .buildkite/pipeline.dra.yml
