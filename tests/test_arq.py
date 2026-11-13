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
    assert len(rows) == 1, result.stdout
    return rows[0]


def rejects(*arguments):
    result = subprocess.run(
        [EXECUTABLE, *arguments],
        capture_output=True,
        text=True,
    )
    assert result.returncode != 0
    assert result.stderr


sw = simulate("--packets", "5", "--data-tx", "1", "--ack-tx", "0",
              "--prop-delay", "0", "--timeout", "4")
assert sw["protocol"] == "stop-and-wait"
assert sw["generated"] == "5"
assert sw["delivered"] == "5"
assert sw["acked"] == "5"
assert sw["sim_time"] == "5.000000000"
assert sw["mean_delay"] == "3.000000000"
assert sw["retransmissions"] == "0"

windowed = simulate("--packets", "4", "--ws", "4", "--wr", "1",
                    "--data-tx", "1", "--ack-tx", "0.1",
                    "--prop-delay", "0.5", "--timeout", "4")
assert windowed["protocol"] == "go-back-n"
assert windowed["sim_time"] == "5.100000000"
assert float(windowed["sim_time"]) < 4 * (
    1.0 + 2 * 0.5 + 0.1
)

gbn = simulate("--packets", "20", "--ws", "4", "--wr", "1",
               "--data-error", "0.08", "--timeout", "2", "--seed", "10")
assert gbn["protocol"] == "go-back-n"
assert gbn["delivered"] == "20"
assert gbn["acked"] == "20"
assert int(gbn["retransmissions"]) > 0

sr = simulate("--packets", "20", "--ws", "4", "--wr", "4",
              "--data-error", "0.08", "--ack-error", "0.05",
              "--timeout", "2", "--seed", "10")
assert sr["protocol"] == "selective-repeat"
assert sr["delivered"] == "20"
assert sr["acked"] == "20"
assert int(sr["data_lost"]) > 0
assert int(sr["ack_lost"]) > 0
assert int(sr["retransmissions"]) > 0

backlogged = simulate("--source", "backlogged", "--duration", "10",
                      "--ws", "3", "--wr", "3", "--data-error", "0",
                      "--ack-error", "0", "--seed", "3")
assert backlogged["protocol"] == "selective-repeat"
assert backlogged["source"] == "backlogged"
assert backlogged["sim_time"] == "10.000000000"
assert int(backlogged["generated"]) >= int(backlogged["delivered"])
assert int(backlogged["delivered"]) > 0

repeat_a = simulate("--packets", "30", "--ws", "3", "--wr", "3",
                    "--data-error", "0.2", "--ack-error", "0.1",
                    "--seed", "77")
repeat_b = simulate("--packets", "30", "--ws", "3", "--wr", "3",
                    "--data-error", "0.2", "--ack-error", "0.1",
                    "--seed", "77")
assert repeat_a == repeat_b

rejects("--ws", "4", "--wr", "4", "--seq-space", "7")
rejects("--data-error", "1")
rejects("--source", "invalid")

print("ARQ tests passed")
