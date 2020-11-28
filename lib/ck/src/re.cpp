#include <ck/re.h>
#include <regex.h>
#include <stdlib.h>


ck::re::re(void) {}

bool ck::re::compile(const char *expr, int flags) {
  int result = regcomp(&regex, expr, flags);
  if (result != 0) {
    valid = false;
    return false;
  }
  valid = true;
  return true;
}

bool ck::re::matches(const char *expr) {
  if (!valid) return false;
  int result = regexec(&regex, expr, 0, NULL, 0);
  if (result == 0) {
		return true;
  } else {
		return false;
  }
}

// bool ck::re::matches(const char *expr, const char *text) {}
