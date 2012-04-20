//===-- tsan_report.cc ------------------------------------------*- C++ -*-===//
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
#include "tsan_report.h"
#include "tsan_platform.h"
#include "tsan_rtl.h"

namespace __tsan {

static void PrintStack(const ReportStack *ent) {
  for (int i = 0; ent; ent = ent->next, i++) {
    Printf("    #%d %s %s:%d", i, ent->func, ent->file, ent->line);
    if (ent->col)
      Printf(":%d", ent->col);
    if (ent->module && ent->offset)
      Printf(" (%s+%p)\n", ent->module, (void*)ent->offset);
    else
      Printf(" (%p)\n", (void*)ent->pc);
  }
}

void PrintReport(const ReportDesc *rep) {
  Printf("==================\n");
  Printf("WARNING: ThreadSanitizer: ");
  if (rep->typ == ReportTypeRace)
    Printf("data race");
  else if (rep->typ == ReportTypeThreadLeak)
    Printf("thread leak");
  else if (rep->typ == ReportTypeMutexDestroyLocked)
    Printf("destroy of a locked mutex");
  else if (rep->typ == ReportTypeSignalUnsafe)
    Printf("signal-unsafe call inside of a signal");

  Printf(" (pid=%d)\n", GetPid());

  if (rep->stack)
    PrintStack(rep->stack);

  for (int i = 0; i < rep->nmop; i++) {
    const ReportMop *mop = &rep->mop[i];
    Printf("  %s of size %d at %p",
        (i == 0 ? (mop->write ? "Write" : "Read")
                : (mop->write ? "Previous write" : "Previous read")),
        mop->size, (void*)mop->addr);
    if (mop->tid == 0)
      Printf(" by main thread:\n");
    else
      Printf(" by thread %d:\n", mop->tid);
    PrintStack(mop->stack);
  }

  if (rep->loc) {
    const ReportLocation *loc = rep->loc;
    if (loc->type == ReportLocationGlobal) {
      Printf("  Location is global '%s' of size %lu at %lx %s:%d\n",
             loc->name, loc->size, loc->addr, loc->file, loc->line);
    } else if (loc->type == ReportLocationHeap) {
      Printf("  Location is heap of size %lu at %lx allocated by thread %d:\n",
             loc->size, loc->addr, loc->tid);
      PrintStack(loc->stack);
    } else if (loc->type == ReportLocationStack) {
      Printf("  Location is stack of thread %d:\n", loc->tid);
    }
  }

  for (int i = 0; i < rep->nmutex; i++) {
    ReportMutex *rm = &rep->mutex[i];
    if (rm->stack == 0)
      continue;
    Printf("  Mutex %d created at:\n", rm->id);
    PrintStack(rm->stack);
  }

  for (int i = 0; i < rep->nthread; i++) {
    ReportThread *rt = &rep->thread[i];
    if (rt->id == 0)  // Little sense in describing the main thread.
      continue;
    Printf("  Thread %d", rt->id);
    if (rt->name)
      Printf(" '%s'", rt->name);
    Printf(" (%s)", rt->running ? "running" : "finished");
    if (rt->stack)
      Printf(" created at:");
    Printf("\n");
    PrintStack(rt->stack);
  }
  Printf("==================\n");
}

void PrintStats(u64 *stat) {
  stat[StatShadowNonZero] = stat[StatShadowProcessed] - stat[StatShadowZero];

  static const char *name[StatCnt] = {};
  name[StatMop]                 = "Memory accesses                   ";
  name[StatMopRead]             = "  Including reads                 ";
  name[StatMopWrite]            = "            writes                ";
  name[StatMop1]                = "  Including size 1                ";
  name[StatMop2]                = "            size 2                ";
  name[StatMop4]                = "            size 4                ";
  name[StatMop8]                = "            size 8                ";
  name[StatMopSame]             = "  Including same                  ";
  name[StatMopRange]            = "  Including range                 ";
  name[StatShadowProcessed]     = "Shadow processed                  ";
  name[StatShadowZero]          = "  Including empty                 ";
  name[StatShadowNonZero]       = "  Including non empty             ";
  name[StatShadowSameSize]      = "  Including same size             ";
  name[StatShadowIntersect]     = "            intersect             ";
  name[StatShadowNotIntersect]  = "            not intersect         ";
  name[StatShadowSameThread]    = "  Including same thread           ";
  name[StatShadowAnotherThread] = "            another thread        ";
  name[StatShadowReplace]       = "  Including evicted               ";

  name[StatFuncEnter]           = "Function entries                  ";
  name[StatFuncExit]            = "Function exits                    ";
  name[StatEvents]              = "Events collected                  ";

  name[StatThreadCreate]        = "Total threads created             ";
  name[StatThreadFinish]        = "  threads finished                ";
  name[StatThreadReuse]         = "  threads reused                  ";
  name[StatThreadMaxTid]        = "  max tid                         ";
  name[StatThreadMaxAlive]      = "  max alive threads               ";

  name[StatMutexCreate]         = "Mutexes created                   ";
  name[StatMutexDestroy]        = "  destroyed                       ";
  name[StatMutexLock]           = "  lock                            ";
  name[StatMutexUnlock]         = "  unlock                          ";
  name[StatMutexRecLock]        = "  recursive lock                  ";
  name[StatMutexRecUnlock]      = "  recursive unlock                ";
  name[StatMutexReadLock]       = "  read lock                       ";
  name[StatMutexReadUnlock]     = "  read unlock                     ";

  name[StatAnnotation]                   = "Dynamic annotations               ";
  name[StatAnnotateHappensBefore]        = "  HappensBefore                   ";
  name[StatAnnotateHappensAfter]         = "  HappensAfter                    ";
  name[StatAnnotateCondVarSignal]        = "  CondVarSignal                   ";
  name[StatAnnotateCondVarSignalAll]     = "  CondVarSignalAll                ";
  name[StatAnnotateMutexIsNotPHB]        = "  MutexIsNotPHB                   ";
  name[StatAnnotateCondVarWait]          = "  CondVarWait                     ";
  name[StatAnnotateRWLockCreate]         = "  RWLockCreate                    ";
  name[StatAnnotateRWLockDestroy]        = "  RWLockDestroy                   ";
  name[StatAnnotateRWLockAcquired]       = "  RWLockAcquired                  ";
  name[StatAnnotateRWLockReleased]       = "  RWLockReleased                  ";
  name[StatAnnotateTraceMemory]          = "  TraceMemory                     ";
  name[StatAnnotateFlushState]           = "  FlushState                      ";
  name[StatAnnotateNewMemory]            = "  NewMemory                       ";
  name[StatAnnotateNoOp]                 = "  NoOp                            ";
  name[StatAnnotateFlushExpectedRaces]   = "  FlushExpectedRaces              ";
  name[StatAnnotateEnableRaceDetection]  = "  EnableRaceDetection             ";
  name[StatAnnotateMutexIsUsedAsCondVar] = "  MutexIsUsedAsCondVar            ";
  name[StatAnnotatePCQGet]               = "  PCQGet                          ";
  name[StatAnnotatePCQPut]               = "  PCQPut                          ";
  name[StatAnnotatePCQDestroy]           = "  PCQDestroy                      ";
  name[StatAnnotatePCQCreate]            = "  PCQCreate                       ";
  name[StatAnnotateExpectRace]           = "  ExpectRace                      ";
  name[StatAnnotateBenignRaceSized]      = "  BenignRaceSized                 ";
  name[StatAnnotateBenignRace]           = "  BenignRace                      ";
  name[StatAnnotateIgnoreReadsBegin]     = "  IgnoreReadsBegin                ";
  name[StatAnnotateIgnoreReadsEnd]       = "  IgnoreReadsEnd                  ";
  name[StatAnnotateIgnoreWritesBegin]    = "  IgnoreWritesBegin               ";
  name[StatAnnotateIgnoreWritesEnd]      = "  IgnoreWritesEnd                 ";
  name[StatAnnotatePublishMemoryRange]   = "  PublishMemoryRange              ";
  name[StatAnnotateUnpublishMemoryRange] = "  UnpublishMe
moryRange            ";
  name[StatAnnotateThreadName]           = "  ThreadName                      ";

  name[StatMtxTotal]            = "Contentionz                       ";
  name[StatMtxTrace]            = "  Trace                           ";
  name[StatMtxThreads]          = "  Threads                         ";
  name[StatMtxReport]           = "  Report                          ";
  name[StatMtxSyncVar]          = "  SyncVar                         ";
  name[StatMtxSyncTab]          = "  SyncTab                         ";
  name[StatMtxSlab]             = "  Slab                            ";
  name[StatMtxAtExit]           = "  Atexit                          ";
  name[StatMtxAnnotations]      = "  Annotations                     ";

  Printf("Statistics:\n");
  for (int i = 0; i < StatCnt; i++)
    Printf("%s: %llu\n", name[i], stat[i]);
}

}  // namespace __tsan
