"""ARQ simulator for COE4DK4 Lab 4: Stop-and-Wait, Go-Back-N and Selective Repeat in ONE event loop.
Times are in ms. Wr = 1 gives GBN, Ws = Wr = 1 gives S&W. Sequence numbers never wrap around.

Examples (curves drawn: S&W, GBN, and one SR curve per --Wr value):
  python arq_sim.py --trace --T 40 --Ws 4 --Wr 3 --pd 0.5 --pa 0.5     # event traces (Exp 1)
  python arq_sim.py --trace --T 40 --Ws 4 --Wr 3 --drop 2:1           # force 1st attempt of seq 2 to be lost
  python arq_sim.py --sweep Ws --values 1 2 4 8 11 14 --plot fig.png   # efficiency vs Ws (Exp 2)
"""
import argparse, heapq, random
from itertools import count

PRIO = dict(end=0, arr=1, ack=2, to=3)   # order of events at equal times: tx end, data arrival, ACK arrival, timeout
LABEL = dict(eff="Link efficiency", att="Avg. transmission attempts per delivered packet",
             time="Mean transfer completion time (ms)")


def simulate(Ws=11, Wr=1, pd=0.05, pa=0.0, N=None, Tout=15.0, Tf=1.0, prop=5.0,
             seed=1, T=5e4, drop=(), trace=False):
    """N=None: backlogged source, run for T ms.  N=int: finite transfer, run until all N are ACKed.
    drop: set of (seq, attempt) data transmissions forced to fail (for hand-checkable traces)."""
    rng, ev, tick = random.Random(seed), [], count()
    log = print if trace else (lambda *_: None)

    def at(t, kind, *a):                        # schedule an event
        heapq.heappush(ev, (t, PRIO[kind], next(tick), kind, a))

    base = nxt = 0                  # sender: oldest un-ACKed seq, next new seq
    acked, retx = set(), []         # sender: seqs ACKed above base, retransmission queue
    last_end, tries = {}, {}        # seq -> end time of its latest transmission (validates timers), #transmissions
    busy, tx = False, 0             # link busy?, total transmission attempts
    rnext, buf = 0, set()           # receiver: next expected seq, buffered out-of-order seqs
    t = 0.0

    def done(s): return s < base or s in acked

    def send():                     # start a transmission if the link is idle and the window allows it
        nonlocal busy, nxt
        while retx and done(retx[0]): retx.pop(0)
        if busy: return
        if retx: s = retx.pop(0)
        elif nxt < base + Ws and (N is None or nxt < N): s, nxt = nxt, nxt + 1
        else: return
        busy = True
        log(f"{t:8.3f}  start tx seq {s}")
        at(t + Tf, "end", s)

    send()
    while ev:
        t, _, _, kind, a = heapq.heappop(ev)
        if N is None and t > T: break
        if kind == "end":                                   # sender finished transmitting seq s
            s, = a
            busy, tx, last_end[s] = False, tx + 1, t
            tries[s] = tries.get(s, 0) + 1
            lost = (s, tries[s]) in drop or rng.random() < pd
            log(f"{t:8.3f}  end tx seq {s} (attempt {tries[s]}): {'LOST' if lost else 'ok'}")
            if not lost: at(t + prop, "arr", s)
            at(t + Tout, "to", s, t)                        # timer starts at the end of EVERY transmission
        elif kind == "arr":                                 # data packet reaches the receiver
            s, = a
            if s >= rnext + Wr:
                log(f"{t:8.3f}  rx seq {s}: outside receive window, discarded, no ACK")
            else:
                dup = s < rnext
                if not dup: buf.add(s)
                while rnext in buf: buf.remove(rnext); rnext += 1
                ok = rng.random() >= pa
                log(f"{t:8.3f}  rx seq {s} ({'duplicate' if dup else 'accepted'}), Rnext={rnext}, "
                    f"ACK N(R)={rnext} {'sent' if ok else 'LOST'}")
                if ok: at(t + prop, "ack", s, rnext)
        elif kind == "ack":                                 # ACK(seq, N(R)) reaches the sender
            s, r = a
            base = max(base, r)                             # cumulative: everything below N(R) is delivered
            if s >= base: acked.add(s)                      # selective: seq s itself is delivered
            while base in acked: acked.remove(base); base += 1
            log(f"{t:8.3f}  ACK(seq {s}, N(R)={r}) arrives, base={base}")
            if N is not None and base >= N: break           # finite transfer complete
        else:                                               # timeout for the transmission that ended at te
            s, te = a
            if last_end.get(s) == te and not done(s):       # ignore timers of ACKed or re-sent packets
                log(f"{t:8.3f}  TIMEOUT seq {s}: " + ("go back to base" if Wr == 1 else "resend only this packet"))
                if Wr == 1:                                 # GBN: resend everything outstanding
                    retx[:] = range(base, nxt)
                    for x in retx: last_end.pop(x, None)    # their old timers are now void
                else: retx.append(s)                        # SR: resend only s
        send()
    elapsed = t if N is not None else T
    return dict(eff=rnext * Tf / elapsed, att=tx / max(rnext, 1), time=elapsed)


