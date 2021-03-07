#include <ck/prompt.h>



ck::prompt::prompt(void) {
  max = 32;
  len = 0;
  ind = 0;
  buf = (char *)calloc(max, 1);
}



ck::prompt::~prompt(void) {
  free(buf);
}


// the number of ANSI paramters
#define NPAR 16
ck::string ck::prompt::readline(const char *prompt) {
  ck::string value;
  /*
int state = 0;
static unsigned long npar, par[NPAR];
int ques = 0;

int history_index = -1;
*/
  return value;
}


void ck::prompt::add_history_item(ck::string item) {
  m_history.push(item);
}


void ck::prompt::insert(char c) {
  // ensure there is enough space for the character insertion
  if (len + 1 >= max - 1) {
    max *= 2;
    buf = (char *)realloc(buf, max);
  }

  // insert
  // _________|__________________00000000000
  //          ^                  ^         ^
  //          ind                len       max

  // move the bytes after the cursor right by one
  memmove(buf + ind + 1, buf + ind, len - ind);
  buf[ind] = c;
  ind++;
  buf[++len] = '\0';
}


void ck::prompt::del(void) {
  if (ind == 0) return;
  int i = ind;
  int l = len;

  // move the bytes after the cursor left by one
  memmove(buf + i - 1, buf + i, l - i);
  ind--;
  buf[--len] = '\0';
}

void ck::prompt::display(const char *prompt) {
  // TODO: this wastes IO operations *alot*
  fprintf(stderr, "\x1b[2K\r");
  // fprintf(stderr, "%4d ", ind);
  fprintf(stderr, "%s", prompt);
  fprintf(stderr, "%s", buf);
  // CSI n C will move by 1 even when you ask for 0...
  int dist = len - ind;
  if (dist != 0) {
    if (dist > 6 /* arbitrary */) {
      for (int i = 0; i < dist; i++)
        fputc('\b', stderr);
    } else {
      fprintf(stderr, "\x1b[%dD", dist);
    }
  }

  fflush(stderr);
}

void ck::prompt::handle_special(char c) {
  switch (c) {
    case 0x01:
      ind = 0;
      break;

    case 0x05:
      ind = len;
      break;

      // C-u to clear the input
    case 0x15:
      memset(buf, 0, len);
      ind = 0;
      len = 0;
      break;

    default:
      printf("special input: 0x%02x\n", c);
  }
}
