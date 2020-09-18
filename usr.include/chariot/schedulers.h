#pragma once

#include <lock.h>

// fwd decl
struct thread;

namespace sched {



	struct round_robin {
		spinlock big_lock;
		struct thread *front;
		struct thread *back;


		/////////////////////////////////////
		// required methods
		int add_task(struct thread *);
		int remove_task(struct thread *t);
		struct thread *pick_next(void);
		/////////////////////////////////////


		int add_task_impl(struct thread *t);
		int remove_task_impl(struct thread *t);


		// expects the big_lock to be held
		void dump(void);


		struct thread_state {
			struct thread *next;
			struct thread *prev;
		};

		using thread_state = thread_state;
	};

};
