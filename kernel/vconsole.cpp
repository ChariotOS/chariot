#include <console.h>
#include <mem.h>
#include <vconsole.h>

vconsole::vconsole(int cols, int rows,
		   void (&drawer)(int x, int y, char val, char attr))
    : drawer(drawer) {
  resize(cols, rows);
}

void vconsole::resize(int c, int r) {
  if (buf != NULL) {
    kfree(buf);
  }

  buf = (struct vconsole::cell*)kmalloc(sizeof(*buf) * r * c);

  for (int i = 0; i < r * c; i++) {
    buf[i].c = ' ';
    buf[i].attr = 0x07;
  }

  m_rows = r;
  m_cols = c;

  x = 0;
  y = 0;
  pos = 0;
}

void vconsole::csi_J(int par) {
  long count = 0;
  long start = 0;
  switch (par) {
    case 0: /* erase from cursor to end of display */
      count = (cols() * rows() - pos);
      start = pos;
      break;
    case 1: /* erase from start to cursor */
      count = pos;
      start = 0;
      break;
    case 2: /* erase whole display */
      count = cols() * rows();
      start = 0;
      break;
    default:
      return;
  }

  for (int i = start; i < start + count; i++) write(i, ' ', 0x07);
}

void vconsole::csi_K(int par) {
  long count;
  long start;

  switch (par) {
    case 0: /* erase from cursor to end of line */
      if (x >= cols()) return;
      count = cols() - x;
      start = pos;
      break;
    case 1: /* erase from start of line to cursor */
      start = pos - x;
      count = (x < cols()) ? x : cols();
      break;
    case 2: /* erase whole line */
      start = pos - x;
      count = cols();
      break;
    default:
      return;
  }

  for (int i = start; i < start + count; i++) write(i, ' ', 0x07);
}

void vconsole::csi_m(void) {
  int i;

  for (i = 0; i <= npar; i++) {
    char p = par[i];

    if (p == 0) {
      attr = 0x07;
    } else if (p >= 30 && p <= 37) {
      attr = (attr & 0xF0) | (((p - 30) & 0xF));
      continue;
    } else if (p >= 40 && p <= 47) {
      attr = (attr & 0xF0) | (((p - 40) & 0xF));
      continue;

    } else if (p >= 90 && p <= 97) {
      attr = (attr & 0xF0) | (((p - 90) & 0xF));
      continue;
    } else {
      switch (par[i]) {
	case 0:
	  attr = 0x07;
	  break;
	case 1:
	  attr = 0x0f;
	  break;
	case 4:
	  attr = 0x0f;
	  break;
	case 7:
	  attr = 0x70;
	  break;
	case 27:
	  attr = 0x07;
	  break;
      }
    }
  }
}

void vconsole::scrollup(void) {
  for (int i = 0; i < cols() * (rows() - 1); i++) {
		auto &c = buf[i + cols()];
		write(i, c.c, c.attr);
	}

  // fill the last line with spaces
  for (int i = cols() * (rows() - 1); i < cols() * rows(); i++)
    write(i, ' ', 0x07);
}

void vconsole::feed(char c) {
  switch (state) {
    case 0:
      if (c > 31 && c < 127) {
	if (x >= cols()) {
	  cr();
	  lf();
	}

	write(pos++, c & 0xFF, attr);
	x++;

      } else if (c == 27) {
	state = 1;
      } else if (c == '\n') {
	lf();
	cr();
      } else if (c == '\r') {
	cr();
      } else if (c == CONS_DEL) {
	del();
      } else if (c == 8) {
	if (x) {
	  x--;
	  pos--;
	}
      } else if (c == 9) {
	/*
	c = 8 - (x & 7);
	x += c;
	pos += c << 1;
	if (x > cols()) {
	  x -= cols();
	  pos -= cols() << 1;
	  lf();
	}
	c = 9;
	*/
      }
      break;
    case 1:
      state = 0;
      if (c == '[') {
	state = 2;
      } else if (c == 'E') {
	gotoxy(0, y + 1);
      } else if (c == 'M') {
	// ri();
      } else if (c == 'D') {
	lf();
      } else if (c == 'Z') {
	// respond(tty);
      } else if (x == '7') {
	save_cur();
      } else if (x == '8') {
	restore_cur();
      }
      break;
    case 2:
      for (npar = 0; npar < VC_NPAR; npar++) par[npar] = 0;
      npar = 0;
      state = 3;
      if ((ques = (c == '?'))) {
	break;
      }
    case 3:
      if (c == ';' && npar < VC_NPAR - 1) {
	npar++;
	break;
      } else if (c >= '0' && c <= '9') {
	par[npar] = 10 * par[npar] + c - '0';
	break;
      } else
	state = 4;
    case 4:
      state = 0;
      switch (c) {
	case 'G':
	case '`':
	  if (par[0]) par[0]--;
	  gotoxy(par[0], y);
	  break;
	case 'A':
	  if (!par[0]) par[0]++;
	  gotoxy(x, y - par[0]);
	  break;
	case 'B':
	case 'e':
	  if (!par[0]) par[0]++;
	  gotoxy(x, y + par[0]);
	  break;
	case 'C':
	case 'a':
	  if (!par[0]) par[0]++;
	  gotoxy(x + par[0], y);
	  break;
	case 'D':
	  if (!par[0]) par[0]++;
	  gotoxy(x - par[0], y);
	  break;
	case 'E':
	  if (!par[0]) par[0]++;
	  gotoxy(0, y + par[0]);
	  break;
	case 'F':
	  if (!par[0]) par[0]++;
	  gotoxy(0, y - par[0]);
	  break;
	case 'd':
	  if (par[0]) par[0]--;
	  gotoxy(x, par[0]);
	  break;
	case 'H':
	case 'f':
	  if (par[0]) par[0]--;
	  if (par[1]) par[1]--;
	  gotoxy(par[1], par[0]);
	  break;
	case 'J':
	  csi_J(par[0]);
	  break;
	case 'K':
	  csi_K(par[0]);
	  break;
	case 'm':
	  csi_m();
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

void vconsole::for_each_cell(void (*fn)(int row, int col, char val,
					char attr)) {
  if (buf == NULL) return;

  int r = 0;
  int c = 0;

  for (int i = 0; true; i++) {
    fn(r, c, buf[i].c, buf[i].attr);

    c += 1;

    if (c >= cols()) {
      c = 0;
      r += 1;
    }
    if (r > rows()) break;
  }
}

void vconsole::gotoxy(unsigned int new_x, unsigned int new_y) {
  if (new_x >= cols() || new_y >= rows()) return;
  x = new_x;
  y = new_y;
  pos = y * cols() + x;
}

void vconsole::write(long p, char c, char attr) {
  if (p < 0 || p >= cols() * rows()) return;

  int x = p % cols();
  int y = p / cols();
  drawer(x, y, c, attr);
  buf[p].c = c;
  buf[p].attr = attr;
}

void vconsole::lf(void) {
  if (y + 1 < rows()) {
    y++;
    pos += cols();
    return;
  }
  scrollup();
}

void vconsole::cr(void) {
  pos -= x;
  x = 0;
}

void vconsole::del(void) {
  if (x) {
    pos--;
    x--;
    write(pos, ' ', 0x07);
  }
}
