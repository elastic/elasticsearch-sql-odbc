#!/bin/bash

set -euo pipefail

echo "steps:"

source .buildkite/branches.sh

for BRANCH in "${BRANCHES[@]}"; do
  cat <<EOF
  - trigger: elasticsearch-sql-odbc-dra-workflow
    label: Trigger DRA snapshot workflow for $BRANCH
    async: true
    build:
      branch: $BRANCH
      env:
        DRA_WORKFLOW: snapshot
  - trigger: elasticsearch-sql-odbc-dra-workflow
    label: Trigger DRA staging workflow for $BRANCH
    async: true
    build:
      branch: $BRANCH
      env:
        DRA_WORKFLOW: staging
EOF
done
