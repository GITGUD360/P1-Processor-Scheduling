# One small guided test variant

Copy `tests/cases/load_admission.asm` to `tests/student_cases/my_variant.asm`.
Change only `.mem 0 7.5` to `.mem 0 10.0` in your copy. Do not edit the public test.
Predict which final register changes and whether instruction timing should change.
Then, after completing the decisions, run:

```sh
./build/sim tomasulo tests/student_cases/my_variant.asm config/baseline.cfg results/variant --trace
python3 tools/show_run.py results/variant
```

Compare its registers and timing with `results/check/load_admission_tomasulo_*`.
The standalone command is marked UNVERIFIED because this custom input has no bundled
reference signature. Explain your predicted comparison; do not relabel it as a
reference-verified baseline. Include the small assembly file and variant output.
