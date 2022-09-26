#!/bin/bash

#set -e

START_TIMEOUT=180

URL=http://elastic-admin:elastic-password@localhost:9200
#URL=https://elastic:5IohDZ6nrmxFCVqribpNN3uu@fabf7d82798c46d7bb926a657b159549.europe-west1.gcp.cloud.es.io:9243 # dev
#URL=https://elastic:KBNYf5Sv3WXh8fHBHOkB3UjL@0fe873d566cc458684a2cfc0831423a2.europe-west1.gcp.cloud.es.io:9243

ITES_DIR=$(dirname $(realpath $0))
WAIT_PID_FILE=$(mktemp -u)
STARTED_AT=$(date +%s)

function index() {
	python3 $ITES_DIR/ites.py -p $URL -tx -o /home/bpi/RnD/elastic/snapshots/ites_data/
}

function wait_to_start() {
	start=$(date +%s)
	while true; do
		now=$(date +%s)
		if ((now - start > $START_TIMEOUT)); then
			for ((i=0; i < 15; i ++)); do # attempt to make it visible
				echo "Timed-out waiting for ES to start"
			done
			exit 1
		fi
		answ=$(curl $URL/_sql -H 'Content-Type: application/json' -d '{"query": "select 1"}' 2>/dev/null \
			| jq '.rows[0][0]' 2>/dev/null )
		if [ "$answ" == "1" ]; then
			echo "ES started!"
			break
		fi
		sleep 1s
	done
	echo "ES started, SQL available."
}

function wait_and_index() {
	wait_to_start
	sleep 1s
	index
	rm $WAIT_PID_FILE

	echo "Done in: $(( $(date +%s) - STARTED_AT ))s"
}

function kill_waiter() {
	PID=$(cat $WAIT_PID_FILE 2>/dev/null)
	if [ ! -z $PID ]; then
		kill -term $PID
		rm $WAIT_PID_FILE
	fi
}

if [ $# -le 0 ]; then
	index
else
	wait_and_index &
	echo $! > $WAIT_PID_FILE
	trap kill_waiter EXIT SIGINT

	# --debug-jvm OR .
	~/RnD/elastic/script/es.sh $*
fi

