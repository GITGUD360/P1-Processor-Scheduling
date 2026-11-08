# Project 1: Processor Scheduling, Performance, and Energy Trade-offs

## Start here: the unedited package runs

Use a C++17 compiler (`c++`/`g++`), `make`, and Python 3.9 or later. No pip packages,
threads, GUI, network access or external datasets are required. Use your configured
Linux/WSL or macOS terminal. Run commands from this folder, where Makefile is located.

```sh
make demo
```

This builds the programs and runs all three models on 96 instructions. **The initial
outputs are UNVERIFIED and do NOT conform to the target model.** Even `Functional oracle: PASS` does not validate timing or energy. Save the before-run screenshot S1.

## Your work

Read `docs/Project\_Description.pdf` and `docs/Guided\_Implementation.md`.
Edit only six marked decisions in:

* `src/inorder\_control.cpp` (G1)
* `src/scoreboard\_control.cpp` (G2-G3)
* `src/tomasulo\_control.cpp` (G4-G6)

Arbitration, state updates, clock, FU operations, accounting and output are provided.
Suggested order: G1; G2-G3 together; G5-G6; then G4. The Tomasulo admission starter
serializes work and may mask its other faults; the local tests expose each separately.

```sh
make check MODEL=inorder        # Optional focused debugging
make check MODEL=scoreboard
make decisions-test MODEL=tomasulo
make check                     # Full validation
make evaluate                  # 12 records + new validation + provenance
```

A failed check returns a nonzero exit code. This is expected before repairs, not a
build-system failure. `make evaluate` still saves completed UNVERIFIED runs and prints
its table before returning a failing status. Contract violations can stop individual
runs. No fallback solution silently repairs your decisions.

## Inspect your own run

```sh
python3 tools/show\_run.py results/check/war\_scoreboard
python3 tools/show\_run.py results/check/load\_admission\_tomasulo
python3 tools/show\_run.py results/evaluation/iot\_serial\_inorder\_s1 --last 12
```

`\_timing.csv` shows each instruction; `\_energy.csv` gives hardware-event counts;
`\_trace.csv` shows cycle snapshots where requested. Counts, not C++ runtime, drive cost.
Console logs are saved automatically; screenshots must be taken from your own terminal.

## Submit

Put your final report in `report/` and screenshots in `evidence/`. Keep CSV/JSON/log files generated under `results/`.

Never change protected framework/configuration/tests to force a result or PASS flag.

## 

