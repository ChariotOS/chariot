#pragma once

#include <chariot.h>
#include <ck/string.h>

namespace ck {
  class command {
    pid_t pid = -1;
    ck::string m_exe;
    ck::vec<ck::string> m_args;

   public:
    command(ck::string exe);
    template <typename... T>
    inline command(ck::string exe, T... args) : m_exe(exe) {
      this->args(args...);
    }

    ~command(void);

    // The base case: we just have a single number.
    template <typename T>
    void args(T t) {
			arg(t);
    }

    // The recursive case: we take a number, alongside
    // some other numbers, and produce their sum.
    template <typename T, typename... Rest>
    void args(T t, Rest... rest) {
			arg(t);
			args(rest...);
    }

    void arg(ck::string);

    int start(void);

		// starts then waits
    int exec(void);

		// waits for the started command to exit
    int wait(void);

  };
}  // namespace ck
