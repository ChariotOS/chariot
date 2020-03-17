

// widgets in a possible window server

#define EV_MOUSE_DOWN 1
#define EV_MOUSE_UP 2
#define EV_MOUSE_ENTER 3
#define EV_MOUSE_LEAVE 4
#define EV_MOUSE_MOVE 5

#define EV_KEY_DOWN 6
#define EV_KEY_UP 7

struct window;
struct widget {
  struct window *win;
  struct widget_ops *ops;
  void *private;
};


struct mouse_event {
	int flags; // clicked, etc...
	int dx, dy; // delta x,y
	int lx, ly; // local x,y
};


// C friendly operation block like we use in the kernel
struct widget_ops {
  int (*draw)(struct widget *, long x, long y, long w, long h);

  // called on any event
  int (*event)(struct widget *, int event, void *data, long dlen);

  int (*setup)(struct widget *, void *data);
  void (*destroy)(struct widget *);
};


/*

struct button : public wind::widget {
	public:
		virtual int on_click(...);
};



static int cpp_widget_event(struct widget *v, int event, void *data, long dlen) {

	auto *w = (wind::widget*)v->private;
	switch (event) {
		case EV_MOUSE_DOWN:
			return w->on_click(...);
	}
}


struct widget_ops cpp_widget_bindings {
	.event = cpp_widget_event
};


*/
