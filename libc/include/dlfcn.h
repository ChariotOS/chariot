#pragma once

#ifdef __cplusplus
extern "C" {
#endif



#ifdef _GNU_SOURCE

typedef struct {
  const char *dli_fname; /* Pathname of shared object that
                            contains address */
  void *dli_fbase;       /* Base address at which shared
                            object is loaded */
  const char *dli_sname; /* Name of symbol whose definition
                            overlaps addr */
  void *dli_saddr;       /* Exact address of symbol named
                            in dli_sname */
} Dl_info;

int dladdr(void *addr, Dl_info *info);

int dladdr1(void *addr, Dl_info *info, void **extra_info, int flags);

#endif


#ifdef __cplusplus
}
#endif
