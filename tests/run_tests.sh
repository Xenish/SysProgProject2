#!/bin/sh
set -eu
for dir in tasks status reports messages logs data; do
    test -f "$dir/.gitkeep"
done
test -f data/survey.csv
test -f tasks/task_001.task
echo 'PASS P0: workspace and authored fixtures'
