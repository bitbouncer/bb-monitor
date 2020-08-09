#!/bin/bash

if [ $# -lt "3" ]; then
        echo "Usage: $0 build_number schema database"
        exit 1;
fi

BUILD_NO=$1
PGFILE=$2
DATABASE=$3

export DB_USER=postgres
export DB_HOST=10.1.46.27
export DB_PASSWORD=2Secret

export PSQL="docker run --rm -e PGPASSWORD=$DB_PASSWORD postgres psql -U $DB_USER -h $DB_HOST -b -q -t"

$PSQL -c "CREATE TABLE IF NOT EXISTS build_version (nr integer NOT NULL); TRUNCATE build_version" $DATABASE
$PSQL -c "INSERT INTO build_version (nr) VALUES ($BUILD_NO);" $DATABASE
docker run --rm -e PGPASSWORD=$DB_PASSWORD -v ${PWD}/${PGFILE}:/tmp/pgfile postgres psql -U $DB_USER -h $DB_HOST -b -q -t -f /tmp/pgfile $DATABASE

