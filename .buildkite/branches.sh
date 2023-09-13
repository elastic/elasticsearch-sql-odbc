#!/bin/bash

# This determines which branches will have pipelines triggered periodically, for dra workflows.
BRANCHES=( $(curl -s https://raw.githubusercontent.com/elastic/elasticsearch/main/branches.json | jq -r '.branches[].branch') )
