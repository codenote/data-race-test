//===-- tsan_mutex.cc -------------------------------------------*- C++ -*-===//
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
#include "tsan_mutex.h"
#include "tsan_platform.h"

namespace __tsan {

// Simple reader-writer spin-mutex. Optimized for not-so-contended case.
// Readers have preference, can possibly starvate writers.

const uptr kUnlocked = 0;
const uptr kWriteLock = 1;
const uptr kReadLock = 2;

class Backoff {
 public:
  Backoff()
    : iter_() {
  }
  bool Do() {
    if (iter_++ < kActiveSpinIters)
      proc_yield(kActiveSpinCnt);
    else
      sched_yield();
    return true;
  }
 private:
  int iter_;
  static const int kActiveSpinIters = 10;
  static const int kActiveSpinCnt = 20;
};

Mutex::Mutex() {
  atomic_store(&state_, kUnlocked, memory_order_relaxed);
}

Mutex::~Mutex() {
  CHECK_EQ(atomic_load(&state_, memory_order_relaxed), kUnlocked);
}

void Mutex::Lock() {
  uptr cmp = kUnlocked;
  if (atomic_compare_exchange_strong(&state_, &cmp, kWriteLock,
                                     memory_order_acquire))
    return;
  for (Backoff backoff; backoff.Do();) {
    if (atomic_load(&state_, memory_order_relaxed) == kUnlocked) {
      cmp = kUnlocked;
      if (atomic_compare_exchange_weak(&state_, &cmp, kWriteLock,
                                       memory_order_acquire))
        return;
    }
  }
}

void Mutex::Unlock() {
  uptr prev = atomic_fetch_sub(&state_, kWriteLock, memory_order_release);
  (void)prev;
  DCHECK_NE(prev & kWriteLock, 0);
}

void Mutex::ReadLock() {
  uptr prev = atomic_fetch_add(&state_, kReadLock, memory_order_acquire);
  if ((prev & kWriteLock) == 0)
    return;
  for (Backoff backoff; backoff.Do();) {
    prev = atomic_load(&state_, memory_order_acquire);
    if ((prev & kWriteLock) == 0)
      return;
  }
}

void Mutex::ReadUnlock() {
  uptr prev = atomic_fetch_sub(&state_, kReadLock, memory_order_release);
  (void)prev;
  DCHECK_EQ(prev & kWriteLock, 0);
  DCHECK_GT(prev & ~kWriteLock, 0);
}

Lock::Lock(Mutex *m)
  : m_(m) {
  m_->Lock();
}

Lock::~Lock() {
  m_->Unlock();
}

ReadLock::ReadLock(Mutex *m)
  : m_(m) {
  m_->ReadLock();
}

ReadLock::~ReadLock() {
  m_->ReadUnlock();
}

}  // namespace __tsan