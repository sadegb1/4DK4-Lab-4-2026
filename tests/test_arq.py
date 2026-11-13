#!/usr/bin/env python3
import csv
import io
import subprocess
import sys


EXECUTABLE = sys.argv[1]


def simulate(*arguments):
    result = subprocess.run(
        [EXECUTABLE, *arguments],
        check=True,
        capture_output=True,
        text=True,
    )
    rows = list(csv.DictReader(io.StringIO(result.stdout)))
    assert rows, result.stdout
    return rows


def rejects(*arguments):
    result = subprocess.run(
        [EXECUTABLE, *arguments],
        capture_output=True,
        text=True,
    )
    assert result.returncode != 0
    assert result.stderr


sw = simulate("--source", "finite", "--packets", "5", "--ws", "1",
              "--wr", "1", "--pd", "0", "--pa", "0", "--seed", "41")[0]
assert sw["protocol"] == "stop-and-wait"
assert sw["generated"] == "5"
assert sw["delivered"] == "5"
assert sw["acked"] == "5"
assert sw["data_attempts"] == "5"
assert sw["attempts_per_delivered"] == "1.000000"
assert sw["simulation_time_ms"] == "55.000000"
assert sw["link_efficiency"] == "0.090909"
assert sw["mean_delay_ms"] == "28.000000"

gbn = simulate("--source", "backlogged", "--runlength", "25", "--ws", "12",
               "--wr", "1", "--pd", "0", "--pa", "0", "--seed", "41")[0]
assert gbn["protocol"] == "go-back-n"
assert gbn["delivered"] == "25"
assert gbn["timeouts"] == "0"
assert gbn["attempts_per_delivered"] == "1.000000"

sr = simulate("--source", "finite", "--packets", "20", "--ws", "4",
              "--wr", "4", "--pd", "0.1", "--pa", "0.1",
              "--seed", "41")[0]
assert sr["protocol"] == "selective-repeat"
assert sr["delivered"] == "20"
assert sr["acked"] == "20"
assert int(sr["data_errors"]) > 0
assert int(sr["ack_errors"]) > 0
assert int(sr["timeouts"]) > 0
assert float(sr["attempts_per_delivered"]) > 1.0

short_timeout = simulate("--source", "finite", "--packets", "5", "--ws", "1",
                         "--wr", "1", "--pd", "0", "--pa", "0",
                         "--timeout", "1", "--seed", "41")[0]
assert int(short_timeout["timeouts"]) > 0
assert int(short_timeout["data_attempts"]) > 5

same_seed_a = simulate("--source", "finite", "--packets", "20", "--ws", "4",
                       "--wr", "4", "--pd", "0.2", "--pa", "0.1",
                       "--seed", "77")[0]
same_seed_b = simulate("--source", "finite", "--packets", "20", "--ws", "4",
                       "--wr", "4", "--pd", "0.2", "--pa", "0.1",
                       "--seed", "77")[0]
assert same_seed_a == same_seed_b

default_seeds = simulate("--source", "finite", "--packets", "1",
                         "--pd", "0", "--pa", "0")
assert [row["seed"] for row in default_seeds] == [
    "400315188", "400381481", "400385757"
]

rejects("--ws", "4", "--wr", "4", "--seq-space", "7")
rejects("--pd", "1")
rejects("--source", "invalid")

print("Lab 4 ARQ tests passed")
