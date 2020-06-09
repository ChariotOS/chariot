#pragma once


#include <chariot.h>
#include <stdio.h>
#include <stdlib.h>

namespace ck {
  template <typename S, typename E>
  class result {
   private:
    bool valid = false;
    bool success = false;
    union {
      // This is so bad, but it's needed and is safe.
      char success_buf[sizeof(S)];
      char err_buf[sizeof(E)];
    } data;


    inline auto assert_valid() {
      if (!valid) {
        panic("Attempt to use an invalid result\n");
      }
    }

   public:
    static auto ok(S val) {
      result<S, E> r;
      r.valid = true;
      r.success = true;
      (void)new ((S*)&r.data.success_buf) S(move(val));
      printf("here\n");
      return move(r);
    }

    static auto err(E&& err) {
      result<S, E> r;
      r.valid = true;
      r.success = false;
      (void)new ((E*)&r.data.err_buf) E(move(err));
      return move(r);
    }


    result() {
			printf("result()\n");
		};
    result(result&&) = default;

    ~result(void) {
      if (!valid) return;
			valid = false;

			printf("~result\n");
      if (success) {
        ((S*)data.success_buf)->~S();
      } else {
        ((E*)data.err_buf)->~E();
      }
    }

    inline operator bool(void) { return success; }

    // unwrap and panic
    inline auto unwrap(void) {
      assert_valid();
      if (!success) {
        // TODO: actually panic
        fprintf(stderr, "Unwrap on error result\n");
        exit(EXIT_FAILURE);
      }


      S val = move(*(S*)&data.success_buf);
      valid = false;
      memset(&data.success_buf, 0, sizeof(S));
      return move(val);
    }

    // default value response
    inline auto unwrap_or(S d) {
      assert_valid();

      if (!success) return d;
      return unwrap();
    }



    inline auto&& err(void) {
      assert_valid();

      if (success) {
        // TODO: actually panic
        fprintf(stderr, "err() on successful result\n");
        exit(EXIT_FAILURE);
      }
      // consume
      valid = false;
      return move(*(E*)&data.err_buf);
    }
  };



  template <typename S, typename E>
  auto Okay(S&& s) {
    return ck::result<S, E>::ok(move(s));
  }


  template <typename S, typename E>
  auto Err(E&& e) {
    return ck::result<S, E>::err(move(e));
  }

}  // namespace ck
