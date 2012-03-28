//===-- tsan_clock.h --------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of ThreadSanitizer (TSan), a race detector.
//
//===----------------------------------------------------------------------===//
#ifndef TSAN_CLOCK_H
#define TSAN_CLOCK_H

#include "tsan_defs.h"
#include "tsan_slab.h"

namespace __tsan {

// The clock that lives in sync variables (mutexes, atomics, etc).
class SyncClock {
 public:
  static const int kChunkSize = 128;
  SyncClock();
  ~SyncClock();

  void Free(SlabCache *slab);

  int size() const {
    return nclk_;
  }

 private:
  int nclk_;
  struct Chunk;
  Chunk* chunk_;
  friend struct ThreadClock;
};

// The clock that lives in threads.
struct ThreadClock {
 public:
  ThreadClock();

  u64 get(int tid) const {
    DCHECK(tid < kMaxTid);
    return clk_[tid];
  }

  void set(int tid, u64 v) {
    DCHECK(tid < kMaxTid);
    DCHECK(v >= clk_[tid]);
    clk_[tid] = v;
    if (nclk_ <= tid)
      nclk_ = tid + 1;
  }

  void tick(int tid) {
    DCHECK(tid < kMaxTid);
    clk_[tid]++;
    if (nclk_ <= tid)
      nclk_ = tid + 1;
  }

  int size() const {
    return nclk_;
  }

  void acquire(const SyncClock *src);
  void release(SyncClock *dst, SlabCache *slab) const;
  void acq_rel(SyncClock *dst, SlabCache *slab);

 private:
  int nclk_;
  u64 clk_[kMaxTid];
};

}  // namespace __tsan

#endif  // TSAN_CLOCK_H