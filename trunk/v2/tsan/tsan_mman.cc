//===-- tsan_mman.cc --------------------------------------------*- C++ -*-===//
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
#include "tsan_mman.h"
#include "tsan_allocator.h"
#include "tsan_rtl.h"

namespace __tsan {

void *user_alloc(ThreadState *thr, uptr pc, uptr sz) {
  CHECK_GT(thr->in_rtl, 0);
  MBlock *b = (MBlock*)Alloc(sz + sizeof(MBlock));
  b->size = sz;
  void *p = b + 1;
  if (CTX() && CTX()->initialized) {
    MemoryResetRange(thr, pc, (uptr)p, sz);
  }
  DPrintf("#%d: alloc(%lu) = %p\n", thr->tid, sz, p);
  return p;
}

void user_free(ThreadState *thr, uptr pc, void *p) {
  CHECK_GT(thr->in_rtl, 0);
  CHECK_NE(p, (void*)0);
  DPrintf("#%d: free(%p)\n", thr->tid, p);
  MBlock *b = user_mblock(thr, p);
  p = b + 1;
  if (CTX() && CTX()->initialized && thr->in_rtl == 1) {
    MemoryRangeFreed(thr, pc, (uptr)p, b->size);
  }
  Free(b);
}

void *user_realloc(ThreadState *thr, uptr pc, void *p, uptr sz) {
  CHECK_GT(thr->in_rtl, 0);
  void *p2 = 0;
  // FIXME: Handle "shrinking" more efficiently,
  // it seems that some software actually does this.
  if (sz) {
    p2 = user_alloc(thr, pc, sz);
    if (p) {
      MBlock *b = user_mblock(thr, p);
      internal_memcpy(p2, p, min(b->size, sz));
    }
  }
  if (p) {
    user_free(thr, pc, p);
  }
  return p2;
}

void *user_alloc_aligned(ThreadState *thr, uptr pc, uptr sz, uptr align) {
  CHECK_GT(thr->in_rtl, 0);
  void *p = user_alloc(thr, pc, sz + align);
  void *pa = (void*)(((uptr)p + align - 1) & ~(align - 1));
  DCHECK_LE((uptr)pa + sz, (uptr)p + sz + align);
  return pa;
}

MBlock *user_mblock(ThreadState *thr, void *p) {
  CHECK_GT(thr->in_rtl, 0);
  CHECK_NE(p, (void*)0);
  MBlock *b = (MBlock*)AllocBlock(p);
  // FIXME: Output a warning, it's a user error.
  if (p < (char*)(b + 1) || p > (char*)(b + 1) + b->size) {
    Printf("user_mblock p=%p b=%p size=%lu beg=%p end=%p\n",
        p, b, b->size, (char*)(b + 1), (char*)(b + 1) + b->size);
    CHECK_GE(p, (char*)(b + 1));
    CHECK_LE(p, (char*)(b + 1) + b->size);
  }
  return b;
}

void *internal_alloc(uptr sz) {
  ThreadState *thr = cur_thread();
  CHECK_GT(thr->in_rtl, 0);
  return Alloc(sz);
}

void internal_free(void *p) {
  ThreadState *thr = cur_thread();
  CHECK_GT(thr->in_rtl, 0);
  Free(p);
}

}  // namespace __tsan
