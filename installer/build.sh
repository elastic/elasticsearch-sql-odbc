#!/usr/bin/env bash

# Copyright Elasticsearch B.V. and/or licensed to Elasticsearch B.V. under one
# or more contributor license agreements. Licensed under the Elastic License;
# you may not use this file except in compliance with the Elastic License.

FAKE="packages/build/FAKE.x64/tools/FAKE.exe"
BUILDSCRIPT="build/scripts/Targets.fsx"
FAKE_NO_LEGACY_WARNING=true

mono .paket/paket.bootstrapper.exe
if [[ -f .paket.lock ]]; then mono .paket/paket.exe restore; fi
if [[ ! -f .paket.lock ]]; then mono .paket/paket.exe install; fi
mono $FAKE --removeLegacyFakeWarning $BUILDSCRIPT "cmdline=$*" --fsiargs -d:MONO
