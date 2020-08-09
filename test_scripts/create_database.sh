#!/bin/bash
set -ef 

export APP_REALM=dev

export DB_USER=postgres
export DB_PASSWORD=Pass2020!

# get ip of localhost
export DB_HOST=$(ip route get 8.8.8.8 | awk -F"src " 'NR==1{split($2,a," ");print a[1]}')
echo IP: $DB_HOST

export PGOPTIONS='--client-min-messages=warning'
export PSQL="docker run --rm -e PGPASSWORD=$DB_PASSWORD -e PGOPTIONS=$PGOPTIONS postgres psql -U $DB_USER -h $DB_HOST -b -q -t"

for SUBYSTEM in monitor
do
export DATABASE=bb_${SUBYSTEM}_${APP_REALM}
export SCHEMA_FILE=${SUBYSTEM}.schema
export INTIAL_DATA_FILE=data-${APP_REALM}/${SUBYSTEM}.data

echo "CREATE ${DATABASE}"
$PSQL -tc "SELECT 1 FROM pg_database WHERE datname = '$DATABASE'" | grep -q 1 || $PSQL -c "CREATE DATABASE $DATABASE"

echo "CREATING SCHEMA FOR ${DATABASE}"
docker run --rm -e PGPASSWORD=$DB_PASSWORD -v ${PWD}/${SCHEMA_FILE}:/tmp/pgfile postgres psql -U $DB_USER -h $DB_HOST -b -q -t -f /tmp/pgfile $DATABASE

echo "WRITE INITIAL DATA FOR ${DATABASE}"
docker run --rm -e PGPASSWORD=$DB_PASSWORD -v ${PWD}/${INTIAL_DATA_FILE}:/tmp/pgfile postgres psql -U $DB_USER -h $DB_HOST -b -q -t -f /tmp/pgfile $DATABASE
done


