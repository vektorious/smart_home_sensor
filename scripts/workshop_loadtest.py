#!/usr/bin/env python3
"""Simulate a room full of Smart Home Sensors against a diy-sensor.org instance.

Sends what the firmware sends: the same eight BME680/BSEC values, the same
payload shape, the same cadence. The point is to exercise the *server* config
before a workshop does it for real — in particular the limits that only bite
when many devices share one credential:

  * the write budget, charged per API key and per stored value, so N devices
    cost N x sensors / interval rows per second against one bucket;
  * new devices per hour per IP, since a class sits behind one NAT address and
    claims every ID within the first hour;
  * the write burst, which absorbs the first synchronised round (N x sensors
    rows arriving at once) and hides a misconfigured rate until it drains.

Run it with the REAL workshop key. A separate key would get its own budget
bucket and report a healthy run that proves nothing about the workshop.

    export SHS_API_KEY='shs-ws2026-...'
    python3 scripts/workshop_loadtest.py --devices 50 --duration 2h

Every device it creates is named <prefix>-<date>-NN and grouped in its own
project, so nothing lands in the real workshop dashboard, and cleanup is one
loop — written to cleanup_<project>.sh as the run starts.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import random
import subprocess
import sys
import time
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone

# Matches code/shs_modular/sensorboard.ino: eight values per reading. The count
# is what the write budget is charged in, so it must not drift from the firmware.
SENSOR_COUNT = 8


def parse_duration(text: str) -> int:
    """'90s', '45m', '2h' or a bare number of seconds."""
    text = text.strip().lower()
    mult = {"s": 1, "m": 60, "h": 3600}
    if text and text[-1] in mult:
        return int(float(text[:-1]) * mult[text[-1]])
    return int(float(text))


class Device:
    """One simulated board, with readings that drift like a real room."""

    def __init__(self, index: int, prefix: str, stamp: str):
        self.device_id = f"{prefix}-{stamp}-{index:02d}"
        self.name = f"Loadtest {index:02d}"
        # A real device derives this; here it only has to be stable per device.
        self.write_key = "%032x" % random.getrandbits(128)
        self.temp = random.uniform(20.0, 24.0)
        self.hum = random.uniform(40.0, 55.0)
        self.iaq = random.uniform(30.0, 90.0)
        self.accuracy = 0
        self.reading = 0

    def step(self) -> dict:
        """Advance the simulated environment one interval."""
        self.reading += 1
        self.temp += random.uniform(-0.15, 0.15)
        self.hum += random.uniform(-0.6, 0.6)
        self.iaq = max(10.0, min(350.0, self.iaq + random.uniform(-12, 12)))
        # BSEC climbs 0->3 over the first readings, as it does on hardware.
        if self.reading > 3 and self.accuracy < 3 and random.random() < 0.25:
            self.accuracy += 1
        return {
            "iaq": {"value": round(self.iaq, 0), "unit": ""},
            "iaq_accuracy": {"value": self.accuracy, "unit": ""},
            "co2_equivalent": {"value": round(500 + self.iaq * 4, 0), "unit": "ppm"},
            "voc_equivalent": {"value": round(0.3 + self.iaq / 120, 2), "unit": "ppm"},
            "temperature": {"value": round(self.temp, 2), "unit": "C"},
            "humidity": {"value": round(self.hum, 1), "unit": "%"},
            "pressure": {"value": round(random.uniform(1008, 1020), 1), "unit": "hPa"},
            "wifi_rssi": {"value": random.randint(-78, -45), "unit": "dBm"},
        }

    def payload(self, project: str) -> str:
        return json.dumps({
            "device_id": self.device_id,
            "name": self.name,
            "write_key": self.write_key,
            "project": project,
            "sensors": self.step(),
        })


def post(url: str, api_key: str, body: str, timeout: int = 30) -> tuple[int, str]:
    """One POST via curl. Returns (http_status, response_body)."""
    proc = subprocess.run(
        ["curl", "-sS", "--max-time", str(timeout),
         "-w", "\n%{http_code}", "-X", "POST", url,
         "-H", "content-type: application/json",
         "-H", f"X-API-Key: {api_key}",
         "--data-binary", "@-"],
        input=body, capture_output=True, text=True,
    )
    out = proc.stdout.rsplit("\n", 1)
    if len(out) != 2 or not out[1].strip().isdigit():
        return (-1, (proc.stderr or proc.stdout).strip()[:200])
    return (int(out[1]), out[0].strip())


def run_round(devices, url, api_key, project, workers) -> tuple[Counter, list]:
    codes: Counter = Counter()
    failures: list = []

    def one(dev):
        # A little jitter: real boards do not share a clock, and a perfectly
        # synchronised round is both unrealistic and unfairly harsh.
        time.sleep(random.uniform(0, 4))
        return dev, post(url, api_key, dev.payload(project))

    with ThreadPoolExecutor(max_workers=workers) as pool:
        for dev, (code, body) in pool.map(one, devices):
            codes[code] += 1
            if code not in (200, 201):
                failures.append((dev.device_id, code, body[:160]))
    return codes, failures


def write_cleanup(path: str, devices, project: str) -> None:
    with open(path, "w") as fh:
        fh.write("#!/usr/bin/env bash\n")
        fh.write(f"# Remove every device created by the load test in project '{project}'.\n")
        fh.write("# Run on the server, from ~/sensor_board.\n")
        fh.write("#\n")
        fh.write("# Not strictly required: with PERSISTENT_DEVICES=false these expire on\n")
        fh.write("# their own 48 h after their last reading. This just does it now.\n")
        fh.write("set -eu\n")
        fh.write('ADMIN="${ADMIN:-.venv/bin/python -m app.admin}"\n\n')
        for dev in devices:
            fh.write(f"$ADMIN delete-device {dev.device_id}\n")
        fh.write("\necho 'Load-test devices removed.'\n")
        fh.write("$ADMIN status\n")
    os.chmod(path, 0o755)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--devices", type=int, default=50)
    ap.add_argument("--interval", default="5m", help="between rounds (default 5m)")
    ap.add_argument("--duration", default="2h", help="total run time (default 2h)")
    ap.add_argument("--url", default="https://diy-sensor.org/sensor/measurement")
    ap.add_argument("--prefix", default="loadtest")
    ap.add_argument("--project", default=None, help="default: <prefix>-<date>")
    ap.add_argument("--workers", type=int, default=25)
    ap.add_argument("--dry-run", action="store_true",
                    help="print the plan and the first payload, send nothing")
    args = ap.parse_args()

    api_key = os.environ.get("SHS_API_KEY", "")
    if not api_key and not args.dry_run:
        print("SHS_API_KEY is not set. Use the real workshop key: a different key "
              "gets its own write-budget bucket and the run proves nothing.",
              file=sys.stderr)
        return 2

    stamp = datetime.now(timezone.utc).strftime("%Y%m%d")
    project = args.project or f"{args.prefix}-{stamp}"
    interval = parse_duration(args.interval)
    duration = parse_duration(args.duration)
    rounds = max(1, math.ceil(duration / interval))

    devices = [Device(i, args.prefix, stamp) for i in range(1, args.devices + 1)]
    rows_per_s = args.devices * SENSOR_COUNT / interval

    print(f"Devices        {args.devices}  ({devices[0].device_id} .. {devices[-1].device_id})")
    print(f"Project        {project}")
    print(f"Endpoint       {args.url}")
    print(f"Cadence        every {interval}s for {duration}s  ->  {rounds} rounds")
    print(f"Sustained load {rows_per_s:.2f} rows/s   ({args.devices} x {SENSOR_COUNT} / {interval})")
    print(f"First round    {args.devices * SENSOR_COUNT} rows against the write burst")
    print(f"Total          {rounds * args.devices} requests, "
          f"{rounds * args.devices * SENSOR_COUNT} rows")

    if args.dry_run:
        print("\n--dry-run: example payload\n")
        print(devices[0].payload(project))
        return 0

    cleanup = f"cleanup_{project}.sh"
    write_cleanup(cleanup, devices, project)
    print(f"Cleanup script {cleanup}  (copy to the server and run)\n", flush=True)

    started = time.time()
    totals: Counter = Counter()
    for rnd in range(1, rounds + 1):
        t0 = time.time()
        codes, failures = run_round(devices, args.url, api_key, project, args.workers)
        totals.update(codes)
        ok = codes[200] + codes[201]
        stamp_now = datetime.now().strftime("%H:%M:%S")
        summary = ", ".join(f"{c}x{n}" for c, n in sorted(codes.items()))
        print(f"[{stamp_now}] round {rnd:2}/{rounds}  ok {ok}/{len(devices)}  "
              f"[{summary}]  {time.time() - t0:.1f}s", flush=True)
        for dev_id, code, body in failures[:5]:
            print(f"           FAIL {dev_id} -> {code} {body}", flush=True)
        if len(failures) > 5:
            print(f"           ... and {len(failures) - 5} more", flush=True)

        if rnd < rounds:
            sleep_for = interval - (time.time() - t0)
            if sleep_for > 0:
                time.sleep(sleep_for)

    ok = totals[200] + totals[201]
    total = sum(totals.values())
    print(f"\nDone in {(time.time() - started) / 60:.1f} min")
    print(f"  requests   {total}")
    print(f"  accepted   {ok}  ({100 * ok / total:.1f}%)")
    print(f"  by status  {dict(sorted(totals.items()))}")
    print(f"  dashboard  https://diy-sensor.org/dashboard/project/{project}")
    print(f"  cleanup    bash {cleanup}   (on the server, from ~/sensor_board)")
    return 0 if ok == total else 1


if __name__ == "__main__":
    sys.exit(main())
