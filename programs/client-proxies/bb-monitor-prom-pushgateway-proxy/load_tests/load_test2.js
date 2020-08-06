import http from "k6/http";
import crypto from "k6/crypto";

let payload = `http_request_duration_seconds_bucket{le="0.05"} 24054
http_request_duration_seconds_bucket{le="0.1"} 33444
http_request_duration_seconds_bucket{le="0.2"} 100392
`;

export default function (data) {
    console.log(crypto.sha256(payload, "hex"));
    let resp = http.post("https://httpbin.org/anything", "some_metric{label=\"val1\"} 42");
    console.log(crypto.sha256(resp.json().data, "hex"));
    console.log(resp.body);
}
