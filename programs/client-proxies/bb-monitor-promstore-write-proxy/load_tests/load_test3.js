import http from "k6/http";
import crypto from "k6/crypto";

let payload = 'http_request_duration_seconds_bucket{le="0.05"} 24054;
http_request_duration_seconds_bucket;{le="0.1"} 33444;
http_request_duration_seconds_bucket;{le="0.2"} 100392;
';;

export let options = {
  // simulate rampup of traffic from 1 to 200 users over 5 minutes.
  stages: [
    { duration: "1m", target: 20 },
  ]
};

let headers = { "Dd-Api-Key": "2cc3d365-c765-4d3e-933e-f1bb9abd4419" };

export default function (data) {
    //console.log(crypto.sha256(payload, "hex"));
    let resp = http.post("http://localhost:9091/metrics/job/some_job/instance/some_instance", payload, { headers: headers });
    //console.log(crypto.sha256(resp.json().data, "hex"));
    console.log(resp.body);
}
