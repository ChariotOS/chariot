#pragma once

#include <rbtree.h>

namespace cpu {
  struct Core;
}

#define RT_MAX_QUEUE


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
  // `Task` is a schedulable entity in the system. It is meant to be
  // overwritten by different subsystems in the kernel. For example, context
  // switching into a thread occurs by overwriting ::run with context switch
  // code
  // TODO: document more
  class Task : public ck::weakable<Task> {
   public:
    Task(Constraints c = AperiodicConstraint{});
    virtual ~Task();
    virtual void run(void) {}
    virtual bool rt_is_preemptable(void) { return false; }

    // get access to the constraint for this rt::Task
    const auto &constraint(void) const { return m_constraint; }
    rt::Scheduler *current_scheduler(void) const { return m_scheduler; }
    int make_runnable(int cpu = -1, bool admit = true);


    // When the task got last started
    uint64_t start_time = 0;
    // How long it has run so far without being preempted.
    uint64_t cur_run_time = 0;
    // ...
    uint64_t run_time = 0;

    // Current deadline / time of next arrival if pending for an aperiodic
    // task. This is also it's dynamic priority
    uint64_t deadline = 0;
    // Time of completion after being run.
    uint64_t exit_time = 0;


    // Statistics are reset when the constraints are changed
    uint64_t arrival_count = 0;       // how many times it has arrived (1 for aperiodic/sporadic)
    uint64_t resched_count = 0;       // how many times resched was invoked on this thread
    uint64_t resched_long_count = 0;  // how many times the long path was taken for the thread
    uint64_t switch_in_count = 0;     // number of times switched to
    uint64_t miss_count = 0;          // number of deadline misses
    uint64_t miss_time_sum = 0;       // sum of missed time
    uint64_t miss_time_sum2 = 0;      // sum of squares of missed time

    // The status of this realtime task. This is different from thread state
    // (RUNNABLE, ZOMBIE, etc...).
    rt::TaskStatus rt_status;

   protected:
    void reset_state();
    void reset_stats();
    friend rt::Scheduler;
    friend rt::PriorityQueue;
    friend rt::Queue;
    // intrusive placement structure into an `rt::PriorityQueue`
    struct rb_node prio_node;
    // intrusive placement structure into an `rt::Queue`
    struct list_head queue_node;

    // Track if this task is queued somewhere, and if it is, which one?
    rt::QueueType queue_type = NO_QUEUE;
    Constraints m_constraint;
    // What scheduler currently controls this Task
    rt::Scheduler *m_scheduler;
  };


  struct Queue {
    QueueType type;
    Queue(QueueType t) : type(t) { m_list.init(); }
    // Add a task to the end of the queue
    void enqueue(rt::Task *);
    // Pop a task from the front of the queue
    rt::Task *dequeue(void);
    // Return the overall size of the queue. This size is maintained
    // at enqueue/dequeue time
    size_t size(void) const { return m_size; }

   private:
    size_t m_size = 0;
    struct list_head m_list;
  };

  struct PriorityQueue {
    QueueType type;
    PriorityQueue(QueueType t) : type(t) {}
    // Add a task to the queue, sorting by soonest deadline
    void enqueue(rt::Task *);
    // Peek at the task with the highest prio
    rt::Task *peek(void);
    // remove a task from the queue
    void remove(rt::Task *tsk);


    // get the task with the highest prio (soonest deadline) This is basically
    // a smart wrapper around `this->remove(this->peek())`
    rt::Task *dequeue(void);

    // Return the overall size of the queue. This size is maintained
    // at enqueue/dequeue time
    size_t size(void) const { return m_size; }

   private:
    size_t m_size = 0;
    struct rb_root m_root = RB_ROOT;
  };

  // Represents the per-cpu scheduler state for a (soft-) realtime scheduler.
  // Methods are implemented in kernel/scheduler.cpp
  class Scheduler {
   public:
    Scheduler(cpu::Core &core);

    bool admit(rt::Task *task, uint64_t now);

    rt::Task *reschedule(void);
    void kick(void);

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

   protected:
    cpu::Core &m_core;
    spinlock m_lock;

    bool in_kick = false;
  };

  scoped_irqlock local_lock();

}  // namespace rt
