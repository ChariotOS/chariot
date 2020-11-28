#pragma once

#include <ck/object.h>
#include <ck/vec.h>
#include <ck/ptr.h>
#include <ck/fsnotifier.h>
#include <ck/func.h>

namespace ck {
	class eventloop {

		public:
			eventloop(void);
			~eventloop(void);


			/**
			 * start the eventloop, only to return once we are told.
			 */
			void start(void);


			/**
			 * Find all events that need to be dispatched
			 */
			void pump(void);


			/**
			 * prepare an event to be sent to an object
			 */
			void post_event(ck::object &obj, ck::event *ev);

			/**
			 * loop through all the posted events and actually evaluate them
			 */
			void dispatch(void);




			static ck::eventloop *current(void);


			static void register_notifier(ck::fsnotifier &);
			static void deregister_notifier(ck::fsnotifier &);

			// cause the eventlopp to exit
			static void exit(void);

			static void defer(ck::func<void(void)> cb);

		private:
			bool m_finished = false;


			struct pending_event {
				inline pending_event(ck::object &obj, ck::event *ev) : obj(obj), ev(ev) {}
				~pending_event() = default;

				ck::object &obj;
				ck::event *ev;
			};

			ck::vec<pending_event> m_pending;

	};
};
