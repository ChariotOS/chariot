#include <vconsole.h>

#ifdef KERNEL

extern void *malloc(unsigned long);
extern void free(void *);

#define NULL (0)

#else
#include <stdlib.h>
#endif

#define COLS (vc->cols)
#define ROWS (vc->rows)

static void vc_write(struct vcons *vc, long p, char c, char attr) {
  if (p < 0 || p >= COLS * ROWS) return;

  // don't draw the change if it isn't needed.
  if (vc->buf[p].c != c || vc->buf[p].attr != attr) {
    int x = p % COLS;
    int y = p / COLS;

    vc->buf[p].c = c;
    vc->buf[p].attr = attr;
    vc->scribe(x, y, &vc->buf[p]);
  }
}

static void vc_goto(struct vcons *vc, int new_x, int new_y) {
  if (new_x >= COLS || new_y >= ROWS) return;
  vc->x = new_x;
  vc->y = new_y;
  vc->pos = new_y * COLS + new_x;
}

static void vc_csi_J(struct vcons *vc, int par) {
  long count = 0;
  long start = 0;
  switch (par) {
    case 0: /* erase from cursor to end of display */
      count = (COLS * ROWS - vc->pos);
      start = vc->pos;
      break;
    case 1: /* erase from start to cursor */
      count = vc->pos;
      start = 0;
      break;
    case 2: /* erase whole display */
      count = COLS * ROWS;
      start = 0;
      break;
    default:
      return;
  }

  for (int i = start; i < start + count; i++) vc_write(vc, i, ' ', 0x07);
}

static void vc_csi_K(struct vcons *vc, int par) {
  long count;
  long start;

  switch (par) {
    case 0: /* erase from cursor to end of line */
      if (vc->x >= COLS) return;
      count = COLS - vc->x;
      start = vc->pos;
      break;
    case 1: /* erase from start of line to cursor */
      start = vc->pos - vc->x;
      count = (vc->x < COLS) ? vc->x : COLS;
      break;
    case 2: /* erase whole line */
      start = vc->pos - vc->x;
      count = COLS;
      break;
    default:
      return;
  }

  for (int i = start; i < start + count; i++) vc_write(vc, i, ' ', 0x07);
}

static void vc_csi_m(struct vcons *vc) {
  int i;

  for (i = 0; i <= vc->npar; i++) {
    char p = vc->par[i];

    if (p == 0) {
      vc->attr = 0x07;
    } else if (p >= 30 && p <= 37) {
      vc->attr = (vc->attr & 0xF0) | (((p - 30) & 0xF));
      continue;
    } else if (p >= 40 && p <= 47) {
      vc->attr = (vc->attr & 0xF0) | (((p - 40) & 0xF));
      continue;

    } else if (p >= 90 && p <= 97) {
      vc->attr = (vc->attr & 0xF0) | (((p - 90) & 0xF));
      continue;
    } else {
      switch (vc->par[i]) {
	case 0:
	  vc->attr = 0x07;
	  break;
	case 1:
	  vc->attr = 0x0f;
	  break;
	case 4:
	  vc->attr = 0x0f;
	  break;
	case 7:
	  vc->attr = 0x70;
	  break;
	case 27:
	  vc->attr = 0x07;
	  break;
      }
    }
  }
}

static void vc_scrollup(struct vcons *vc) {
  for (int i = 0; i < COLS * (ROWS - 1); i++) {
    struct vc_cell *c = &vc->buf[i + COLS];
    vc_write(vc, i, c->c, c->attr);
  }

  // fill the last line with spaces
  for (int i = COLS * (ROWS - 1); i < COLS * ROWS; i++) {
    vc_write(vc, i, ' ', 0x07);
  }
}

static void vc_lf(struct vcons *vc) {
  if (vc->y + 1 < ROWS) {
    vc->y++;
    vc->pos += COLS;
    return;
  }
  vc_scrollup(vc);
}

static void vc_cr(struct vcons *vc) {
  vc->pos -= vc->x;
  vc->x = 0;
}

static void vc_del(struct vcons *vc) {
  if (vc->x) {
    vc->pos--;
    vc->x--;
    vc_write(vc, vc->pos, ' ', 0x07);
  }
}

static void vc_clear(struct vcons *vc) {
  vc->state = 0;
  vc->rows = 0;
  vc->cols = 0;

  vc->x = 0;
  vc->y = 0;

  vc->saved_x = 0;
  vc->saved_y = 0;

  vc->attr = 0x07;
}

// implementation of the user API
void vc_init(struct vcons *vc, int col, int row, vc_scribe_func scribe) {
  vc_clear(vc);
  vc->scribe = scribe;
  vc->cols = col;
  vc->rows = row;
  vc_resize(vc, col, row);
}

void vc_init_static(struct vcons *vc, int col, int row, vc_scribe_func scribe,
		    struct vc_cell *cells) {
  vc_clear(vc);
  vc->scribe = scribe;
  vc->cols = col;
  vc->rows = row;
  vc->buf = cells;
}

