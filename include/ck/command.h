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

    template <typename T>
    void args(T t) {
      arg(t);
    }

    template <typename T, typename... Rest>
    void args(T t, Rest... rest) {
      arg(t);
      args(rest...);
    }

    void arg(ck::string);


    inline const ck::string &exe(void) const { return m_exe; }
    inline int argc(void) const { return m_args.size(); }
    inline ck::vec<ck::string> argv(void) const { return m_args; }

    // start the command
    int start(void);

    // starts then waits
    int exec(void);

    // waits for the started command to exit
    int wait(void);

    inline pid_t leak(void) {
      auto o = pid;
      pid = -1;
      return o;
    }
  };

  ck::string format(const ck::command &cmd);
}  // namespace ck
