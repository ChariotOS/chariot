#ifndef _INTTYPES_H
#define _INTTYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <features.h>
#include <stdint.h>

#define __NEED_wchar_t
#define __NEED_intmax_t
#include <bits/alltypes.h>

typedef struct {
  intmax_t quot, rem;
} imaxdiv_t;

/*
intmax_t imaxabs(intmax_t);
imaxdiv_t imaxdiv(intmax_t, intmax_t);

intmax_t strtoimax(const char *__restrict, char **__restrict, int);
uintmax_t strtoumax(const char *__restrict, char **__restrict, int);

intmax_t wcstoimax(const wchar_t *__restrict, wchar_t **__restrict, int);
uintmax_t wcstoumax(const wchar_t *__restrict, wchar_t **__restrict, int);
*/

#include <chariot/inttypes.h>

#ifdef __cplusplus
}
#endif

#endif
