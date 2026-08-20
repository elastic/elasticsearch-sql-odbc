#!/usr/bin/env bash
set -euo pipefail

WORKFLOW="${DRA_WORKFLOW:?DRA_WORKFLOW is required}"

echo "--- :compression: Downloading ${WORKFLOW} MSI artifacts"

buildkite-agent artifact download '*.msi' .

echo "--- :package: Staging ${WORKFLOW} artifacts"
mkdir -p artifacts

find . -name "*.msi" ! -path "./artifacts/*" -exec cp {} artifacts/ \;

if ! ls artifacts/*.msi 1>/dev/null 2>&1; then
  echo "ERROR: no ${WORKFLOW} MSI artifacts found." >&2
  exit 1
fi

echo "Staged artifacts:"
ls -1 artifacts/
