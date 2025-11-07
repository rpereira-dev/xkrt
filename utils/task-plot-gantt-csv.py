#!/usr/bin/env python3
"""
gantt_plot.py

Usage:
    python gantt_plot.py tasks.csv

CSV format expected (colon-separated):
    tid:label:addr:time:state
where 'state' contains values like 'executing' and 'completed'.
"""
import sys
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from collections import deque
import matplotlib.lines as mlines

def main():
    if len(sys.argv) < 2:
        print("Usage: python gantt_plot.py <tasks.csv>")
        sys.exit(1)

    csv_path = sys.argv[1]
    sep = ":"  # CSV separator

    # === LOAD DATA ===
    df = pd.read_csv(csv_path, sep=sep, dtype={"tid": int, "label": str, "addr": str, "time": float, "state": str})
    df["state"] = df["state"].str.lower()
    df = df.sort_values(by=["addr", "time"])

    # === BUILD INTERVALS: pair executing -> completed per addr; use TID of the executing event ===
    records = []  # tuples: (executing_tid, label, addr, start, end)
    for addr, group in df.groupby("addr"):
        # iterate chronologically for this addr
        pending = deque()  # will hold tuples (start_time, tid_of_executing, label)
        for _, row in group.iterrows():
            st = row["state"]
            if st == "executing":
                # record the executing event with its tid and label
                pending.append((row["time"], int(row["tid"]), row["label"]))
            elif st == "completed":
                # pair with earliest pending executing if available
                if pending:
                    start_time, executing_tid, label = pending.popleft()
                    end_time = row["time"]
                    # sanity: if end_time < start_time, skip
                    if end_time >= start_time:
                        records.append((executing_tid, label, addr, start_time, end_time))
                    # else ignore invalid ordering
                else:
                    # no matching executing for this completed — skip
                    continue
            else:
                # ignore other states
                continue

    if not records:
        print("No complete executing→completed intervals found.")
        sys.exit(1)

    intervals = pd.DataFrame(records, columns=["tid", "label", "addr", "start", "end"])

    # === PLOT ===
    fig, ax = plt.subplots(figsize=(12, 6))

    # y axis: one row per thread that executing tasks
    tids = sorted(intervals["tid"].unique())
    tid_to_y = {tid: i for i, tid in enumerate(tids)}
    yticks = list(range(len(tids)))

    # colors per label (cycle if more labels than colormap size)
    labels = sorted(intervals["label"].unique())
    base_cmap = cm.get_cmap("tab20")
    cmap_n = getattr(base_cmap, "N", 20) or 20
    label_to_color = {}
    for i, lbl in enumerate(labels):
        label_to_color[lbl] = base_cmap(i % cmap_n)

    # draw bars
    for _, row in intervals.iterrows():
        y = tid_to_y[row["tid"]]
        ax.barh(
            y=y,
            width=row["end"] - row["start"],
            left=row["start"],
            height=0.6,
            color=label_to_color[row["label"]],
            edgecolor="black",
            align="center",
            alpha=0.9,
        )

    # styling
    ax.set_yticks(yticks)
    ax.set_yticklabels([str(t) for t in tids])
    ax.set_xlabel("Time")
    ax.set_ylabel("Thread ID (tid from 'executing' event)")
    ax.set_title("Gantt: tasks plotted on the thread that executing them (executing→completed)")

    ax.grid(True, axis="x", linestyle="--", alpha=0.4)

    # legend (one color per label)
    handles = [mlines.Line2D([], [], color=label_to_color[l], lw=6, label=l) for l in labels]
    ax.legend(handles=handles, title="Task Labels", bbox_to_anchor=(1.02, 1), loc="upper left")

    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()

