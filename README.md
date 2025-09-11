# HFT Playpen

A playground that stitches together lock-free primitives and a minimal matching engine for performance experimentation and learning.

## Components
- **Ring Buffer** — lock-free SPSC.
- **Slab Allocator** — fast fixed-size allocator.
- **Lock-free Logger** — background logger that decouples I/O from engine.
- **Custom Binary Protocol** — compact fixed-size messages for deterministic parsing.
- **Order Generator Agent** — synthetic, configurable order generation.
- **Order Parser** — decodes wire messages into internal `OrderRequest` objects.
- **Orderbook & Matching Engine** — price-time priority matching, cancels, partial fills.
- **Trading Pipeline Harness** — wires components into a 4-thread pipeline.

---

## High Level Architecture

- Agent produces fixed-size binary messages (New/Cancel/Modify).
- Parser deserializes into `OrderRequest` and pushes to the parser->engine ring.
- Engine consumes requests, updates orderbook, emits `TradeEvent`.
- Logger consumes trades and writes to file.

---

## Wire format / protocol
A compact fixed-size binary message to keep parsing deterministic and allocation-free.

Example fields in `NewOrderMessage` (packed):
- `msg_type` (enum)
- `msg_length` (uint32)
- `version` (uint8)
- `order_id` (uint64)
- `symbol_id` (uint32)
- `price_ticks` (uint32) — convert via `price = ticks * tick_size`
- `quantity` (uint32)
- `side` (enum)
- `tif` (enum)

See `protocol/messages.h` and `protocol/binary_codec.h` for the exact layout and helpers.

---

## Known TODOs
- Implement `ModifyOrder` end-to-end (currently a stub).
- Ensure timestamp propagation through every stage for precise latency breakdown.
- Add microbench baselines to `benchmarks/` for all components (orderbook, matching engine, slab allocator,logger, ring buffer, parser).  
- Add `emplace()`, `capacity()`, `size()` API to ring buffer
- Slab allocator double-free detection, bounds checking, return empty slabs to OS and add thread-local caches.
- Fix `AvailableQuantityFor` the current implementation is not thread-safe across concurrent modifications.
---

## Component references

### SPSC/MPMC Ring Buffer
* Implementations / references
  * [Charles Frasch — unit tests example (cppcon2023)](https://github.com/CharlesFrasch/cppcon2023/blob/main/unitTests.cpp)
  * [Charles Frasch — microbench example (cppcon2023)](https://github.com/CharlesFrasch/cppcon2023/blob/main/bench.cpp#L15)
  * [rigtorp/SPSCQueue — tests](https://github.com/rigtorp/SPSCQueue/blob/master/src/SPSCQueueTest.cpp)  
  * [rigtorp/SPSCQueue — benchmark](https://github.com/rigtorp/SPSCQueue/blob/master/src/SPSCQueueBenchmark.cpp)  
  * [rigtorp/SPSCQueue (repo)](https://github.com/rigtorp/SPSCQueue/tree/master)
  * [1024cores — Lock-free algorithms / queues](https://www.1024cores.net/home/lock-free-algorithms/queues)
  * [Single Producer Single Consumer Lock-free FIFO From the Ground Up - Charles Frasch - CppCon 2023](https://www.youtube.com/watch?v=K3P_Lmq6pw0)
  * [Beyond Sequential Consistency - Leveraging Atomics for Fun & Profit - Christopher Fretz C++Now 2025](https://www.youtube.com/watch?v=qNs0_kKpcIA)


### HFT / performance talks
  * [When Nanoseconds Matter: Ultrafast Trading Systems in C++ - David Gross - CppCon 2024](https://www.youtube.com/watch?v=sX2nF1fW7kI)  
  * [Trading at light speed: designing low latency systems in C++ - David Gross - Meeting C++ 2022](https://www.youtube.com/watch?v=8uAW5FQtcvE)
  * [CppCon 2017: Carl Cook “When a Microsecond Is an Eternity: High Performance Trading Systems in C++”](https://www.youtube.com/watch?v=NH1Tta7purM)
  

### Slab allocator
* Implementations / references
  * [“Understanding the slab allocator” (kernel doc)](https://www.kernel.org/doc/gorman/html/understand/understand011.html)
  * [Bonwick — original slab paper (PDF)](https://people.eecs.berkeley.edu/~kubitron/cs194-24/hand-outs/bonwick_slab.pdf)
  * [libumem (gburd) — repo](https://github.com/gburd/libumem/tree/master)
  * [libumem — header](https://github.com/gburd/libumem/blob/master/umem.h)  
  * [libumem — core C source](https://github.com/gburd/libumem/blob/master/umem.c)  
  * [libumem — internal header](https://github.com/gburd/libumem/blob/master/umem_impl.h)