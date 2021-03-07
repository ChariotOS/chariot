#pragma once

#define VC_NPAR 16

#ifdef __cplusplus

// the actual VC structure is implemented in C, to share code with userspace
extern "C" {
#endif

struct vc_cell {
  char c;              // the char at this cell
  unsigned char attr;  // the attribute of this cell
} __attribute__((packed));

typedef void (*vc_scribe_func)(int x, int y, struct vc_cell *, int flags);



#define VC_SCRIBE_CURSOR (1 << 0)

// opaque
struct vcons {
  /* do not modify any of these fields directly */
  int state;
  int cols, rows;

  unsigned int pos;
  unsigned int x;
  unsigned int y;

  unsigned int saved_x;
  unsigned int saved_y;

  vc_scribe_func scribe;

  unsigned int npar;
  unsigned int par[VC_NPAR];
  unsigned int ques;
  unsigned int attr;

  struct vc_cell *buf;
};

// initialize a vconsole.
void vc_init(struct vcons *, int col, int row, vc_scribe_func);

// initialize a static vconsole with some cells
void vc_init_static(struct vcons *, int col, int row, vc_scribe_func, struct vc_cell *cells);
void vc_resize(struct vcons *, int col, int row);
void vc_feed(struct vcons *, char c);

// doesn't free the vcons, just the data inside it.
void vc_free(struct vcons *);

struct vc_cell *vc_get_cell(struct vcons *, int col, int row);

#ifdef __cplusplus
}
#endif