void vc_resize(struct vcons *vc, int col, int row) {
  if (vc->buf != NULL) free(vc->buf);
  vc->buf = (struct vc_cell *)malloc(sizeof(struct vc_cell) * col * row);
  for (int i = 0; i < col * row; i++) {
    vc->buf[i].c = ' ';
    vc->buf[i].attr = 0x07;
  }
  vc->cols = col;
  vc->rows = row;
  vc->x = 0;
  vc->y = 0;
  vc->pos = 0;
}


#define TAB_WIDTH 8

void vc_feed(struct vcons *vc, char c) {
  switch (vc->state) {
    case 0:
      if (c > 31 && c < 127) {
	if (vc->x >= COLS) {
	  vc_cr(vc);
	  vc_lf(vc);
	}

	vc_write(vc, vc->pos++, c & 0xFF, vc->attr);
	vc->x++;

      } else if (c == 27) {
	vc->state = 1;
      } else if (c == '\n') {
	vc_lf(vc);
	vc_cr(vc);
      } else if (c == '\r') {
	vc_cr(vc);
      } else if (c == 0x7f /* delete */) {
	vc_del(vc);
      } else if (c == 8) {
	if (vc->x) {
	  vc->x--;
	  vc->pos--;
	}
      } else if (c == '\t') {
	c = 8 - (vc->x & 7);
	vc->x += c;
	vc->pos += c;
	if (vc->x > COLS) {
	  vc->x -= COLS;
	  vc->pos -= COLS;
	  vc_lf(vc);
	}
	c = 9;
      }
      break;
    case 1:
      vc->state = 0;
      if (c == '[') {
	vc->state = 2;
      } else if (c == 'E') {
	vc_goto(vc, 0, vc->y + 1);
      } else if (c == 'M') {
	// ri();
      } else if (c == 'D') {
	vc_lf(vc);
      } else if (c == 'Z') {
	// respond(tty);
      } else if (c == '7') {
	vc->saved_x = vc->x;
	vc->saved_y = vc->y;
      } else if (c == '8') {
	vc->x = vc->saved_x;
	vc->y = vc->saved_y;
	vc->pos = vc->y * COLS + vc->x;
      }
      break;
    case 2:
      for (vc->npar = 0; vc->npar < VC_NPAR; vc->npar++) vc->par[vc->npar] = 0;
      vc->npar = 0;
      vc->state = 3;
      if ((vc->ques = (c == '?'))) {
	break;
      }
    case 3:
      if (c == ';' && vc->npar < VC_NPAR - 1) {
	vc->npar++;
	break;
      } else if (c >= '0' && c <= '9') {
	vc->par[vc->npar] = 10 * vc->par[vc->npar] + c - '0';
	break;
      } else
	vc->state = 4;
    case 4:
      vc->state = 0;
      switch (c) {
	case 'G':
	case '`':
	  if (vc->par[0]) vc->par[0]--;
	  vc_goto(vc, vc->par[0], vc->y);
	  break;
	case 'A':
	  if (!vc->par[0]) vc->par[0]++;
	  vc_goto(vc, vc->x, vc->y - vc->par[0]);
	  break;
	case 'B':
	case 'e':
	  if (!vc->par[0]) vc->par[0]++;
	  vc_goto(vc, vc->x, vc->y + vc->par[0]);
	  break;
	case 'C':
	case 'a':
	  if (!vc->par[0]) vc->par[0]++;
	  vc_goto(vc, vc->x + vc->par[0], vc->y);
	  break;
	case 'D':
	  if (!vc->par[0]) vc->par[0]++;
	  vc_goto(vc, vc->x - vc->par[0], vc->y);
	  break;
	case 'E':
	  if (!vc->par[0]) vc->par[0]++;
	  vc_goto(vc, 0, vc->y + vc->par[0]);
	  break;
	case 'F':
	  if (!vc->par[0]) vc->par[0]++;
	  vc_goto(vc, 0, vc->y - vc->par[0]);
	  break;
	case 'd':
	  if (vc->par[0]) vc->par[0]--;
	  vc_goto(vc, vc->x, vc->par[0]);
	  break;
	case 'H':
	case 'f':
	  if (vc->par[0]) vc->par[0]--;
	  if (vc->par[1]) vc->par[1]--;
	  vc_goto(vc, vc->par[1], vc->par[0]);
	  break;
	case 'J':
	  vc_csi_J(vc, vc->par[0]);
	  break;
	case 'K':
	  vc_csi_K(vc, vc->par[0]);
	  break;
	case 'm':
	  vc_csi_m(vc);
	  break;
	  /*
	  case 'L':
		  csi_L(par[0]);
		  break;
	  case 'M':
		  csi_M(par[0]);
		  break;
	  case 'P':
		  csi_P(par[0]);
		  break;
	  case '@':
		  csi_at(par[0]);
		  break;
	  case 'r':
		  if (par[0]) par[0]--;
		  if (!par[1]) par[1]=lines;
		  if (par[0] < par[1] &&
		      par[1] <= lines) {
			  top=par[0];
			  bottom=par[1];
		  }
		  break;
	  case 's':
		  save_cur();
		  break;
	  case 'u':
		  restore_cur();
		  break;
	  */
      }
  }
}

void vc_free(struct vcons *vc) {
  if (vc->buf != NULL) {
    free(vc->buf);
  }
}

struct vc_cell *vc_get_cell(struct vcons *vc, int col, int row) {
  return &vc->buf[col + row * COLS];
}
