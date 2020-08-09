export DB_USER=postgres
export DB_HOST=10.1.46.27
export DB_PASSWORD=2Secret

export PSQL="docker run --rm -e PGPASSWORD=$DB_PASSWORD -e PGOPTIONS='--client-min-messages=warning' postgres psql -U $DB_USER -h $DB_HOST -b -q"

$PSQL -tc "SELECT 1 FROM pg_database WHERE datname = 'bbmonitor_dev'" | grep -q 1 || $PSQL -c "CREATE DATABASE bbmonitor_dev"
$PSQL -tc "SELECT 1 FROM pg_database WHERE datname = 'bbnagios_dev'" | grep -q 1 || $PSQL -c "CREATE DATABASE bbnagios_dev"

$PSQL -tc "SELECT 1 FROM pg_database WHERE datname = 'bbmonitor_test'" | grep -q 1 || $PSQL -c "CREATE DATABASE bbmonitor_test"
$PSQL -tc "SELECT 1 FROM pg_database WHERE datname = 'bbnagios_test'" | grep -q 1 || $PSQL -c "CREATE DATABASE bbnagios_test"

$PSQL -tc "SELECT 1 FROM pg_database WHERE datname = 'bbmonitor_staging'" | grep -q 1 || $PSQL -c "CREATE DATABASE bbmonitor_staging"
$PSQL -tc "SELECT 1 FROM pg_database WHERE datname = 'bbnagios_staging'" | grep -q 1 || $PSQL -c "CREATE DATABASE bbnagios_staging"

$PSQL -tc "SELECT 1 FROM pg_database WHERE datname = 'bbmonitor_prod'" | grep -q 1 || $PSQL -c "CREATE DATABASE bbmonitor_prod"
$PSQL -tc "SELECT 1 FROM pg_database WHERE datname = 'bbnagios_prod'" | grep -q 1 || $PSQL -c "CREATE DATABASE bbnagios_prod"


#create schema
#$PSQL -f monitor-schema.sql bb-monitor-dev
#$PSQL -f nagios-schema.sql bb-monitor-dev
#$PSQL -f monitor-data-dev.sql bb-monitor-dev
#$PSQL -f nagios-data-dev.sql bb-nagios-dev
#insert default








