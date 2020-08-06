#docker run --rm busybox /bin/sh "-ec" "while :; do date ; sleep 5 ; done"

docker run --rm \
  --log-driver=splunk \
  --log-opt splunk-token=176FCEBF-4CF5-4EDF-91BC-703796522D20 \
  --log-opt splunk-url=http://localhost:8088 \
  --log-opt splunk-insecureskipverify=true \
  --log-opt splunk-verify-connection=false \
  hello-world



docker run --rm \
   --log-driver=splunk \
   --log-opt splunk-token=176FCEBF-4CF5-4EDF-91BC-703796522D20 \
   --log-opt splunk-url=http://localhost:8088 \
   --log-opt splunk-insecureskipverify=true \
   --log-opt splunk-verify-connection=false \
   --log-opt tag="{{.ImageName}}/{{.Name}}/{{.ID}}" \
   --log-opt labels=location \
   --log-opt env=TEST \
   --env "TEST=false" \
   --label location=west \
   busybox /bin/sh "-ec" "while :; do date ; echo 'another log'; sleep 5 ; done"


#https://docs.docker.com/v17.09/engine/admin/logging/splunk/#splunk-options


docker run --rm busybox /bin/sh "-ec" "while :; do date ; echo 'another log'; sleep 5 ; done"



