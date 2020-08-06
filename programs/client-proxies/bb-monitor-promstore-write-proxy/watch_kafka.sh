#!/bin/bash
docker run --rm confluentinc/cp-schema-registry:5.1.0 \
kafka-avro-console-consumer --topic BB_MONITOR_C1_DEV_metrics --bootstrap-server $KSPP_KAFKA_BROKER_URL --property schema.registry.url=$KSPP_SCHEMA_REGISTRY_URL 


