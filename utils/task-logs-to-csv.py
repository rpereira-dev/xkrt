#!/usr/bin/env python3
import sys
import re
import pandas as pd

def main():
    if len(sys.argv) < 3:
        print("Usage: python parse_log_to_csv.py <input_log.txt> <output.csv>")
        sys.exit(1)

    log_path = sys.argv[1]
    csv_path = sys.argv[2]

    # Regex pattern to extract time, TID, label, addr, and state
    pattern = re.compile(
        r"\[(?P<time>[0-9]+\.[0-9]+)\]\s+\[TID=(?P<tid>\d+)\].*?task\s+`(?P<label>[^`]+)`\s+of addr\s+`(?P<addr>0x[0-9a-fA-F]+)`\s+is now in state\s+`(?P<state>\w+)`",
        re.IGNORECASE,
    )

    records = []
    with open(log_path, "r", encoding="utf-8") as f:
        for line in f:
            m = pattern.search(line)
            if not m:
                continue
            time = float(m.group("time"))
            tid = int(m.group("tid"))
            label = m.group("label")
            addr = m.group("addr")
            state = m.group("state").lower()
            records.append((tid, label, addr, time, state))

    if not records:
        print("No matching task entries found.")
        sys.exit(1)

    # Create dataframe and save as colon-separated CSV
    df = pd.DataFrame(records, columns=["tid", "label", "addr", "time", "state"])
    df.to_csv(csv_path, sep=":", index=False)
    print(f"✅ Wrote {len(df)} entries to {csv_path}")

if __name__ == "__main__":
    main()

