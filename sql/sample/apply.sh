#!/bin/bash
set -ef 

export BUILD_NUMBER=$1
export APP_REALM=${2:-dev}

if [ $# -lt "2" ]; then
        echo "Usage: $0 build_number app_realm"
        exit 1;
fi

export DB_USER=postgres
export DB_HOST=10.1.46.27
export DB_PASSWORD=2Secret

export PGOPTIONS='--client-min-messages=warning'
export PSQL="docker run --rm -e PGPASSWORD=$DB_PASSWORD -e PGOPTIONS=$PGOPTIONS postgres psql -U $DB_USER -h $DB_HOST -b -q -t"

for SUBYSTEM in monitor nagios
do
export DATABASE=bb${SUBYSTEM}_${APP_REALM}
export SCHEMA_FILE=${SUBYSTEM}.schema
export INTIAL_DATA_FILE=data-${APP_REALM}/${SUBYSTEM}.data

echo "CREATE ${DATABASE}"
$PSQL -tc "SELECT 1 FROM pg_database WHERE datname = '$DATABASE'" | grep -q 1 || $PSQL -c "CREATE DATABASE $DATABASE"

echo "CREATING SCHEMA FOR ${DATABASE}"
docker run --rm -e PGPASSWORD=$DB_PASSWORD -v ${PWD}/${SCHEMA_FILE}:/tmp/pgfile postgres psql -U $DB_USER -h $DB_HOST -b -q -t -f /tmp/pgfile $DATABASE

#write current version in db
$PSQL -c "INSERT INTO versions (key, version) VALUES ('JENKINS_BUILD_NO', '$BUILD_NUMBER');" $DATABASE

echo "WRITE INITIAL DATA FOR ${DATABASE}"
docker run --rm -e PGPASSWORD=$DB_PASSWORD -v ${PWD}/${INTIAL_DATA_FILE}:/tmp/pgfile postgres psql -U $DB_USER -h $DB_HOST -b -q -t -f /tmp/pgfile $DATABASE
done


