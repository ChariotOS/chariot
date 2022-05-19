#pragma once

#include <rbtree.h>

namespace cpu {
  struct Core;
}

#define RT_MAX_QUEUE


struct Thread;

// All of this interface is implemented in kernel/scheduler.cpp
namespace rt {

  // Define the types of queues up here so we can use them before defining
  // rt::PriorityQueue
  enum QueueType { RUNNABLE_QUEUE = 0, PENDING_QUEUE = 1, APERIODIC_QUEUE = 2, NO_QUEUE = 3 };

  // Threads are one of:
  //
  // - aperiodic  (simple priority, not real-time)
  // - periodic   (period, slice, first arrival)
  // - sporadic   (size, arrival)
  //
  // On creation, a thread is aperiodic with medium priority.
  enum ConstraintType { APERIODIC = 0, PERIODIC = 1, SPORADIC = 2 };

  // Aperdiodic threads have no real-time constraings. They simply have a
  // priority, μ. Newly created threads begin their life in this class
  //
  // Most non-RT threads fall into this category, and have no guarenteed
  // execution time.
  struct AperiodicConstraint {
    uint64_t priority;  // μ: Higher number = lower prio
  };

  // Periodic threads have the constraint (phase φ, period τ, slice σ). Such a
  // thread is eligible to execute (it arrives) for the first time at wall
  // clock a + φ then arrives again at a+φ+τ, a+φ+2τ, a+φ+3τ ... The time of then ext
  // arrival is the dealine for the current arrival. On each arrival the task
  // is guarenteed that it will execute for at least σ before it's next
  // arrival (it's current deadline);
  //
  // From Liu:
  //   This type of task model is a well known deterministic workload. It
  //   characterizes accurately many traditional hard real-time applications,
  //   such as digital control, real-time monitoring, and constant bit-rate
  //   voice/video transmissions. Many scheduling algorithms based on this
  //   model have good performance and well-understood behavior.
  struct PeriodicConstraint {
    uint64_t phase;   // φ: the time of first arrival relative to time of admission
    uint64_t period;  // τ: How frequently it arrives (arrival + period = deadline)
    uint64_t slice;   // σ: How much RT computation when it arrives
  };

  // Sporadic threads have the constraint (phase φ, size ω, dealine δ,
  // aperiodic prio μ). A sporadic thread arrives at wall clock time a+φ, and
  // is guarenteed to execute for at least ω before the wall clock time δ, and
  // subsequently will execute as an aperiodic thread with priority μ.
  struct SporadicConstraint {
    uint64_t phase;               // φ: time of arrival relative to admission
    uint64_t size;                // ω: length of RT computation
    uint64_t deadline;            // δ: deadline for RT computation
    uint64_t aperiodic_priority;  // μ: what priority once it it is complete
  };


  // A single type to represent all types of constraints.
  struct Constraints {
    ConstraintType type = APERIODIC;
    union {
      AperiodicConstraint aperiodic;
      PeriodicConstraint periodic;
      SporadicConstraint sporadic;
    };

    Constraints(AperiodicConstraint c) : type(APERIODIC), aperiodic(c) {}
    Constraints(PeriodicConstraint c) : type(PERIODIC), periodic(c) {}
    Constraints(SporadicConstraint c) : type(SPORADIC), sporadic(c) {}
  };

  // Forward Declaration
  class Scheduler;
  class PriorityQueue;
  class Queue;

  class TaskQueue {
   public:
    TaskQueue(QueueType t) : m_type(t) {}
    virtual void enqueue(Thread *) = 0;
    virtual Thread *peek(void) = 0;
    // remove a task from the queue
    virtual void remove(Thread *tsk) = 0;
    // how many entries are there
    virtual size_t size(void) = 0;


    // Return the next task, removing it from the queue
    Thread *dequeue(void);
    inline rt::QueueType type(void) const { return m_type; }

   public:
    const QueueType m_type;
  };

  enum TaskStatus {
    ARRIVED = 0,  // no admission control done
    ADMITTED,     // admitted
    CHANGING,     // admitted for new constraints, now changing to them
    YIELDING,     // explit yield of slice
    SLEEPING,     // being removed from RT and non-RT run/arrival queues
                  // probably due to having been put into a wait queue
    EXITING,      // being removed from RT and non-RT run/arrival queues
                  // will not return
    DENIED,       // not admitted
    REAPABLE,     // it's OK for the reaper to destroy the thread
  };


  // argument to Thread::make_runnable
#define RT_CORE_SELF -1
#define RT_CORE_ANY -2


  struct Queue : public TaskQueue {
    using TaskQueue::TaskQueue;
    void enqueue(Thread *) override;
    void remove(Thread *task) override;
    Thread *peek(void) override;
    size_t size(void) override { return m_size; }

    void dump(const char *msg);

   private:
    size_t m_size = 0;
    struct list_head m_list;
  };

  struct PriorityQueue : public TaskQueue {
    using TaskQueue::TaskQueue;
    void enqueue(Thread *) override;
    void remove(Thread *task) override;
    Thread *peek(void) override;
    size_t size(void) override { return m_size; }

    void dump(const char *msg);

   private:
    size_t m_size = 0;
    struct rb_root m_root = RB_ROOT;
  };

  // Represents the per-cpu scheduler state for a (soft-) realtime scheduler.
  // Methods are implemented in kernel/scheduler.cpp
  class Scheduler {
   public:
    Scheduler(cpu::Core &core);

    bool admit(Thread *task, uint64_t now);

    // populate next_thread and return if a new task is ready to run
    bool reschedule(void);
    void pump_sized_tasks(Thread *next);
    // Get next_thread if it exists, clear it.
    ck::ref<Thread> claim(void);
    void kick(void);


    // take the task off any queue. Assumes the lock is not held
    int dequeue(Thread *task);

    cpu::Core &core(void) { return m_core; }

    scoped_irqlock lock(void);

    // Periodic and sporadic threads that have arrived (and are runnable)
    rt::PriorityQueue runnable = RUNNABLE_QUEUE;
    // Periodic and sporadic threads that have not yet arrived
    rt::PriorityQueue pending = PENDING_QUEUE;
    // Aperiodic threads that are runnable
    rt::Queue aperiodic = APERIODIC_QUEUE;

    uint64_t slack = 0;       // allowed slop for scheduler execution itself
    uint64_t num_thefts = 0;  // how many threads I've successfully stolen
    Thread *next_thread = nullptr;

   protected:
    cpu::Core &m_core;
    spinlock m_lock;

    bool in_kick = false;
  };

  scoped_irqlock local_lock();

}  // namespace rt
