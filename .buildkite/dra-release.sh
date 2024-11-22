#!/bin/bash

set -euo pipefail

DRA_WORKFLOW=${DRA_WORKFLOW:-snapshot}

if [[ ("$BUILDKITE_BRANCH" == "main" || "$BUILDKITE_BRANCH" == *.x) && "$DRA_WORKFLOW" == "staging" ]]; then
	exit 0
fi

buildkite-agent artifact download '*.msi' .

RM_BRANCH="$BUILDKITE_BRANCH"
if [[ "$BUILDKITE_BRANCH" == "main" ]]; then
	RM_BRANCH=master
fi

DRV_VERSION=$(grep 'set(DRV_VERSION' CMakeLists.txt | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+')
GIT_COMMIT=$(git rev-parse HEAD)

# Allow other users access to read the artifacts so they are readable in the container
chmod a+r installer/build/out/*

# Allow other users write access to create checksum files
chmod a+w installer/build/out

docker run --rm \
	--name release-manager \
	-e VAULT_ADDR="$DRA_VAULT_ADDR" \
	-e VAULT_ROLE_ID="$DRA_VAULT_ROLE_ID_SECRET" \
	-e VAULT_SECRET_ID="$DRA_VAULT_SECRET_ID_SECRET" \
	--mount type=bind,readonly=false,src="$PWD",target=/artifacts \
	docker.elastic.co/infra/release-manager:latest \
	cli collect \
	--project elasticsearch-sql-odbc \
	--branch "$RM_BRANCH" \
	--commit "$GIT_COMMIT" \
	--workflow "$DRA_WORKFLOW" \
	--version "$DRV_VERSION" \
	--artifact-set main
