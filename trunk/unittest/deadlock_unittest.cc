/*
  This file is part of Valgrind, a dynamic binary instrumentation
  framework.

  Copyright (C) 2008-2008 Google Inc
     opensource@google.com 

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307, USA.

  The GNU General Public License is contained in the file COPYING.
*/

// Author: Konstantin Serebryany <opensource@google.com> 
//
// This file contains a set of unit tests for a deadlock detection tool. 
//
//
//
// This test can be compiled with pthreads (default) or
// with any other library that supports threads, locks, cond vars, etc. 
// 
// To compile with pthreads: 
//   g++  deadlock_unittest.cc -lpthread -g
// 
// To compile with different library: 
//   1. cp thread_wrappers_pthread.h thread_wrappers_yourlib.h
//   2. edit thread_wrappers_yourlib.h
//   3. add '-DTHREAD_WRAPPERS="thread_wrappers_yourlib.h"' to your compilation.
//
//

// This test must not include any other file specific to threading library,
// everything should be inside THREAD_WRAPPERS. 
#ifndef THREAD_WRAPPERS 
# define THREAD_WRAPPERS "thread_wrappers_pthread.h"
#endif 
#include THREAD_WRAPPERS

#include <vector>
#include <string>
#include <map>
#include <ext/hash_map>
#include <algorithm>
#include <cstring>      // strlen(), index(), rindex()

//
// Each test resides in its own namespace. 
// Namespaces are named test01, test02, ... 
// Please, *DO NOT* change the logic of existing tests nor rename them. 
// Create a new test instead. 
//
// Some tests use sleep()/usleep(). 
// This is not a synchronization, but a simple way to trigger 
// some specific behaviour of the scheduler.

// Globals and utilities used by several tests. {{{1

typedef void (*void_func_void_t)(void);

struct Test{
  void_func_void_t f_;
  int flags_;
  Test(void_func_void_t f, int flags) 
    : f_(f)
    , flags_(flags)
  {}
  Test() : f_(0), flags_(0) {}
};
std::map<int, Test> TheMapOfTests;


struct TestAdder {
  TestAdder(void_func_void_t f, int id, int flags = 0) {
    CHECK(TheMapOfTests.count(id) == 0);
    TheMapOfTests[id] = Test(f, flags);
  }
};

#define REGISTER_TEST(f, id)         TestAdder add_test_##id (f, id);

// Put everything into stderr.
#define printf(args...) fprintf(stderr, args)

#ifndef MAIN_INIT_ACTION
#define MAIN_INIT_ACTION
#endif 


static bool ArgIsOne(int *arg) { return *arg == 1; };


ProducerConsumerQueue *Q[4] = {
  new ProducerConsumerQueue(INT_MAX),
  new ProducerConsumerQueue(INT_MAX),
  new ProducerConsumerQueue(INT_MAX),
  new ProducerConsumerQueue(INT_MAX)
};
Mutex mu[4];

void PutAndWait(int *work_item, int idx) {
  // Put work_item1.
  Q[idx]->Put(work_item);

  // Wait for work_item completion.
  mu[idx].LockWhen(Condition(&ArgIsOne, work_item));
  mu[idx].Unlock();
}

void GetAndServe(int idx) {
  // Get an item.
  int *item = reinterpret_cast<int*>(Q[idx]->Get());

  // Handle work item and signal completion.
  mu[idx].Lock();
  *item = 1;
  mu[idx].Unlock();
}


bool TryGetAndServe(int idx) {
  // Get an item.
  int *item;
  if (Q[idx]->TryGet(reinterpret_cast<void**>(&item))) {
    // Handle work item and signal completion.
    mu[idx].Lock();
    *item = 1;
    mu[idx].Unlock();
    return true;
  } else {
    return false;
  }
}



