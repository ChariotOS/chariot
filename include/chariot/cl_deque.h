/*
 * MIT License
 *
 * Copyright (c) 2018 Nick Wanninger
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// Chase-Lev work stealing deque
//
// Dynamic Circular Work-Stealing Deque
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.170.1097&rep=rep1&type=pdf
//
// Correct and Efﬁcient Work-Stealing for Weak Memory Models
// http://www.di.ens.fr/~zappa/readings/ppopp13.pdf


#pragma once

#ifndef CL_DEQUE_H
#define CL_DEQUE_H

#include <types.h>
#include <atom.h>


/**
 * cl_deque is an implementation of the Chase-Lev dynamic circular
 * work-stealing deque described in their paper,
 * http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.170.1097&rep=rep1&type=pdf
 * It's goal is to allow efficient, thread safe, lock-free work stealing
 * for the scheduler internally in cedar. The only thing allowed to add to the
 * deque is the owner of the deque, all other methods of accessing the deque
 * require that the users go through the `steal()` method and attempt to steal
 * work from this deque
 */
template <typename T>
class cl_deque {
  class circular_array {
   private:
    long long log_size = 0;
    ck::atom<T>* segment;

   public:
    circular_array(long long sz) {
      log_size = sz;
      segment = new ck::atom<T>[1 << log_size];
    }

    long size() { return 1 << log_size; }

    /**
     * load a value atomically from the segment
     */
    T get(long i) { return segment[i % size()].load(memory_order_relaxed); }

    /**
     * store a value atomically in the segment
     */
    void put(long long i, T x) { segment[i % size()].store(x, memory_order_relaxed); }

    /**
     * "resize" the deque and return a new circular array pointer
     * with that new size. This is so that the cl_deque can have an atomic lock
     * that it modifies atomically (a pointer) and it moves the logic outside of
     * the the circular_array
     */
    auto grow(long long b, long long t) {
      // create a new circular array of twice the size
      auto* a = new circular_array(log_size + 1);
      for (long long i = t; i < b; i++)
        a->put(i, get(i));
      return a;
    }
  };



  ck::atom<circular_array*> buffer;
  ck::atom<long long> top, bottom;


 private:
  bool cas_top(long long oldval, long long newval) {
    return top.compare_exchange_strong(oldval, newval, memory_order_seq_cst, memory_order_relaxed);
  }

 public:
  // for some reason, the CPUs don't initialize the buffers, so we need to
  // do it manually.
  void init() {
    buffer = new circular_array(1);
    top = 0;
    bottom = 0;
    printf("init! buffer = %p\n", buffer);
  }


  cl_deque() { init(); }

  ~cl_deque() {
    auto* p = buffer.load(memory_order_relaxed);
    if (p) delete p;
  }


  long long size(void) {
    long long b = bottom.load(memory_order_relaxed);
    long long t = top.load(memory_order_relaxed);
    return b - t;
  }

  /**
   * Pushes o onto the bottom of the deque
   *
   * this function is meant to only be invoked by the owner
   */
  void push(T o) {
    long long b = bottom.load(memory_order_relaxed);
    atomic_thread_fence(memory_order_seq_cst);
    long long t = top.load(memory_order_relaxed);
    circular_array* a = buffer;
    long size = b - t;
    if (size >= a->size() - 1) {
      a = a->grow(b, t);
      buffer.store(a, memory_order_relaxed);
    }
    a->put(b, o);
    atomic_thread_fence(memory_order_relaxed);
    bottom.store(b + 1, memory_order_relaxed);
  }

  /**
   * Pops a T from the bottom of the deque if the deque is not empty,
   * setting the success arg to true. If the deque is empty, return
   * the default constructor for T and set the success arg to false
   *
   * this function is meant to only be invoked by the owner
   */
  T pop(bool* success = nullptr) {
    long long b = bottom.load(memory_order_relaxed);
    circular_array* a = buffer.load(memory_order_relaxed);
    atomic_thread_fence(memory_order_seq_cst);
    b = b - 1;
    bottom = b;
    long long t = top.load(memory_order_relaxed);
    long long size = b - t;
    if (size < 0) {
      bottom = t;
      if (success) *success = false;
      return nullptr;
    }

    T o = a->get(b);
    if (success) *success = true;
    if (size > 0) {
      return o;
    }
    if (!cas_top(t, t + 1)) {
      if (success) *success = false;
      o = nullptr;
    }

    bottom = t + 1;
    return o;
  }

  /**
   * If the deque is empty, returns T{} and sets success arg to false,
   * otherwise returns the element successfully stolen from the top of the
   * deque, or throws a logic error if this process loses a race with another
   * process to steal the topmoset element
   */
  T steal(bool* success = nullptr) {
    long long t = top.load(memory_order_acquire);
    atomic_thread_fence(memory_order_seq_cst);
    long long b = bottom.load(memory_order_acquire);
    circular_array* a = buffer;
    long long size = b - t;
    if (size <= 0) {
      if (success) *success = false;
      return nullptr;
    }
    T o = a->get(t);
    if (success) *success = true;
    if (!cas_top(t, t + 1)) {
      return nullptr;
      // throw logic_error("Steal compare and swap failed");
    }

    return o;
  }
};



#endif