def run(seeds, **kw):                                       # average over seeds
    rs = [simulate(seed=s, **kw) for s in seeds]
    return {k: sum(r[k] for r in rs) / len(rs) for k in rs[0]}


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--Ws", type=int, default=11, help="send window (ignored for S&W; x-axis if --sweep Ws)")
    ap.add_argument("--Wr", type=int, nargs="+", default=[3, 6, 11], help="one SR curve per value")
    ap.add_argument("--pd", type=float, default=0.0, help="data packet error prob")
    ap.add_argument("--pa", type=float, default=0.0, help="ACK error prob")
    ap.add_argument("--Tout", type=float, default=15.0, help="retransmission timeout (ms)")
    ap.add_argument("--N", type=int, help="finite transfer of N packets (default: backlogged source)")
    ap.add_argument("--T", type=float, default=5e4, help="run length in ms (backlogged source only)")
    ap.add_argument("--seeds", type=int, nargs="+", default=[1, 2, 3, 4, 5], help="put a student ID in here")
    ap.add_argument("--sweep", choices=["Ws", "pd", "pa", "Tout", "N"], help="parameter on the x-axis")
    ap.add_argument("--values", type=float, nargs="+", help="x-axis values for --sweep")
    ap.add_argument("--drop", nargs="*", default=[], help="force data loss, e.g. 2:1 = 1st attempt of seq 2")
    ap.add_argument("--trace", action="store_true", help="print the event trace (use a short --T or --N)")
    ap.add_argument("--plot", help="save plots as <metric>_<this name>, e.g. fig.png")
    a = ap.parse_args()

    base = dict(Ws=a.Ws, pd=a.pd, pa=a.pa, Tout=a.Tout, N=a.N, T=a.T,
                drop={tuple(map(int, d.split(":"))) for d in a.drop})
    curves = {"S&W": dict(Ws=1, Wr=1), "GBN": dict(Wr=1), **{f"SR Wr={w}": dict(Ws=w, Wr=w) for w in a.Wr}}

    if a.trace:
        for name, kw in curves.items():
            print(f"\n===== {name} =====")
            simulate(**{**base, **kw}, seed=a.seeds[0], trace=True)
        raise SystemExit

    xs = [int(v) if a.sweep in ("Ws", "N") else v for v in a.values] if a.sweep else [None]
    res = {c: [run(a.seeds, **{**base, **({a.sweep: x} if a.sweep else {}), **kw}) for x in xs]
           for c, kw in curves.items()}
    for m in (["time"] if a.N or a.sweep == "N" else ["eff", "att"]):
        print(f"\n{LABEL[m]}   (x = {a.sweep}, {len(a.seeds)} seeds)")
        print(f"{'x':<8}" + "".join(f"{c:<12}" for c in curves))
        for i, x in enumerate(xs):
            print(f"{str(x):<8}" + "".join(f"{res[c][i][m]:<12.4f}" for c in curves))
        if a.plot:
            import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
            fig, ax = plt.subplots()
            for i, c in enumerate(curves):
                ax.plot(xs, [r[m] for r in res[c]], marker="osD^vx+*"[i % 8],
                        linestyle="-", label=c)
            ax.set_xlabel(a.sweep)
            ax.set_ylabel(LABEL[m])
            ax.grid(True)
            ax.legend(loc="center left", bbox_to_anchor=(1.02, 0.5))
            fig.savefig(f"{m}_{a.plot}", dpi=150, bbox_inches="tight")
            plt.close(fig)