int main(int argc, char** argv) { // {{{1
  MAIN_INIT_ACTION;
  srand(time(0));
  if (argc > 1) {
    // the tests are listed in command line flags 
    for (int i = 1; i < argc; i++) {
      int f_num = atoi(argv[i]);
      CHECK(TheMapOfTests.count(f_num));
      TheMapOfTests[f_num].f_();
    }
  } else {
    // all tests 
    for (std::map<int,Test>::iterator it = TheMapOfTests.begin(); 
        it != TheMapOfTests.end();
        ++it) {
      it->second.f_();
    } 
  }
}




// An array of threads. Create/start/join all elements at once. {{{1
class MyThreadArray {
 public:
  typedef void (*F) (void);
  MyThreadArray(F f1, F f2 = NULL, F f3 = NULL, F f4 = NULL) {
    ar_[0] = new MyThread(f1);
    ar_[1] = f2 ? new MyThread(f2) : NULL;
    ar_[2] = f3 ? new MyThread(f3) : NULL;
    ar_[3] = f4 ? new MyThread(f4) : NULL;
  }
  void Start() {
    for(int i = 0; i < 4; i++) {
      if(ar_[i]) {
        ar_[i]->Start();
        usleep(10);
      }
    }
  }

  void Join() {
    for(int i = 0; i < 4; i++) {
      if(ar_[i]) {
        ar_[i]->Join();
      }
    }
  }

  ~MyThreadArray() {
    for(int i = 0; i < 4; i++) {
      delete ar_[i];
    }
  }
 private:
  MyThread *ar_[4];
};


// Set of threads that execute the same function.
class MyThreadSet {
 public:
  typedef void (*F) (void);
  MyThreadSet(F f, int count) 
    : count_(count) {
    CHECK(count_ >= 1 && count_ <= 1000);
    ar_ = new MyThread* [count_];
    for (int i = 0; i < count_; i++) {
      ar_[i] = new MyThread(f);
    }
  }
  void Start() {
    for (int i = 0; i < count_; i++) {
      ar_[i]->Start();
    }
  }
  void Join() {
    for (int i = 0; i < count_; i++) {
      ar_[i]->Join();
    }
  }
  ~MyThreadSet() {
    for (int i = 0; i < count_; i++) {
      delete ar_[i];
    }
    delete ar_;
  }

 private: 
  MyThread **ar_;
  int count_;
};

int ThreadId() {
  static Mutex mu;
  static map<pthread_t, int> m;

  int id;
  pthread_t self = pthread_self();

  mu.Lock();
  map<pthread_t, int>::iterator it = m.find(self);
  if (it != m.end()) {
    id = it->second;
  } else {
    id = m.size();
    m[self] = id;
  }
  mu.Unlock();
  return id;
}

// test00: {{{1
namespace test00 {
void Run() {
  printf("test00: negative\n");
}
REGISTER_TEST(Run, 00)
}  // namespace test00

// test01: Simple deadlock, 2 threads. {{{1
namespace test01 {
Mutex mu1, mu2;
void Worker1()  {
  mu1.Lock();
  mu2.Lock();
  mu2.Unlock();
  mu1.Unlock();
}
void Worker2()  {
  usleep(1000);
  mu2.Lock();
  mu1.Lock();
  mu1.Unlock();
  mu2.Unlock();
}
void Run() {
  MyThreadArray t(Worker1, Worker2);
  t.Start();
  t.Join();
  printf("test01: positive, simple deadlock\n");
}
REGISTER_TEST(Run, 01)
}  // namespace test01

// test02: Simple deadlock, 4 threads. {{{1
namespace test02 {
Mutex mu1, mu2, mu3, mu4;
void Worker1()  {
  mu1.Lock();   mu2.Lock();
  mu2.Unlock(); mu1.Unlock();
}
void Worker2()  {
  usleep(1000);
  mu2.Lock();   mu3.Lock();
  mu3.Unlock(); mu2.Unlock();
}
void Worker3()  {
  usleep(2000);
  mu3.Lock();   mu4.Lock();
  mu4.Unlock(); mu3.Unlock();
}
void Worker4()  {
  usleep(3000);
  mu4.Lock();   mu1.Lock();
  mu1.Unlock(); mu4.Unlock();
}
void Run() {
  MyThreadArray t(Worker1, Worker2, Worker3, Worker4);
  t.Start();
  t.Join();
  printf("test02: positive, simple deadlock\n");
}
REGISTER_TEST(Run, 02)
}  // namespace test02

