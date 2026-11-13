# Lab 4: ARQ discrete-event simulator

This project uses the bundled `simlib` event scheduler to model one source and
one destination over a point-to-point link. The generic sender/receiver-window
implementation selects:

- Stop-and-Wait when `Ws=1, Wr=1`
- Go-Back-N when `Wr=1`
- Selective-Repeat when `Wr>1`

The sequence-number space must satisfy `Ws + Wr <= sequence_space`.

The source is divided into Lab 3-style modules: `main.c` handles configuration
and run control, `packet_transmission.c` handles data/ACK transmission and
timeouts, `packet_arrival.c` handles receiver and ACK-arrival processing,
`output.c` formats results, and `cleanup.c` releases simulation state.

## Build and test

```sh
make
make test
```

The program writes one CSV header and one result row to standard output. Invalid
arguments are reported on standard error.

## Running experiments

Finite-source example:

```sh
./arq_sim --source finite --packets 1000 --ws 4 --wr 4 \
  --data-error 0.01 --ack-error 0.005 --timeout 5 --seed 41
```

Continuously backlogged example:

```sh
./arq_sim --source backlogged --duration 10000 --ws 8 --wr 1 \
  --data-error 0.01 --ack-error 0.005 --timeout 5 --seed 41
```

The complete set of arguments is printed by `./arq_sim --help`. Re-run with
different seeds or parameter values to compare performance.

## Model and assumptions

- Data transmissions are serialized in the source-to-destination direction;
  ACK transmissions are serialized independently in the reverse direction
  (full duplex). Both directions use the same one-way propagation delay.
- Data and ACK transmission times, propagation delay, error probabilities, and
  timeout are configurable. Each transmission attempt is independently erased
  with its direction's configured probability; corruption is treated as loss.
- The source is initially backlogged. Finite mode has exactly `--packets`
  packets available from time zero and ends when all have been acknowledged.
  Backlogged mode supplies packets whenever a sender-window slot opens and
  runs for the configured duration; its reported delivery count includes only
  packets delivered by the receiver before the duration expires.
- In-order delivery is counted at the destination. Selective Repeat buffers
  in-window out-of-order frames and acknowledges them individually. Go-Back-N
  discards out-of-order frames and sends cumulative ACKs.
- Timeout countdown begins at the end of a data-frame transmission. Stop-and-
  Wait and Go-Back-N use one timer for the oldest unacknowledged packet;
  Selective-Repeat uses an individual timer per outstanding packet.
- Throughput is delivered packets divided by simulated time. Mean delay is
  measured from packet creation at the sender to in-order delivery at the
  receiver. For the backlogged source, packet creation occurs when the packet
  enters the sender's active window.
