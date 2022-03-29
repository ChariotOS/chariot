#include <cppglue.h>
#include <mem.h>
#include <printf.h>
#include <types.h>

void *__dso_handle;
unsigned __atexit_func_count = 0;
atexit_func_entry_t __atexit_funcs[ATEXIT_MAX_FUNCS];


extern "C" void abort(void) { panic("ABORT"); }
#define BAD() panic("Undefined C++ function (%s)\n", __func__);

// Called when a pure virtual function call is attempted
void __cxa_pure_virtual(void) { BAD(); }

int __cxa_atexit(void (*destructor)(void *), void *arg, void *dso_handle) {
  if (__atexit_func_count >= ATEXIT_MAX_FUNCS) {
    return -1;
  }

  __atexit_funcs[__atexit_func_count].destructor_func = destructor;
  __atexit_funcs[__atexit_func_count].obj_ptr = arg;
  __atexit_funcs[__atexit_func_count].dso_handle = dso_handle;
  __atexit_func_count++;
  return 0;
}

void __cxa_finalize(void *f) {
  unsigned i = __atexit_func_count;
  /* "If f is NULL, it shall call all the termination funtions." */
  if (!f) {
    while (i--) {
      if (__atexit_funcs[i].destructor_func) {
        (*__atexit_funcs[i].destructor_func)(__atexit_funcs[i].obj_ptr);
      }
    }
  } else {
    for (; i != 0; --i) {
      if (__atexit_funcs[i].destructor_func == f) {
        (*__atexit_funcs[i].destructor_func)(__atexit_funcs[i].obj_ptr);
        __atexit_funcs[i].destructor_func = NULL;
      }
    }
  }
}

#if 0
// Not really needed since we will compile with -fno-rtti and -fno-exceptions
void _Unwind_Resume(void) { BAD(); }

extern "C" _Unwind_Reason_Code _Unwind_Resume_or_Rethrow(
    struct _Unwind_Exception *exception_object) {
  BAD();
  return _URC_NO_REASON;
}

extern "C" void _Unwind_SetGR(struct _Unwind_Context *context, int index,
                              uint64_t new_value) {
  BAD();
}

extern "C" uint64_t _Unwind_GetIP(struct _Unwind_Context *context) {
  BAD();
  return 0;
}

extern "C" void _Unwind_SetIP(struct _Unwind_Context *context,
                              uint64_t new_value) {
  BAD();
}

extern "C" uint64_t _Unwind_GetIPInfo(struct _Unwind_Context *context,
                                      int *ip_before_insn) {
  BAD();
  return 0;
}

extern "C" _Unwind_Reason_Code _Unwind_RaiseException(
    struct _Unwind_Exception *exception_object) {
  BAD();
  return _URC_NO_REASON;
}

extern "C" void _Unwind_DeleteException(
    struct _Unwind_Exception *exception_object) {
  BAD();
}

extern "C" uint64_t _Unwind_GetLanguageSpecificData(
    struct _Unwind_Context *context) {
  BAD();
  return 0;
}

extern "C" uint64_t _Unwind_GetRegionStart(struct _Unwind_Context *context) {
  BAD();
  return 0;
}

extern "C" uint64_t _Unwind_GetTextRelBase(struct _Unwind_Context *context) {
  BAD();
  return 0;
}

extern "C" uint64_t _Unwind_GetDataRelBase(struct _Unwind_Context *context) {
  BAD();
  return 0;
}
#endif

void *operator new(size_t size) {
  // printf("operator new %zu\n", size);
  return zalloc(size);
}

void *operator new[](size_t size) { return zalloc(size); }

template <typename T>
void *operator new(size_t size, T *&dst) {
  dst = zalloc(size);
  return dst;
}

void operator delete(void *ptr) { free(ptr); }

void operator delete[](void *ptr) { free(ptr); }

void operator delete(void *ptr, size_t s) { free(ptr); }

void operator delete[](void *ptr, size_t s) { free(ptr); }



extern "C" void __stack_chk_fail(void) { panic("stack pointer smashed!\n"); }
/*
namespace std {

    void __throw_bad_alloc(void) {
        BAD();
    }

    void __throw_length_error(char const * c) {
        BAD();
    }
}
*/


// helper functions for getting/setting flags in guard_object
static bool initializerHasRun(uint64_t *guard_object) { return (*((uint8_t *)guard_object) != 0); }

/*
 * The chariot cxa guard api expects two ints as an array
 * the first is the lock, the second is the status (1 is initialized, 0 is not)
 */
extern "C" int __cxa_guard_acquire(uint64_t *guard_object) {
  // Double check that the initializer has not already been run
  if (initializerHasRun(guard_object)) return 0;

  if (initializerHasRun(guard_object)) {
    return 0;
  }

  // mark this guard object as being in use
  ((uint8_t *)guard_object)[1] = 1;

  // return non-zero to tell caller to run initializer
  return 1;
}


//
// Sets the first byte of the guard_object to a non-zero value.
// Releases any locks acquired by __cxa_guard_acquire().
//
extern "C" void __cxa_guard_release(uint64_t *guard_object) {
  // first mark initalizer as having been run, so
  // other threads won't try to re-run it.
  *((uint8_t *)guard_object) = 1;

  // TODO: release a global mutex
}