// test03: Queue deadlock test, 2 workers. {{{1
// This test will deadlock for sure.
namespace  test03 {

void Worker1() {
  int *item = new int (0);
  PutAndWait(item, 0);
  GetAndServe(1);
}
void Worker2() {
  int *item = new int (0);
  PutAndWait(item, 1);
  GetAndServe(0);
}
void Run() {
  printf("test03: queue deadlock\n");
  MyThreadArray t(Worker1, Worker2);
  t.Start();
  t.Join();
}
REGISTER_TEST(Run, 03)
}  // namespace test03

// test04: Queue deadlock test, 3 workers. {{{1
// This test will deadlock for sure.
namespace  test04 {

void Worker1() {
  int *item = new int (0);
  PutAndWait(item, 0);
  GetAndServe(1);
}
void Worker2() {
  int *item = new int (0);
  PutAndWait(item, 1);
  GetAndServe(2);
}

void Worker3() {
  int *item = new int (0);
  PutAndWait(item, 2);
  GetAndServe(0);
}

void Run() {
  printf("test04: queue deadlock\n");
  MyThreadArray t(Worker1, Worker2, Worker3);
  t.Start();
  t.Join();
}
REGISTER_TEST(Run, 04)
}  // namespace test04

// test05: Queue deadlock test, 1 worker set. {{{1
// This test will deadlock after some number of served requests.
namespace  test05 {

int item_number = 0;


// This function randomly enqueues work and waits on it or serves a piece of work.
void Worker() {
  while(true) {
    int action = rand() % 100;
    if (action <= 1) {        // PutAndWait.
      int n = __sync_add_and_fetch(&item_number, 1);
      int *item = new int(0);
      PutAndWait(item, 0);
      if ((n % 10000) == 0) {
        printf("Done %d\n", n);
      }
      delete item;
    } else {                 // GetAndServe.
      TryGetAndServe(0);
    }
  }
}


void Run() {
  printf("test05: queue deadlock\n");
  MyThreadSet t(Worker, 5);
  t.Start();
  t.Join();
}
REGISTER_TEST(Run, 05)
}  // namespace test05

// test06: Queue deadlock test, 3 worker sets. {{{1
// This test will deadlock after some number of served requests.
namespace  test06 {

int item_number[2] = {0, 0};

// This function randomly enqueues work to queue 'put_queue' and waits on it 
// or serves a piece of work from queue 'get_queue'.
void Worker(int put_queue, int get_queue) {
  while(true) {
    int action = rand() % 1000;
    if (action <= 100) {        // PutAndWait.
      int n = __sync_add_and_fetch(&item_number[put_queue], 1);
      int *item = new int(0);
      PutAndWait(item, put_queue);
      if ((n % 1000) == 0) {
        printf("Q[%d]: done %d\n", put_queue, n);
      }
      delete item;
    } else {                 // TryGetAndServe.
      TryGetAndServe(get_queue);
    }
  }
}

void Worker1() { Worker(0, 1); }
void Worker2() { Worker(1, 2); }
void Worker3() { Worker(2, 0); }

void Run() {
  printf("test06: queue deadlock\n");
  MyThreadSet t1(Worker1, 4);
  MyThreadSet t2(Worker2, 4);
  MyThreadSet t3(Worker3, 4);
  t1.Start();
  t2.Start();
  t3.Start();
  t1.Join();
  t2.Join();
  t3.Join();
}
REGISTER_TEST(Run, 06)
}  // namespace test06
