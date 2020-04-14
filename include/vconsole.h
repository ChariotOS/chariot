#pragma once

#include <func.h>

#define VC_NPAR 16

// emulates a TTY on an internal buffer
struct vconsole {
  vconsole(int cols, int rows, void (&drawer)(int x, int y, char val, char attr));

  void resize(int cols, int rows);

  // feed a single char of display input
  void feed(char c);
	inline void feed(const char *m) {
		for (int i = 0; m[i]; i++) feed(m[i]);
	}

  inline int cols(void) { return m_cols; }
  inline int rows(void) { return m_rows; }

  inline int cursor(int &x, int &y) {
    x = this->x;
    y = this->y;
    return 0;
  }

  struct cell {
    char c;
    char attr;
  };

  void for_each_cell(void (*fn)(int x, int y, char val, char attr));

 private:
	void (&drawer)(int row, int col, char val, char attr);
	int state = 0;
  int m_rows, m_cols;

  uint32_t pos = 0;
  uint32_t x = 0;
  uint32_t y = 0;

  uint32_t saved_x = 0, saved_y = 0;

  struct cell *buf = NULL;

  // parameter storage
  uint32_t npar, par[VC_NPAR];
  uint32_t ques = 0;
  uint32_t attr = 0x07;

  void gotoxy(unsigned int new_x, unsigned int new_y);
  void write(long pos, char c, char attr);

  void scrollup(void);
  void lf(void);
  void cr(void);
  void del(void);
  void csi_J(int par);
  void csi_K(int par);
	void csi_m(void);

  inline void save_cur(void) {
    saved_x = x;
    saved_y = y;
  }

  inline void restore_cur(void) {
    x = saved_x;
    y = saved_y;
    pos = y * cols() + x;
  }


};
