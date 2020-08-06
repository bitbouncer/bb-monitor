import http from "k6/http";
import { check, fail } from "k6";

export let options = {
  // simulate rampup of traffic from 1 to 200 users over 5 minutes.
  stages: [
    { duration: "5m", target: 200 },
  ]
};

export default function() {
  let req1 = {
    method: "POST",
    url: "http://localhost:9091/metrics/job/some_job/instance/some_instance",
    body: {
      hello: "some_metric{label=\"val1\"} 42",
    },
    params: { headers: { "Content-Type": "application/x-www-form-urlencoded" } }	
   };

  var res = http.post("http://localhost:9091/metrics/job/some_job/instance/some_instance", "some_metric{label=\"val1\"} 42");
}

