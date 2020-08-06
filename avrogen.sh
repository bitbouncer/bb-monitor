#!/usr/bin/env bash
set -ef
mkdir -p bb_monitor_utils/avro/
kspp_avrogencpp -i schemas/bb_avro_metrics.schema -o bb_monitor_utils/avro/bb_avro_metrics.h
kspp_avrogencpp -i schemas/bb_avro_logline.schema -o bb_monitor_utils/avro/bb_avro_logline.h
kspp_avrogencpp -i schemas/dd_avro_intake.schema -o  bb_monitor_utils/avro/dd_avro_intake.h

