# Frozen v1.0 counterfactual: four independent feedback channels.
# NOT a semantics-preserving rewrite of the one-channel serial workload.
.reg R1 0
.reg R2 1
.reg F0 4.0
.reg F1 4.0
.reg F2 4.0
.reg F3 4.0
.reg F24 0.5
.reg F25 1.0
.reg F26 2.0
.mem 0 2.0
.mem 1 4.0
.mem 2 6.0
.mem 3 8.0
.mem 4 10.0
.mem 5 12.0
.mem 6 14.0
.mem 7 16.0
.mem 8 18.0
.mem 9 20.0
.mem 10 22.0
.mem 11 24.0
.mem 12 26.0
.mem 13 28.0
.mem 14 30.0
.mem 15 32.0

# Sample 1, channel 0.
LD F8, [0]
FADD F9, F0, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F0, F11, F26
IADD R1, R1, R2

# Sample 2, channel 1.
LD F8, [1]
FADD F9, F1, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F1, F11, F26
IADD R1, R1, R2

# Sample 3, channel 2.
LD F8, [2]
FADD F9, F2, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F2, F11, F26
IADD R1, R1, R2

# Sample 4, channel 3.
LD F8, [3]
FADD F9, F3, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F3, F11, F26
IADD R1, R1, R2

# Sample 5, channel 0.
LD F8, [4]
FADD F9, F0, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F0, F11, F26
IADD R1, R1, R2

# Sample 6, channel 1.
LD F8, [5]
FADD F9, F1, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F1, F11, F26
IADD R1, R1, R2

# Sample 7, channel 2.
LD F8, [6]
FADD F9, F2, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F2, F11, F26
IADD R1, R1, R2

# Sample 8, channel 3.
LD F8, [7]
FADD F9, F3, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F3, F11, F26
IADD R1, R1, R2

# Sample 9, channel 0.
LD F8, [8]
FADD F9, F0, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F0, F11, F26
IADD R1, R1, R2

# Sample 10, channel 1.
LD F8, [9]
FADD F9, F1, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F1, F11, F26
IADD R1, R1, R2

# Sample 11, channel 2.
LD F8, [10]
FADD F9, F2, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F2, F11, F26
IADD R1, R1, R2

# Sample 12, channel 3.
LD F8, [11]
FADD F9, F3, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F3, F11, F26
IADD R1, R1, R2

# Sample 13, channel 0.
LD F8, [12]
FADD F9, F0, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F0, F11, F26
IADD R1, R1, R2

# Sample 14, channel 1.
LD F8, [13]
FADD F9, F1, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F1, F11, F26
IADD R1, R1, R2

# Sample 15, channel 2.
LD F8, [14]
FADD F9, F2, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F2, F11, F26
IADD R1, R1, R2

# Sample 16, channel 3.
LD F8, [15]
FADD F9, F3, F8
FMUL F10, F9, F24
FADD F11, F10, F25
FDIV F3, F11, F26
IADD R1, R1, R2
