# HFT Playpen

High-frequency trading components playground for performance experimentation.

## Components
- Ring Buffer: TBD

seq_cst(no cpu pin):
-----------------------------------------------------------------------------
Benchmark                   Time             CPU   Iterations UserCounters...
-----------------------------------------------------------------------------
BM_SPSC_RingBuffer        134 ns          134 ns     11200000 ops/s=7.46667M/s

relaxed/acq/rel(no cpu pin):
-----------------------------------------------------------------------------
Benchmark                   Time             CPU   Iterations UserCounters...
-----------------------------------------------------------------------------
BM_SPSC_RingBuffer       55.1 ns         53.0 ns     11200000 ops/s=18.8632M/s

relaxed/acq/rel(pinned):
-----------------------------------------------------------------------------
Benchmark                   Time             CPU   Iterations UserCounters...
-----------------------------------------------------------------------------
BM_SPSC_RingBuffer       22.0 ns         22.5 ns     32000000 ops/s=44.5217M/s