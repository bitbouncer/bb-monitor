#!/bin/bash

while true; do date; ts=$(date +'%s'); i=0; while [ $i -lt 100000 ]; do echo "some_metric{label=\"val$i\"}" $ts; i=$((i+1)); done | curl --data-binary @- http://localhost:9091/metrics/job/some_job; done
