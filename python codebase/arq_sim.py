"""ARQ simulator for COE4DK4 Lab 4: Stop-and-Wait, Go-Back-N and Selective Repeat in ONE event loop.
Times are in ms. receive_window = 1 gives GBN, send_window = receive_window = 1 gives S&W. No sequence wrap-around.
Examples (curves drawn: S&W, GBN, and one SR curve per --receive_windows value):
  python arq_sim.py --trace --run_length_ms 40 --send_window 4 --receive_windows 3 --forced_losses 2:1
  python arq_sim.py --sweep send_window --values 1 2 4 8 11 14 --plot fig.png
"""
import argparse, heapq, random

# Event types. At equal times the smaller number happens first: tx ends, data arrives, ACK arrives, timeout.
TX_END, DATA_ARRIVAL, ACK_ARRIVAL, TIMEOUT, START = 0, 1, 2, 3, 4
METRIC_LABELS = dict(efficiency="Link efficiency", elapsed_ms="Mean transfer completion time (ms)",
                     attempts_per_packet="Avg. transmission attempts per delivered packet")
SWEEP_LABELS = dict(send_window="Send window size, Ws (packets)",
                    data_error_prob="Data-packet error probability, pd",
                    ack_error_prob="ACK error probability, pa",
                    timeout_ms="Retransmission timeout, Tout (ms)",
                    num_packets="Transfer size, N (packets)")


def simulate(send_window=11, receive_window=1, data_error_prob=0.0, ack_error_prob=0.0,
             num_packets=None, timeout_ms=15.0, packet_time_ms=1.0, propagation_delay_ms=5.0,
             seed=1, run_length_ms=5e4, forced_losses=(), trace=False):
    """num_packets=None: backlogged source, run for run_length_ms. num_packets=int: finite transfer, run until
    all are ACKed. forced_losses: strings like "2:1" = the 1st transmission attempt of seq 2 is lost."""
    random_generator = random.Random(seed)
    event_queue = [(0.0, START, 0, 0)]            # events are (time, type, seq, detail); START kicks things off
    is_go_back_n = (receive_window == 1)
    oldest_unacked = next_new_seq = 0             # sender: start of send window / next never-sent seq
    acked_above_oldest, retransmit_queue = set(), []   # sender: seqs ACKed individually above oldest / resend list
    last_tx_end_time, tx_count_per_seq = {}, {}   # per seq: end of latest transmission (validates timers), #sends
    link_busy, total_transmissions = False, 0
    next_expected, out_of_order_buffer = 0, set() # receiver: next in-order seq, packets buffered ahead of it

    while event_queue:
        now, event_type, seq, detail = heapq.heappop(event_queue)
        if num_packets is None and now > run_length_ms:
            break
        if event_type == TX_END:                            # sender finished transmitting packet seq
            link_busy = False
            total_transmissions += 1
            last_tx_end_time[seq] = now
            attempt = tx_count_per_seq[seq] = tx_count_per_seq.get(seq, 0) + 1
            data_lost = f"{seq}:{attempt}" in forced_losses or random_generator.random() < data_error_prob
            if trace: print(f"{now:8.3f}  end tx seq {seq} (attempt {attempt}): {'LOST' if data_lost else 'ok'}")
            if not data_lost:
                heapq.heappush(event_queue, (now + propagation_delay_ms, DATA_ARRIVAL, seq, 0))
            heapq.heappush(event_queue, (now + timeout_ms, TIMEOUT, seq, now))   # timer starts after EVERY send
        elif event_type == DATA_ARRIVAL:                    # data packet reaches the receiver
            if seq >= next_expected + receive_window:
                if trace: print(f"{now:8.3f}  rx seq {seq}: outside receive window, discarded, no ACK")
            else:
                is_duplicate = seq < next_expected
                if not is_duplicate: out_of_order_buffer.add(seq)
                while next_expected in out_of_order_buffer:  # deliver in-order packets, slide the window
                    out_of_order_buffer.remove(next_expected)
                    next_expected += 1
                ack_sent = random_generator.random() >= ack_error_prob
                if trace: print(f"{now:8.3f}  rx seq {seq} ({'duplicate' if is_duplicate else 'accepted'}), "
                                f"next_expected={next_expected}, ACK N(R)={next_expected} {'sent' if ack_sent else 'LOST'}")
                if ack_sent:
                    heapq.heappush(event_queue, (now + propagation_delay_ms, ACK_ARRIVAL, seq, next_expected))
        elif event_type == ACK_ARRIVAL:                     # ACK(seq, N(R)) reaches the sender; detail is N(R)
            oldest_unacked = max(oldest_unacked, detail)    # cumulative: everything below N(R) arrived
            if seq >= oldest_unacked: acked_above_oldest.add(seq)   # selective: this packet itself arrived
            while oldest_unacked in acked_above_oldest:     # slide the send window
                acked_above_oldest.remove(oldest_unacked)
                oldest_unacked += 1
            if trace: print(f"{now:8.3f}  ACK(seq {seq}, N(R)={detail}) arrives, oldest_unacked={oldest_unacked}")
            if num_packets is not None and oldest_unacked >= num_packets:
                break                                       # finite transfer complete
        elif event_type == TIMEOUT:                         # detail is when the timed transmission ended
            timer_is_stale = last_tx_end_time.get(seq) != detail or seq < oldest_unacked or seq in acked_above_oldest
            if not timer_is_stale:
                if trace: print(f"{now:8.3f}  TIMEOUT seq {seq}: " + ("go back" if is_go_back_n else "resend only this packet"))
                if is_go_back_n:                            # GBN: resend everything outstanding
                    retransmit_queue[:] = range(oldest_unacked, next_new_seq)
                    for pending_seq in retransmit_queue:
                        last_tx_end_time.pop(pending_seq, None)   # their old timers are now void
                else:                                       # SR: resend only the timed-out packet
                    retransmit_queue.append(seq)

        # Start sending if the link is idle and there is something allowed to send
        while retransmit_queue and (retransmit_queue[0] < oldest_unacked or retransmit_queue[0] in acked_above_oldest):
            retransmit_queue.pop(0)                         # drop queued resends that were ACKed in the meantime
        can_send_new_packet = next_new_seq < oldest_unacked + send_window and (num_packets is None or next_new_seq < num_packets)
        if not link_busy and (retransmit_queue or can_send_new_packet):
            if retransmit_queue:
                seq_to_send = retransmit_queue.pop(0)
            else:
                seq_to_send, next_new_seq = next_new_seq, next_new_seq + 1
            link_busy = True
            if trace: print(f"{now:8.3f}  start tx seq {seq_to_send}")
            heapq.heappush(event_queue, (now + packet_time_ms, TX_END, seq_to_send, 0))

    elapsed_ms = now if num_packets is not None else run_length_ms
    return dict(efficiency=next_expected * packet_time_ms / elapsed_ms, elapsed_ms=elapsed_ms,
                attempts_per_packet=total_transmissions / max(next_expected, 1))


