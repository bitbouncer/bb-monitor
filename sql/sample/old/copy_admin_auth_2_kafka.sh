#!/bin/bash
export TOPIC_NAME=BB_MONITOR_ADMIN_AUTH_VIEWv0
export sixty_minutes=3600000
export TOPIC_OPTS="cleanup.policy=compact,segment.ms=$sixty_minutes"

docker run --rm confluentinc/cp-kafka:5.1.0 kafka-topics --delete  --topic $TOPIC_NAME --zookeeper $KSPP_ZK_URL
docker run --rm confluentinc/cp-kafka:5.1.0 kafka-topics --if-not-exists --create --zookeeper $KSPP_ZK_URL --partitions 1 --replication-factor 3 --topic $TOPIC_NAME
docker run --rm confluentinc/cp-kafka:5.1.0 kafka-configs --zookeeper $KSPP_ZK_URL --alter --entity-type topics --entity-name $TOPIC_NAME --add-config $TOPIC_OPTS

#postgres2kafka --db_host=10.1.46.27 --db_dbname=bb_monitor --db_user=postgres --db_password=2Secret --table=admin_auth_view --id_column=id --timestamp_column=ts --topic=$TOPIC_NAME --oneshot
postgres2kafka --db_host=localhost --db_dbname=bb_monitor --db_user=postgres --db_password=docker --table=admin_auth_view --id_column=id --timestamp_column=ts --topic=$TOPIC_NAME --oneshot
