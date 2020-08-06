SINGLE TENNANT PUSHGATEWAY - NO AUTHENTIFICATION
convert to avro::prometherus::metrics and pushes those to kafka



echo "some_metric 3.14" | curl --data-binary @- http://localhost:9091/metrics/job/some_job


cat <<EOF | curl --data-binary @- http://localhost:9091/metrics/job/some_job/instance/some_instance
  # TYPE some_metric counter
  some_metric{label="val1"} 42
  # TYPE another_metric gauge
  # HELP another_metric Just an example.
  another_metric 2398.283
  EOF