def run_averaged(seeds, **params):                          # run once per seed, then average every metric
    all_results = [simulate(seed=seed, **params) for seed in seeds]
    return {metric: sum(result[metric] for result in all_results) / len(all_results) for metric in all_results[0]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--send_window", type=int, default=11, help="send window (x-axis if --sweep send_window)")
    parser.add_argument("--receive_windows", type=int, nargs="+", default=[3, 6, 11], help="one SR curve per value")
    parser.add_argument("--data_error_prob", type=float, default=0.0, help="data packet error probability")
    parser.add_argument("--ack_error_prob", type=float, default=0.0, help="ACK error probability")
    parser.add_argument("--timeout_ms", type=float, default=15.0, help="retransmission timeout (ms)")
    parser.add_argument("--num_packets", type=int, help="finite transfer size (default: backlogged source)")
    parser.add_argument("--run_length_ms", type=float, default=5e4, help="run length (backlogged source only)")
    parser.add_argument("--seeds", type=int, nargs="+", default=[1, 2, 3, 4, 5], help="put a student ID in here")
    parser.add_argument("--sweep", choices=["send_window", "data_error_prob", "ack_error_prob", "timeout_ms",
                                            "num_packets"], help="parameter on the x-axis (default: none, one point)")
    parser.add_argument("--values", type=float, nargs="+", help="x-axis values for --sweep")
    parser.add_argument("--forced_losses", nargs="*", default=[], help="e.g. 2:1 = 1st attempt of seq 2 is lost")
    parser.add_argument("--trace", action="store_true", help="print the event trace (use a short run)")
    parser.add_argument("--plot", help="save plots as <metric>_<this name>, e.g. fig.png")
    args = parser.parse_args()

    fixed_params = dict(send_window=args.send_window, data_error_prob=args.data_error_prob,
                        ack_error_prob=args.ack_error_prob, timeout_ms=args.timeout_ms,
                        num_packets=args.num_packets, run_length_ms=args.run_length_ms, forced_losses=args.forced_losses)
    curves = {"S&W": dict(send_window=1, receive_window=1), "GBN": dict(receive_window=1),
              **{f"SR Wr={w}": dict(receive_window=w) for w in args.receive_windows}}
    if args.trace:
        for curve_name, curve_params in curves.items():
            print(f"\n===== {curve_name} =====")
            simulate(**{**fixed_params, **curve_params}, seed=args.seeds[0], trace=True)
        raise SystemExit

    sweep_name = args.sweep or "send_window"                # with no --sweep, run one point at the given send_window
    x_values = [int(v) if sweep_name in ("send_window", "num_packets") else v for v in args.values or [args.send_window]]
    results = {name: [run_averaged(args.seeds, **{**fixed_params, sweep_name: x_value, **curve_params})   # curve wins
                      for x_value in x_values] for name, curve_params in curves.items()}
    for metric in (["elapsed_ms"] if args.num_packets or sweep_name == "num_packets" else ["efficiency", "attempts_per_packet"]):
        print(f"\n{METRIC_LABELS[metric]}   (x = {sweep_name}, {len(args.seeds)} seeds)")
        print(f"{'x':<8}" + "".join(f"{name:<12}" for name in curves))
        for i, x_value in enumerate(x_values):
            print(f"{x_value:<8}" + "".join(f"{results[name][i][metric]:<12.4f}" for name in curves))
        if args.plot:
            import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
            fig, ax = plt.subplots()
            for i, name in enumerate(curves):
                ax.plot(x_values, [result[metric] for result in results[name]],
                        marker="osD^vx+*"[i % 8], linestyle="-", label=name)
            ax.set_xlabel(SWEEP_LABELS[sweep_name])
            ax.set_ylabel(METRIC_LABELS[metric])
            ax.grid(True)
            ax.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))
            fig.savefig(f"{metric}_{args.plot}", dpi=150, bbox_inches="tight")
            plt.close(fig)
