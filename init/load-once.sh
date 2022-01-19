#! /bin/bash

# This script ensures that kdump is (re-)loaded once.
# It prevents multiple invocation of load.sh even if multiple instances
# of the script are run in parallel. The first instance repeats the action
# as long as there is any pending work.

LOCKFILE=/var/run/kdump.lock
JOBSFILE=/var/run/kdump.jobs
export JOBSFILE

# Add a pending job
echo $$ >> "$JOBSFILE"

# Make sure lock-file exists
> "$LOCKFILE"

flock -n "$LOCKFILE" -c '
    while [ -s "$JOBSFILE" ]
    do
        truncate -s 0 "$JOBSFILE" || exit 1
        /usr/lib/kdump/load.sh
    done'
