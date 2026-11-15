# Lab 4: Simulation of ARQ Protocols

This project adapts the Lab 3 discrete-event simulator to one source, one
destination, and one point-to-point link. It retains the Lab 3 event scheduler,
random-number generator, packet-duration module, and source-file organization.
The station contention, collision, and backoff model is replaced by Stop-and-
Wait, Go-Back-N, and Selective-Repeat ARQ.

## Preparation defaults

Simulation time is measured in milliseconds. The default packet length is
1000 bits, the link rate is 1 Mbps, and the resulting data-packet transmission
time is 1 ms. The round-trip propagation delay is 10 ms (5 ms per direction);
the default timeout is 15 ms. ACK transmission and processing times are
neglected. Data-packet and ACK errors are independently sampled for every
transmission attempt.

The sender/receiver windows select the protocol:

- `Ws=1, Wr=1`: Stop-and-Wait
- `Wr=1`: Go-Back-N
- `Wr>1`: Selective-Repeat

For Go-Back-N, a future out-of-sequence packet is discarded without an ACK;
a duplicate of a previously accepted packet is re-ACKed to recover a lost ACK.
Stop-and-Wait uses the same duplicate-ACK recovery. Selective-Repeat buffers
in-window out-of-order packets, ACKs them individually, and retransmits timed-out
packets individually. The lecture's optional NAK mechanism is not modeled; the
lab experiments specify data-packet and ACK errors with timeout recovery.

The configured sequence-number space must satisfy `Ws + Wr <= M`. Packets are
tracked internally by absolute IDs and their displayed sequence numbers are
`ID mod M`; receiver decisions use the absolute IDs. This avoids ambiguity from
sequence-number wraparound in this model.

## Build and verification

```sh
make
make test
```

`make` builds `app`. The tests check error-free timing, all three protocol
mappings, recovery from data and ACK errors, timeout behavior, repeatability
with a fixed seed, and parameter validation.

To print short event traces for the verification experiment:

```sh
make app_trace
./app_trace --source finite --packets 4 --pd 0.5 --pa 0.5 --seed 400315188
```

The trace build prints event details as well as the final CSV result.

## Running experiments

Use `./app --help` for all options. Each invocation prints a CSV header and
one result row per configured random seed. By default, the three Lab 3 seeds
are reused for each run, so protocol and parameter comparisons use common
seeds. `--seed N` runs one selected seed. Confirm that at least one seed in
`RANDOM_SEED_LIST` is a group member's student ID, as required by the lab.

Examples:

```sh
# Continuously backlogged, error-free GBN
./app --source backlogged --runlength 1000000 --ws 12 --wr 1 --pd 0 --pa 0

# Selective-Repeat with several receiver windows
./app --source backlogged --runlength 1000000 --ws 16 --wr 4 --pd 0.05 --pa 0

# Finite transfer of 100 packets
./app --source finite --packets 100 --ws 8 --wr 4 --pd 0.05 --pa 0.05

# Change the timeout (milliseconds)
./app --source backlogged --ws 16 --wr 4 --pd 0.05 --pa 0.02 --timeout 25
```

Each experiment can be run as repeated invocations with one parameter changed;
the CSV rows can then be combined for plotting. Use the same seed list for all
protocol comparisons. For a finite transfer, the simulation ends when the
sender has received ACKs for all packets. For a backlogged source, it ends when
`RUNLENGTH` packets have been delivered in order.

The CSV reports delivered-packet link efficiency
`delivered * 1 ms / simulation_time`, the average transmission attempts per
delivered packet, mean packet delay, packet/ACK errors, timeout count, and
transfer/run timing. Finite-source packets are present at time zero; backlogged
packets are created as sender-window slots become available. Timeout counting
begins when a data-packet transmission completes.
