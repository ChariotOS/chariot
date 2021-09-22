/*
 *  Duktape public API for Duktape 2.6.0.
 *
 *  See the API reference for documentation on call semantics.  The exposed,
 *  supported API is between the "BEGIN PUBLIC API" and "END PUBLIC API"
 *  comments.  Other parts of the header are Duktape internal and related to
 *  e.g. platform/compiler/feature detection.
 *
 *  Git commit fffa346eff06a8764b02c31d4336f63a773a95c3 (v2.6.0).
 *  Git branch v2-maintenance.
 *
 *  See Duktape AUTHORS.rst and LICENSE.txt for copyright and
 *  licensing information.
 */

/* LICENSE.txt */
/*
 *  ===============
 *  Duktape license
 *  ===============
 *  
 *  (http://opensource.org/licenses/MIT)
 *  
 *  Copyright (c) 2013-2019 by Duktape authors (see AUTHORS.rst)
 *  
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */

/* AUTHORS.rst */
/*
 *  ===============
 *  Duktape authors
 *  ===============
 *  
 *  Copyright
 *  =========
 *  
 *  Duktape copyrights are held by its authors.  Each author has a copyright
 *  to their contribution, and agrees to irrevocably license the contribution
 *  under the Duktape ``LICENSE.txt``.
 *  
 *  Authors
 *  =======
 *  
 *  Please include an e-mail address, a link to your GitHub profile, or something
 *  similar to allow your contribution to be identified accurately.
 *  
 *  The following people have contributed code, website contents, or Wiki contents,
 *  and agreed to irrevocably license their contributions under the Duktape
 *  ``LICENSE.txt`` (in order of appearance):
 *  
 *  * Sami Vaarala <sami.vaarala@iki.fi>
 *  * Niki Dobrev
 *  * Andreas \u00d6man <andreas@lonelycoder.com>
 *  * L\u00e1szl\u00f3 Lang\u00f3 <llango.u-szeged@partner.samsung.com>
 *  * Legimet <legimet.calc@gmail.com>
 *  * Karl Skomski <karl@skomski.com>
 *  * Bruce Pascoe <fatcerberus1@gmail.com>
 *  * Ren\u00e9 Hollander <rene@rene8888.at>
 *  * Julien Hamaide (https://github.com/crazyjul)
 *  * Sebastian G\u00f6tte (https://github.com/jaseg)
 *  * Tomasz Magulski (https://github.com/magul)
 *  * \D. Bohdan (https://github.com/dbohdan)
 *  * Ond\u0159ej Jirman (https://github.com/megous)
 *  * Sa\u00fal Ibarra Corretg\u00e9 <saghul@gmail.com>
 *  * Jeremy HU <huxingyi@msn.com>
 *  * Ole Andr\u00e9 Vadla Ravn\u00e5s (https://github.com/oleavr)
 *  * Harold Brenes (https://github.com/harold-b)
 *  * Oliver Crow (https://github.com/ocrow)
 *  * Jakub Ch\u0142api\u0144ski (https://github.com/jchlapinski)
 *  * Brett Vickers (https://github.com/beevik)
 *  * Dominik Okwieka (https://github.com/okitec)
 *  * Remko Tron\u00e7on (https://el-tramo.be)
 *  * Romero Malaquias (rbsm@ic.ufal.br)
 *  * Michael Drake <michael.drake@codethink.co.uk>
 *  * Steven Don (https://github.com/shdon)
 *  * Simon Stone (https://github.com/sstone1)
 *  * \J. McC. (https://github.com/jmhmccr)
 *  * Jakub Nowakowski (https://github.com/jimvonmoon)
 *  * Tommy Nguyen (https://github.com/tn0502)
 *  * Fabrice Fontaine (https://github.com/ffontaine)
 *  * Christopher Hiller (https://github.com/boneskull)
 *  * Gonzalo Diethelm (https://github.com/gonzus)
 *  * Michal Kasperek (https://github.com/michalkas)
 *  * Andrew Janke (https://github.com/apjanke)
 *  * Steve Fan (https://github.com/stevefan1999)
 *  * Edward Betts (https://github.com/edwardbetts)
 *  * Ozhan Duz (https://github.com/webfolderio)
 *  * Akos Kiss (https://github.com/akosthekiss)
 *  * TheBrokenRail (https://github.com/TheBrokenRail)
 *  * Jesse Doyle (https://github.com/jessedoyle)
 *  * Gero Kuehn (https://github.com/dc6jgk)
 *  * James Swift (https://github.com/phraemer)
 *  * Luis de Bethencourt (https://github.com/luisbg)
 *  * Ian Whyman (https://github.com/v00d00)
 *  * Rick Sayre (https://github.com/whorfin)
 *  
 *  Other contributions
 *  ===================
 *  
 *  The following people have contributed something other than code (e.g. reported
 *  bugs, provided ideas, etc; roughly in order of appearance):
 *  
 *  * Greg Burns
 *  * Anthony Rabine
 *  * Carlos Costa
 *  * Aur\u00e9lien Bouilland
 *  * Preet Desai (Pris Matic)
 *  * judofyr (http://www.reddit.com/user/judofyr)
 *  * Jason Woofenden
 *  * Micha\u0142 Przyby\u015b
 *  * Anthony Howe
 *  * Conrad Pankoff
 *  * Jim Schimpf
 *  * Rajaran Gaunker (https://github.com/zimbabao)
 *  * Andreas \u00d6man
 *  * Doug Sanden
 *  * Josh Engebretson (https://github.com/JoshEngebretson)
 *  * Remo Eichenberger (https://github.com/remoe)
 *  * Mamod Mehyar (https://github.com/mamod)
 *  * David Demelier (https://github.com/markand)
 *  * Tim Caswell (https://github.com/creationix)
 *  * Mitchell Blank Jr (https://github.com/mitchblank)
 *  * https://github.com/yushli
 *  * Seo Sanghyeon (https://github.com/sanxiyn)
 *  * Han ChoongWoo (https://github.com/tunz)
 *  * Joshua Peek (https://github.com/josh)
 *  * Bruce E. Pascoe (https://github.com/fatcerberus)
 *  * https://github.com/Kelledin
 *  * https://github.com/sstruchtrup
 *  * Michael Drake (https://github.com/tlsa)
 *  * https://github.com/chris-y
 *  * Laurent Zubiaur (https://github.com/lzubiaur)
 *  * Neil Kolban (https://github.com/nkolban)
 *  * Wilhelm Wanecek (https://github.com/wanecek)
 *  * Andrew Janke (https://github.com/apjanke)
 *  * Unamer (https://github.com/unamer)
 *  * Karl Dahlke (eklhad@gmail.com)
 *  
 *  If you are accidentally missing from this list, send me an e-mail
 *  (``sami.vaarala@iki.fi``) and I'll fix the omission.
 */

#if !defined(DUKTAPE_H_INCLUDED)
#define DUKTAPE_H_INCLUDED

#define JS_SINGLE_FILE

/*
 *  BEGIN PUBLIC API
 */

/*
 *  Version and Git commit identification
 */

/* Duktape version, (major * 10000) + (minor * 100) + patch.  Allows C code
 * to #if (JS_VERSION >= NNN) against Duktape API version.  The same value
 * is also available to ECMAScript code in Duktape.version.  Unofficial
 * development snapshots have 99 for patch level (e.g. 0.10.99 would be a
 * development version after 0.10.0 but before the next official release).
 */
#define JS_VERSION                       20600L

/* Git commit, describe, and branch for Duktape build.  Useful for
 * non-official snapshot builds so that application code can easily log
 * which Duktape snapshot was used.  Not available in the ECMAScript
 * environment.
 */
#define JS_GIT_COMMIT                    "fffa346eff06a8764b02c31d4336f63a773a95c3"
#define JS_GIT_DESCRIBE                  "v2.6.0"
#define JS_GIT_BRANCH                    "v2-maintenance"

/* External js_config.h provides platform/compiler/OS dependent
 * typedefs and macros, and JS_USE_xxx config options so that
 * the rest of Duktape doesn't need to do any feature detection.
 * JS_VERSION is defined before including so that configuration
 * snippets can react to it.
 */
#include <js/config.h>

/*
 *  Avoid C++ name mangling
 */

#if defined(__cplusplus)
extern "C" {
#endif

/*
 *  Some defines forwarded from feature detection
 */

#undef JS_API_VARIADIC_MACROS
#if defined(JS_USE_VARIADIC_MACROS)
#define JS_API_VARIADIC_MACROS
#endif

#define JS_API_NORETURN(decl) JS_NORETURN(decl)

/*
 *  Public API specific typedefs
 *
 *  Many types are wrapped by Duktape for portability to rare platforms
 *  where e.g. 'int' is a 16-bit type.  See practical typing discussion
 *  in Duktape web documentation.
 */

struct js_thread_state;
struct js_memory_functions;
struct js_function_list_entry;
struct js_number_list_entry;
struct js_time_components;

/* js_context is now defined in js_config.h because it may also be
 * referenced there by prototypes.
 */
typedef struct js_thread_state js_thread_state;
typedef struct js_memory_functions js_memory_functions;
typedef struct js_function_list_entry js_function_list_entry;
typedef struct js_number_list_entry js_number_list_entry;
typedef struct js_time_components js_time_components;

typedef js_ret_t (*js_c_function)(js_context *ctx);
typedef void *(*js_alloc_function) (void *udata, js_size_t size);
typedef void *(*js_realloc_function) (void *udata, void *ptr, js_size_t size);
typedef void (*js_free_function) (void *udata, void *ptr);
typedef void (*js_fatal_function) (void *udata, const char *msg);
typedef void (*js_decode_char_function) (void *udata, js_codepoint_t codepoint);
typedef js_codepoint_t (*js_map_char_function) (void *udata, js_codepoint_t codepoint);
typedef js_ret_t (*js_safe_call_function) (js_context *ctx, void *udata);
typedef js_size_t (*js_debug_read_function) (void *udata, char *buffer, js_size_t length);
typedef js_size_t (*js_debug_write_function) (void *udata, const char *buffer, js_size_t length);
typedef js_size_t (*js_debug_peek_function) (void *udata);
typedef void (*js_debug_read_flush_function) (void *udata);
typedef void (*js_debug_write_flush_function) (void *udata);
typedef js_idx_t (*js_debug_request_function) (js_context *ctx, void *udata, js_idx_t nvalues);
typedef void (*js_debug_detached_function) (js_context *ctx, void *udata);

struct js_thread_state {
	/* XXX: Enough space to hold internal suspend/resume structure.
	 * This is rather awkward and to be fixed when the internal
	 * structure is visible for the public API header.
	 */
	char data[128];
};

struct js_memory_functions {
	js_alloc_function alloc_func;
	js_realloc_function realloc_func;
	js_free_function free_func;
	void *udata;
};

struct js_function_list_entry {
	const char *key;
	js_c_function value;
	js_idx_t nargs;
};

struct js_number_list_entry {
	const char *key;
	js_double_t value;
};

struct js_time_components {
	js_double_t year;          /* year, e.g. 2016, ECMAScript year range */
	js_double_t month;         /* month: 1-12 */
	js_double_t day;           /* day: 1-31 */
	js_double_t hours;         /* hour: 0-59 */
	js_double_t minutes;       /* minute: 0-59 */
	js_double_t seconds;       /* second: 0-59 (in POSIX time no leap second) */
	js_double_t milliseconds;  /* may contain sub-millisecond fractions */
	js_double_t weekday;       /* weekday: 0-6, 0=Sunday, 1=Monday, ..., 6=Saturday */
};

/*
 *  Constants
 */

/* Duktape debug protocol version used by this build. */
#define JS_DEBUG_PROTOCOL_VERSION        2

/* Used to represent invalid index; if caller uses this without checking,
 * this index will map to a non-existent stack entry.  Also used in some
 * API calls as a marker to denote "no value".
 */
#define JS_INVALID_INDEX                 JS_IDX_MIN

/* Indicates that a native function does not have a fixed number of args,
 * and the argument stack should not be capped/extended at all.
 */
#define JS_VARARGS                       ((js_int_t) (-1))

/* Number of value stack entries (in addition to actual call arguments)
 * guaranteed to be allocated on entry to a Duktape/C function.
 */
#define JS_API_ENTRY_STACK               64U

/* Value types, used by e.g. js_get_type() */
#define JS_TYPE_MIN                      0U
#define JS_TYPE_NONE                     0U    /* no value, e.g. invalid index */
#define JS_TYPE_UNDEFINED                1U    /* ECMAScript undefined */
#define JS_TYPE_NULL                     2U    /* ECMAScript null */
#define JS_TYPE_BOOLEAN                  3U    /* ECMAScript boolean: 0 or 1 */
#define JS_TYPE_NUMBER                   4U    /* ECMAScript number: double */
#define JS_TYPE_STRING                   5U    /* ECMAScript string: CESU-8 / extended UTF-8 encoded */
#define JS_TYPE_OBJECT                   6U    /* ECMAScript object: includes objects, arrays, functions, threads */
#define JS_TYPE_BUFFER                   7U    /* fixed or dynamic, garbage collected byte buffer */
#define JS_TYPE_POINTER                  8U    /* raw void pointer */
#define JS_TYPE_LIGHTFUNC                9U    /* lightweight function pointer */
#define JS_TYPE_MAX                      9U

/* Value mask types, used by e.g. js_get_type_mask() */
#define JS_TYPE_MASK_NONE                (1U << JS_TYPE_NONE)
#define JS_TYPE_MASK_UNDEFINED           (1U << JS_TYPE_UNDEFINED)
#define JS_TYPE_MASK_NULL                (1U << JS_TYPE_NULL)
#define JS_TYPE_MASK_BOOLEAN             (1U << JS_TYPE_BOOLEAN)
#define JS_TYPE_MASK_NUMBER              (1U << JS_TYPE_NUMBER)
#define JS_TYPE_MASK_STRING              (1U << JS_TYPE_STRING)
#define JS_TYPE_MASK_OBJECT              (1U << JS_TYPE_OBJECT)
#define JS_TYPE_MASK_BUFFER              (1U << JS_TYPE_BUFFER)
#define JS_TYPE_MASK_POINTER             (1U << JS_TYPE_POINTER)
#define JS_TYPE_MASK_LIGHTFUNC           (1U << JS_TYPE_LIGHTFUNC)
#define JS_TYPE_MASK_THROW               (1U << 10)  /* internal flag value: throw if mask doesn't match */
#define JS_TYPE_MASK_PROMOTE             (1U << 11)  /* internal flag value: promote to object if mask matches */

/* Coercion hints */
#define JS_HINT_NONE                     0    /* prefer number, unless input is a Date, in which
                                                * case prefer string (E5 Section 8.12.8)
                                                */
#define JS_HINT_STRING                   1    /* prefer string */
#define JS_HINT_NUMBER                   2    /* prefer number */

/* Enumeration flags for js_enum() */
#define JS_ENUM_INCLUDE_NONENUMERABLE    (1U << 0)    /* enumerate non-numerable properties in addition to enumerable */
#define JS_ENUM_INCLUDE_HIDDEN           (1U << 1)    /* enumerate hidden symbols too (in Duktape 1.x called internal properties) */
#define JS_ENUM_INCLUDE_SYMBOLS          (1U << 2)    /* enumerate symbols */
#define JS_ENUM_EXCLUDE_STRINGS          (1U << 3)    /* exclude strings */
#define JS_ENUM_OWN_PROPERTIES_ONLY      (1U << 4)    /* don't walk prototype chain, only check own properties */
#define JS_ENUM_ARRAY_INDICES_ONLY       (1U << 5)    /* only enumerate array indices */
/* XXX: misleading name */
#define JS_ENUM_SORT_ARRAY_INDICES       (1U << 6)    /* sort array indices (applied to full enumeration result, including inherited array indices); XXX: misleading name */
#define JS_ENUM_NO_PROXY_BEHAVIOR        (1U << 7)    /* enumerate a proxy object itself without invoking proxy behavior */

/* Compilation flags for js_compile() and js_eval() */
/* JS_COMPILE_xxx bits 0-2 are reserved for an internal 'nargs' argument.
 */
#define JS_COMPILE_EVAL                  (1U << 3)    /* compile eval code (instead of global code) */
#define JS_COMPILE_FUNCTION              (1U << 4)    /* compile function code (instead of global code) */
#define JS_COMPILE_STRICT                (1U << 5)    /* use strict (outer) context for global, eval, or function code */
#define JS_COMPILE_SHEBANG               (1U << 6)    /* allow shebang ('#! ...') comment on first line of source */
#define JS_COMPILE_SAFE                  (1U << 7)    /* (internal) catch compilation errors */
#define JS_COMPILE_NORESULT              (1U << 8)    /* (internal) omit eval result */
#define JS_COMPILE_NOSOURCE              (1U << 9)    /* (internal) no source string on stack */
#define JS_COMPILE_STRLEN                (1U << 10)   /* (internal) take strlen() of src_buffer (avoids double evaluation in macro) */
#define JS_COMPILE_NOFILENAME            (1U << 11)   /* (internal) no filename on stack */
#define JS_COMPILE_FUNCEXPR              (1U << 12)   /* (internal) source is a function expression (used for Function constructor) */

/* Flags for js_def_prop() and its variants; base flags + a lot of convenience shorthands */
#define JS_DEFPROP_WRITABLE              (1U << 0)    /* set writable (effective if JS_DEFPROP_HAVE_WRITABLE set) */
#define JS_DEFPROP_ENUMERABLE            (1U << 1)    /* set enumerable (effective if JS_DEFPROP_HAVE_ENUMERABLE set) */
#define JS_DEFPROP_CONFIGURABLE          (1U << 2)    /* set configurable (effective if JS_DEFPROP_HAVE_CONFIGURABLE set) */
#define JS_DEFPROP_HAVE_WRITABLE         (1U << 3)    /* set/clear writable */
#define JS_DEFPROP_HAVE_ENUMERABLE       (1U << 4)    /* set/clear enumerable */
#define JS_DEFPROP_HAVE_CONFIGURABLE     (1U << 5)    /* set/clear configurable */
#define JS_DEFPROP_HAVE_VALUE            (1U << 6)    /* set value (given on value stack) */
#define JS_DEFPROP_HAVE_GETTER           (1U << 7)    /* set getter (given on value stack) */
#define JS_DEFPROP_HAVE_SETTER           (1U << 8)    /* set setter (given on value stack) */
#define JS_DEFPROP_FORCE                 (1U << 9)    /* force change if possible, may still fail for e.g. virtual properties */
#define JS_DEFPROP_SET_WRITABLE          (JS_DEFPROP_HAVE_WRITABLE | JS_DEFPROP_WRITABLE)
#define JS_DEFPROP_CLEAR_WRITABLE        JS_DEFPROP_HAVE_WRITABLE
#define JS_DEFPROP_SET_ENUMERABLE        (JS_DEFPROP_HAVE_ENUMERABLE | JS_DEFPROP_ENUMERABLE)
#define JS_DEFPROP_CLEAR_ENUMERABLE      JS_DEFPROP_HAVE_ENUMERABLE
#define JS_DEFPROP_SET_CONFIGURABLE      (JS_DEFPROP_HAVE_CONFIGURABLE | JS_DEFPROP_CONFIGURABLE)
#define JS_DEFPROP_CLEAR_CONFIGURABLE    JS_DEFPROP_HAVE_CONFIGURABLE
#define JS_DEFPROP_W                     JS_DEFPROP_WRITABLE
#define JS_DEFPROP_E                     JS_DEFPROP_ENUMERABLE
#define JS_DEFPROP_C                     JS_DEFPROP_CONFIGURABLE
#define JS_DEFPROP_WE                    (JS_DEFPROP_WRITABLE | JS_DEFPROP_ENUMERABLE)
#define JS_DEFPROP_WC                    (JS_DEFPROP_WRITABLE | JS_DEFPROP_CONFIGURABLE)
#define JS_DEFPROP_EC                    (JS_DEFPROP_ENUMERABLE | JS_DEFPROP_CONFIGURABLE)
#define JS_DEFPROP_WEC                   (JS_DEFPROP_WRITABLE | JS_DEFPROP_ENUMERABLE | JS_DEFPROP_CONFIGURABLE)
#define JS_DEFPROP_HAVE_W                JS_DEFPROP_HAVE_WRITABLE
#define JS_DEFPROP_HAVE_E                JS_DEFPROP_HAVE_ENUMERABLE
#define JS_DEFPROP_HAVE_C                JS_DEFPROP_HAVE_CONFIGURABLE
#define JS_DEFPROP_HAVE_WE               (JS_DEFPROP_HAVE_WRITABLE | JS_DEFPROP_HAVE_ENUMERABLE)
#define JS_DEFPROP_HAVE_WC               (JS_DEFPROP_HAVE_WRITABLE | JS_DEFPROP_HAVE_CONFIGURABLE)
#define JS_DEFPROP_HAVE_EC               (JS_DEFPROP_HAVE_ENUMERABLE | JS_DEFPROP_HAVE_CONFIGURABLE)
#define JS_DEFPROP_HAVE_WEC              (JS_DEFPROP_HAVE_WRITABLE | JS_DEFPROP_HAVE_ENUMERABLE | JS_DEFPROP_HAVE_CONFIGURABLE)
#define JS_DEFPROP_SET_W                 JS_DEFPROP_SET_WRITABLE
#define JS_DEFPROP_SET_E                 JS_DEFPROP_SET_ENUMERABLE
#define JS_DEFPROP_SET_C                 JS_DEFPROP_SET_CONFIGURABLE
#define JS_DEFPROP_SET_WE                (JS_DEFPROP_SET_WRITABLE | JS_DEFPROP_SET_ENUMERABLE)
#define JS_DEFPROP_SET_WC                (JS_DEFPROP_SET_WRITABLE | JS_DEFPROP_SET_CONFIGURABLE)
#define JS_DEFPROP_SET_EC                (JS_DEFPROP_SET_ENUMERABLE | JS_DEFPROP_SET_CONFIGURABLE)
#define JS_DEFPROP_SET_WEC               (JS_DEFPROP_SET_WRITABLE | JS_DEFPROP_SET_ENUMERABLE | JS_DEFPROP_SET_CONFIGURABLE)
#define JS_DEFPROP_CLEAR_W               JS_DEFPROP_CLEAR_WRITABLE
#define JS_DEFPROP_CLEAR_E               JS_DEFPROP_CLEAR_ENUMERABLE
#define JS_DEFPROP_CLEAR_C               JS_DEFPROP_CLEAR_CONFIGURABLE
#define JS_DEFPROP_CLEAR_WE              (JS_DEFPROP_CLEAR_WRITABLE | JS_DEFPROP_CLEAR_ENUMERABLE)
#define JS_DEFPROP_CLEAR_WC              (JS_DEFPROP_CLEAR_WRITABLE | JS_DEFPROP_CLEAR_CONFIGURABLE)
#define JS_DEFPROP_CLEAR_EC              (JS_DEFPROP_CLEAR_ENUMERABLE | JS_DEFPROP_CLEAR_CONFIGURABLE)
#define JS_DEFPROP_CLEAR_WEC             (JS_DEFPROP_CLEAR_WRITABLE | JS_DEFPROP_CLEAR_ENUMERABLE | JS_DEFPROP_CLEAR_CONFIGURABLE)
#define JS_DEFPROP_ATTR_W                (JS_DEFPROP_HAVE_WEC | JS_DEFPROP_W)
#define JS_DEFPROP_ATTR_E                (JS_DEFPROP_HAVE_WEC | JS_DEFPROP_E)
#define JS_DEFPROP_ATTR_C                (JS_DEFPROP_HAVE_WEC | JS_DEFPROP_C)
#define JS_DEFPROP_ATTR_WE               (JS_DEFPROP_HAVE_WEC | JS_DEFPROP_WE)
#define JS_DEFPROP_ATTR_WC               (JS_DEFPROP_HAVE_WEC | JS_DEFPROP_WC)
#define JS_DEFPROP_ATTR_EC               (JS_DEFPROP_HAVE_WEC | JS_DEFPROP_EC)
#define JS_DEFPROP_ATTR_WEC              (JS_DEFPROP_HAVE_WEC | JS_DEFPROP_WEC)

/* Flags for js_push_thread_raw() */
#define JS_THREAD_NEW_GLOBAL_ENV         (1U << 0)    /* create a new global environment */

/* Flags for js_gc() */
#define JS_GC_COMPACT                    (1U << 0)    /* compact heap objects */

/* Error codes (must be 8 bits at most, see js_error.h) */
#define JS_ERR_NONE                      0    /* no error (e.g. from js_get_error_code()) */
#define JS_ERR_ERROR                     1    /* Error */
#define JS_ERR_EVAL_ERROR                2    /* EvalError */
#define JS_ERR_RANGE_ERROR               3    /* RangeError */
#define JS_ERR_REFERENCE_ERROR           4    /* ReferenceError */
#define JS_ERR_SYNTAX_ERROR              5    /* SyntaxError */
#define JS_ERR_TYPE_ERROR                6    /* TypeError */
#define JS_ERR_URI_ERROR                 7    /* URIError */

/* Return codes for C functions (shortcut for throwing an error) */
#define JS_RET_ERROR                     (-JS_ERR_ERROR)
#define JS_RET_EVAL_ERROR                (-JS_ERR_EVAL_ERROR)
#define JS_RET_RANGE_ERROR               (-JS_ERR_RANGE_ERROR)
#define JS_RET_REFERENCE_ERROR           (-JS_ERR_REFERENCE_ERROR)
#define JS_RET_SYNTAX_ERROR              (-JS_ERR_SYNTAX_ERROR)
#define JS_RET_TYPE_ERROR                (-JS_ERR_TYPE_ERROR)
#define JS_RET_URI_ERROR                 (-JS_ERR_URI_ERROR)

/* Return codes for protected calls (js_safe_call(), js_pcall()) */
#define JS_EXEC_SUCCESS                  0
#define JS_EXEC_ERROR                    1

/* Debug levels for JS_USE_DEBUG_WRITE(). */
#define JS_LEVEL_DEBUG                   0
#define JS_LEVEL_DDEBUG                  1
#define JS_LEVEL_DDDEBUG                 2

/*
 *  Macros to create Symbols as C statically constructed strings.
 *
 *  Call e.g. as JS_HIDDEN_SYMBOL("myProperty") <=> ("\xFF" "myProperty").
 *
 *  Local symbols have a unique suffix, caller should take care to avoid
 *  conflicting with the Duktape internal representation by e.g. prepending
 *  a '!' character: JS_LOCAL_SYMBOL("myLocal", "!123").
 *
 *  Note that these can only be used for string constants, not dynamically
 *  created strings.
 *
 *  You shouldn't normally use JS_INTERNAL_SYMBOL() at all.  It is reserved
 *  for Duktape internal symbols only.  There are no versioning guarantees
 *  for internal symbols.
 */

#define JS_HIDDEN_SYMBOL(x)     ("\xFF" x)
#define JS_GLOBAL_SYMBOL(x)     ("\x80" x)
#define JS_LOCAL_SYMBOL(x,uniq) ("\x81" x "\xff" uniq)
#define JS_WELLKNOWN_SYMBOL(x)  ("\x81" x "\xff")
#define JS_INTERNAL_SYMBOL(x)   ("\x82" x)

/*
 *  If no variadic macros, __FILE__ and __LINE__ are passed through globals
 *  which is ugly and not thread safe.
 */

#if !defined(JS_API_VARIADIC_MACROS)
JS_EXTERNAL_DECL const char *js_api_global_filename;
JS_EXTERNAL_DECL js_int_t js_api_global_line;
#endif

/*
 *  Context management
 */

JS_EXTERNAL_DECL
js_context *js_create_heap(js_alloc_function alloc_func,
                             js_realloc_function realloc_func,
                             js_free_function free_func,
                             void *heap_udata,
                             js_fatal_function fatal_handler);
JS_EXTERNAL_DECL void js_destroy_heap(js_context *ctx);

JS_EXTERNAL_DECL void js_suspend(js_context *ctx, js_thread_state *state);
JS_EXTERNAL_DECL void js_resume(js_context *ctx, const js_thread_state *state);

#define js_create_heap_default() \
	js_create_heap(NULL, NULL, NULL, NULL, NULL)

/*
 *  Memory management
 *
 *  Raw functions have no side effects (cannot trigger GC).
 */

JS_EXTERNAL_DECL void *js_alloc_raw(js_context *ctx, js_size_t size);
JS_EXTERNAL_DECL void js_free_raw(js_context *ctx, void *ptr);
JS_EXTERNAL_DECL void *js_realloc_raw(js_context *ctx, void *ptr, js_size_t size);
JS_EXTERNAL_DECL void *js_alloc(js_context *ctx, js_size_t size);
JS_EXTERNAL_DECL void js_free(js_context *ctx, void *ptr);
JS_EXTERNAL_DECL void *js_realloc(js_context *ctx, void *ptr, js_size_t size);
JS_EXTERNAL_DECL void js_get_memory_functions(js_context *ctx, js_memory_functions *out_funcs);
JS_EXTERNAL_DECL void js_gc(js_context *ctx, js_uint_t flags);

/*
 *  Error handling
 */

JS_API_NORETURN(JS_EXTERNAL_DECL void js_throw_raw(js_context *ctx));
#define js_throw(ctx) \
	(js_throw_raw((ctx)), (js_ret_t) 0)
JS_API_NORETURN(JS_EXTERNAL_DECL void js_fatal_raw(js_context *ctx, const char *err_msg));
#define js_fatal(ctx,err_msg) \
	(js_fatal_raw((ctx), (err_msg)), (js_ret_t) 0)
JS_API_NORETURN(JS_EXTERNAL_DECL void js_error_raw(js_context *ctx, js_errcode_t err_code, const char *filename, js_int_t line, const char *fmt, ...));

#if defined(JS_API_VARIADIC_MACROS)
#define js_error(ctx,err_code,...)  \
	(js_error_raw((ctx), (js_errcode_t) (err_code), (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), __VA_ARGS__), (js_ret_t) 0)
#define js_generic_error(ctx,...)  \
	(js_error_raw((ctx), (js_errcode_t) JS_ERR_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), __VA_ARGS__), (js_ret_t) 0)
#define js_eval_error(ctx,...)  \
	(js_error_raw((ctx), (js_errcode_t) JS_ERR_EVAL_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), __VA_ARGS__), (js_ret_t) 0)
#define js_range_error(ctx,...)  \
	(js_error_raw((ctx), (js_errcode_t) JS_ERR_RANGE_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), __VA_ARGS__), (js_ret_t) 0)
#define js_reference_error(ctx,...)  \
	(js_error_raw((ctx), (js_errcode_t) JS_ERR_REFERENCE_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), __VA_ARGS__), (js_ret_t) 0)
#define js_syntax_error(ctx,...)  \
	(js_error_raw((ctx), (js_errcode_t) JS_ERR_SYNTAX_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), __VA_ARGS__), (js_ret_t) 0)
#define js_type_error(ctx,...)  \
	(js_error_raw((ctx), (js_errcode_t) JS_ERR_TYPE_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), __VA_ARGS__), (js_ret_t) 0)
#define js_uri_error(ctx,...)  \
	(js_error_raw((ctx), (js_errcode_t) JS_ERR_URI_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), __VA_ARGS__), (js_ret_t) 0)
#else  /* JS_API_VARIADIC_MACROS */
/* For legacy compilers without variadic macros a macro hack is used to allow
 * variable arguments.  While the macro allows "return js_error(...)", it
 * will fail with e.g. "(void) js_error(...)".  The calls are noreturn but
 * with a return value to allow the "return js_error(...)" idiom.  This may
 * cause some compiler warnings, but without noreturn the generated code is
 * often worse.  The same approach as with variadic macros (using
 * "(js_error(...), 0)") won't work due to the macro hack structure.
 */
JS_API_NORETURN(JS_EXTERNAL_DECL js_ret_t js_error_stash(js_context *ctx, js_errcode_t err_code, const char *fmt, ...));
JS_API_NORETURN(JS_EXTERNAL_DECL js_ret_t js_generic_error_stash(js_context *ctx, const char *fmt, ...));
JS_API_NORETURN(JS_EXTERNAL_DECL js_ret_t js_eval_error_stash(js_context *ctx, const char *fmt, ...));
JS_API_NORETURN(JS_EXTERNAL_DECL js_ret_t js_range_error_stash(js_context *ctx, const char *fmt, ...));
JS_API_NORETURN(JS_EXTERNAL_DECL js_ret_t js_reference_error_stash(js_context *ctx, const char *fmt, ...));
JS_API_NORETURN(JS_EXTERNAL_DECL js_ret_t js_syntax_error_stash(js_context *ctx, const char *fmt, ...));
JS_API_NORETURN(JS_EXTERNAL_DECL js_ret_t js_type_error_stash(js_context *ctx, const char *fmt, ...));
JS_API_NORETURN(JS_EXTERNAL_DECL js_ret_t js_uri_error_stash(js_context *ctx, const char *fmt, ...));
#define js_error  \
	(js_api_global_filename = (const char *) (JS_FILE_MACRO), \
	 js_api_global_line = (js_int_t) (JS_LINE_MACRO), \
	 js_error_stash)  /* last value is func pointer, arguments follow in parens */
#define js_generic_error  \
	(js_api_global_filename = (const char *) (JS_FILE_MACRO), \
	 js_api_global_line = (js_int_t) (JS_LINE_MACRO), \
	 js_generic_error_stash)
#define js_eval_error  \
	(js_api_global_filename = (const char *) (JS_FILE_MACRO), \
	 js_api_global_line = (js_int_t) (JS_LINE_MACRO), \
	 js_eval_error_stash)
#define js_range_error  \
	(js_api_global_filename = (const char *) (JS_FILE_MACRO), \
	 js_api_global_line = (js_int_t) (JS_LINE_MACRO), \
	 js_range_error_stash)
#define js_reference_error  \
	(js_api_global_filename = (const char *) (JS_FILE_MACRO), \
	 js_api_global_line = (js_int_t) (JS_LINE_MACRO), \
	 js_reference_error_stash)
#define js_syntax_error  \
	(js_api_global_filename = (const char *) (JS_FILE_MACRO), \
	 js_api_global_line = (js_int_t) (JS_LINE_MACRO), \
	 js_syntax_error_stash)
#define js_type_error  \
	(js_api_global_filename = (const char *) (JS_FILE_MACRO), \
	 js_api_global_line = (js_int_t) (JS_LINE_MACRO), \
	 js_type_error_stash)
#define js_uri_error  \
	(js_api_global_filename = (const char *) (JS_FILE_MACRO), \
	 js_api_global_line = (js_int_t) (JS_LINE_MACRO), \
	 js_uri_error_stash)
#endif  /* JS_API_VARIADIC_MACROS */

JS_API_NORETURN(JS_EXTERNAL_DECL void js_error_va_raw(js_context *ctx, js_errcode_t err_code, const char *filename, js_int_t line, const char *fmt, va_list ap));

#define js_error_va(ctx,err_code,fmt,ap)  \
	(js_error_va_raw((ctx), (js_errcode_t) (err_code), (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), (fmt), (ap)), (js_ret_t) 0)
#define js_generic_error_va(ctx,fmt,ap)  \
	(js_error_va_raw((ctx), (js_errcode_t) JS_ERR_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), (fmt), (ap)), (js_ret_t) 0)
#define js_eval_error_va(ctx,fmt,ap)  \
	(js_error_va_raw((ctx), (js_errcode_t) JS_ERR_EVAL_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), (fmt), (ap)), (js_ret_t) 0)
#define js_range_error_va(ctx,fmt,ap)  \
	(js_error_va_raw((ctx), (js_errcode_t) JS_ERR_RANGE_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), (fmt), (ap)), (js_ret_t) 0)
#define js_reference_error_va(ctx,fmt,ap)  \
	(js_error_va_raw((ctx), (js_errcode_t) JS_ERR_REFERENCE_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), (fmt), (ap)), (js_ret_t) 0)
#define js_syntax_error_va(ctx,fmt,ap)  \
	(js_error_va_raw((ctx), (js_errcode_t) JS_ERR_SYNTAX_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), (fmt), (ap)), (js_ret_t) 0)
#define js_type_error_va(ctx,fmt,ap)  \
	(js_error_va_raw((ctx), (js_errcode_t) JS_ERR_TYPE_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), (fmt), (ap)), (js_ret_t) 0)
#define js_uri_error_va(ctx,fmt,ap)  \
	(js_error_va_raw((ctx), (js_errcode_t) JS_ERR_URI_ERROR, (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), (fmt), (ap)), (js_ret_t) 0)

/*
 *  Other state related functions
 */

JS_EXTERNAL_DECL js_bool_t js_is_strict_call(js_context *ctx);
JS_EXTERNAL_DECL js_bool_t js_is_constructor_call(js_context *ctx);

/*
 *  Stack management
 */

JS_EXTERNAL_DECL js_idx_t js_normalize_index(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_idx_t js_require_normalize_index(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_valid_index(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_require_valid_index(js_context *ctx, js_idx_t idx);

JS_EXTERNAL_DECL js_idx_t js_get_top(js_context *ctx);
JS_EXTERNAL_DECL void js_set_top(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_idx_t js_get_top_index(js_context *ctx);
JS_EXTERNAL_DECL js_idx_t js_require_top_index(js_context *ctx);

/* Although extra/top could be an unsigned type here, using a signed type
 * makes the API more robust to calling code calculation errors or corner
 * cases (where caller might occasionally come up with negative values).
 * Negative values are treated as zero, which is better than casting them
 * to a large unsigned number.  (This principle is used elsewhere in the
 * API too.)
 */
JS_EXTERNAL_DECL js_bool_t js_check_stack(js_context *ctx, js_idx_t extra);
JS_EXTERNAL_DECL void js_require_stack(js_context *ctx, js_idx_t extra);
JS_EXTERNAL_DECL js_bool_t js_check_stack_top(js_context *ctx, js_idx_t top);
JS_EXTERNAL_DECL void js_require_stack_top(js_context *ctx, js_idx_t top);

/*
 *  Stack manipulation (other than push/pop)
 */

JS_EXTERNAL_DECL void js_swap(js_context *ctx, js_idx_t idx1, js_idx_t idx2);
JS_EXTERNAL_DECL void js_swap_top(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_dup(js_context *ctx, js_idx_t from_idx);
JS_EXTERNAL_DECL void js_dup_top(js_context *ctx);
JS_EXTERNAL_DECL void js_insert(js_context *ctx, js_idx_t to_idx);
JS_EXTERNAL_DECL void js_pull(js_context *ctx, js_idx_t from_idx);
JS_EXTERNAL_DECL void js_replace(js_context *ctx, js_idx_t to_idx);
JS_EXTERNAL_DECL void js_copy(js_context *ctx, js_idx_t from_idx, js_idx_t to_idx);
JS_EXTERNAL_DECL void js_remove(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_xcopymove_raw(js_context *to_ctx, js_context *from_ctx, js_idx_t count, js_bool_t is_copy);

#define js_xmove_top(to_ctx,from_ctx,count) \
	js_xcopymove_raw((to_ctx), (from_ctx), (count), 0 /*is_copy*/)
#define js_xcopy_top(to_ctx,from_ctx,count) \
	js_xcopymove_raw((to_ctx), (from_ctx), (count), 1 /*is_copy*/)

/*
 *  Push operations
 *
 *  Push functions return the absolute (relative to bottom of frame)
 *  position of the pushed value for convenience.
 *
 *  Note: js_dup() is technically a push.
 */

JS_EXTERNAL_DECL void js_push_undefined(js_context *ctx);
JS_EXTERNAL_DECL void js_push_null(js_context *ctx);
JS_EXTERNAL_DECL void js_push_boolean(js_context *ctx, js_bool_t val);
JS_EXTERNAL_DECL void js_push_true(js_context *ctx);
JS_EXTERNAL_DECL void js_push_false(js_context *ctx);
JS_EXTERNAL_DECL void js_push_number(js_context *ctx, js_double_t val);
JS_EXTERNAL_DECL void js_push_nan(js_context *ctx);
JS_EXTERNAL_DECL void js_push_int(js_context *ctx, js_int_t val);
JS_EXTERNAL_DECL void js_push_uint(js_context *ctx, js_uint_t val);
JS_EXTERNAL_DECL const char *js_push_string(js_context *ctx, const char *str);
JS_EXTERNAL_DECL const char *js_push_lstring(js_context *ctx, const char *str, js_size_t len);
JS_EXTERNAL_DECL void js_push_pointer(js_context *ctx, void *p);
JS_EXTERNAL_DECL const char *js_push_sprintf(js_context *ctx, const char *fmt, ...);
JS_EXTERNAL_DECL const char *js_push_vsprintf(js_context *ctx, const char *fmt, va_list ap);

/* js_push_literal() may evaluate its argument (a C string literal) more than
 * once on purpose.  When speed is preferred, sizeof() avoids an unnecessary
 * strlen() at runtime.  Sizeof("foo") == 4, so subtract 1.  The argument
 * must be non-NULL and should not contain internal NUL characters as the
 * behavior will then depend on config options.
 */
#if defined(JS_USE_PREFER_SIZE)
#define js_push_literal(ctx,cstring)  js_push_string((ctx), (cstring))
#else
JS_EXTERNAL_DECL const char *js_push_literal_raw(js_context *ctx, const char *str, js_size_t len);
#define js_push_literal(ctx,cstring)  js_push_literal_raw((ctx), (cstring), sizeof((cstring)) - 1U)
#endif

JS_EXTERNAL_DECL void js_push_this(js_context *ctx);
JS_EXTERNAL_DECL void js_push_new_target(js_context *ctx);
JS_EXTERNAL_DECL void js_push_current_function(js_context *ctx);
JS_EXTERNAL_DECL void js_push_current_thread(js_context *ctx);
JS_EXTERNAL_DECL void js_push_global_object(js_context *ctx);
JS_EXTERNAL_DECL void js_push_heap_stash(js_context *ctx);
JS_EXTERNAL_DECL void js_push_global_stash(js_context *ctx);
JS_EXTERNAL_DECL void js_push_thread_stash(js_context *ctx, js_context *target_ctx);

JS_EXTERNAL_DECL js_idx_t js_push_object(js_context *ctx);
JS_EXTERNAL_DECL js_idx_t js_push_bare_object(js_context *ctx);
JS_EXTERNAL_DECL js_idx_t js_push_array(js_context *ctx);
JS_EXTERNAL_DECL js_idx_t js_push_bare_array(js_context *ctx);
JS_EXTERNAL_DECL js_idx_t js_push_c_function(js_context *ctx, js_c_function func, js_idx_t nargs);
JS_EXTERNAL_DECL js_idx_t js_push_c_lightfunc(js_context *ctx, js_c_function func, js_idx_t nargs, js_idx_t length, js_int_t magic);
JS_EXTERNAL_DECL js_idx_t js_push_thread_raw(js_context *ctx, js_uint_t flags);
JS_EXTERNAL_DECL js_idx_t js_push_proxy(js_context *ctx, js_uint_t proxy_flags);

#define js_push_thread(ctx) \
	js_push_thread_raw((ctx), 0 /*flags*/)

#define js_push_thread_new_globalenv(ctx) \
	js_push_thread_raw((ctx), JS_THREAD_NEW_GLOBAL_ENV /*flags*/)

JS_EXTERNAL_DECL js_idx_t js_push_error_object_raw(js_context *ctx, js_errcode_t err_code, const char *filename, js_int_t line, const char *fmt, ...);

#if defined(JS_API_VARIADIC_MACROS)
#define js_push_error_object(ctx,err_code,...)  \
	js_push_error_object_raw((ctx), (err_code), (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), __VA_ARGS__)
#else
JS_EXTERNAL_DECL js_idx_t js_push_error_object_stash(js_context *ctx, js_errcode_t err_code, const char *fmt, ...);
/* Note: parentheses are required so that the comma expression works in assignments. */
#define js_push_error_object  \
	(js_api_global_filename = (const char *) (JS_FILE_MACRO), \
	 js_api_global_line = (js_int_t) (JS_LINE_MACRO), \
	 js_push_error_object_stash)  /* last value is func pointer, arguments follow in parens */
#endif

JS_EXTERNAL_DECL js_idx_t js_push_error_object_va_raw(js_context *ctx, js_errcode_t err_code, const char *filename, js_int_t line, const char *fmt, va_list ap);
#define js_push_error_object_va(ctx,err_code,fmt,ap)  \
	js_push_error_object_va_raw((ctx), (err_code), (const char *) (JS_FILE_MACRO), (js_int_t) (JS_LINE_MACRO), (fmt), (ap))

#define JS_BUF_FLAG_DYNAMIC   (1 << 0)    /* internal flag: dynamic buffer */
#define JS_BUF_FLAG_EXTERNAL  (1 << 1)    /* internal flag: external buffer */
#define JS_BUF_FLAG_NOZERO    (1 << 2)    /* internal flag: don't zero allocated buffer */

JS_EXTERNAL_DECL void *js_push_buffer_raw(js_context *ctx, js_size_t size, js_small_uint_t flags);

#define js_push_buffer(ctx,size,dynamic) \
	js_push_buffer_raw((ctx), (size), (dynamic) ? JS_BUF_FLAG_DYNAMIC : 0)
#define js_push_fixed_buffer(ctx,size) \
	js_push_buffer_raw((ctx), (size), 0 /*flags*/)
#define js_push_dynamic_buffer(ctx,size) \
	js_push_buffer_raw((ctx), (size), JS_BUF_FLAG_DYNAMIC /*flags*/)
#define js_push_external_buffer(ctx) \
	((void) js_push_buffer_raw((ctx), 0, JS_BUF_FLAG_DYNAMIC | JS_BUF_FLAG_EXTERNAL))

#define JS_BUFOBJ_ARRAYBUFFER         0
#define JS_BUFOBJ_NODEJS_BUFFER       1
#define JS_BUFOBJ_DATAVIEW            2
#define JS_BUFOBJ_INT8ARRAY           3
#define JS_BUFOBJ_UINT8ARRAY          4
#define JS_BUFOBJ_UINT8CLAMPEDARRAY   5
#define JS_BUFOBJ_INT16ARRAY          6
#define JS_BUFOBJ_UINT16ARRAY         7
#define JS_BUFOBJ_INT32ARRAY          8
#define JS_BUFOBJ_UINT32ARRAY         9
#define JS_BUFOBJ_FLOAT32ARRAY        10
#define JS_BUFOBJ_FLOAT64ARRAY        11

JS_EXTERNAL_DECL void js_push_buffer_object(js_context *ctx, js_idx_t idx_buffer, js_size_t byte_offset, js_size_t byte_length, js_uint_t flags);

JS_EXTERNAL_DECL js_idx_t js_push_heapptr(js_context *ctx, void *ptr);

/*
 *  Pop operations
 */

JS_EXTERNAL_DECL void js_pop(js_context *ctx);
JS_EXTERNAL_DECL void js_pop_n(js_context *ctx, js_idx_t count);
JS_EXTERNAL_DECL void js_pop_2(js_context *ctx);
JS_EXTERNAL_DECL void js_pop_3(js_context *ctx);

/*
 *  Type checks
 *
 *  js_is_none(), which would indicate whether index it outside of stack,
 *  is not needed; js_is_valid_index() gives the same information.
 */

JS_EXTERNAL_DECL js_int_t js_get_type(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_check_type(js_context *ctx, js_idx_t idx, js_int_t type);
JS_EXTERNAL_DECL js_uint_t js_get_type_mask(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_check_type_mask(js_context *ctx, js_idx_t idx, js_uint_t mask);

JS_EXTERNAL_DECL js_bool_t js_is_undefined(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_null(js_context *ctx, js_idx_t idx);
#define js_is_null_or_undefined(ctx, idx) \
	((js_get_type_mask((ctx), (idx)) & (JS_TYPE_MASK_NULL | JS_TYPE_MASK_UNDEFINED)) ? 1 : 0)

JS_EXTERNAL_DECL js_bool_t js_is_boolean(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_number(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_nan(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_string(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_object(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_buffer(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_buffer_data(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_pointer(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_lightfunc(js_context *ctx, js_idx_t idx);

JS_EXTERNAL_DECL js_bool_t js_is_symbol(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_array(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_function(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_c_function(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_ecmascript_function(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_bound_function(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_thread(js_context *ctx, js_idx_t idx);

#define js_is_callable(ctx,idx) \
	js_is_function((ctx), (idx))
JS_EXTERNAL_DECL js_bool_t js_is_constructable(js_context *ctx, js_idx_t idx);

JS_EXTERNAL_DECL js_bool_t js_is_dynamic_buffer(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_fixed_buffer(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_is_external_buffer(js_context *ctx, js_idx_t idx);

/* Buffers and lightfuncs are not considered primitive because they mimic
 * objects and e.g. js_to_primitive() will coerce them instead of returning
 * them as is.  Symbols are represented as strings internally.
 */
#define js_is_primitive(ctx,idx) \
	js_check_type_mask((ctx), (idx), JS_TYPE_MASK_UNDEFINED | \
	                                  JS_TYPE_MASK_NULL | \
	                                  JS_TYPE_MASK_BOOLEAN | \
	                                  JS_TYPE_MASK_NUMBER | \
	                                  JS_TYPE_MASK_STRING | \
	                                  JS_TYPE_MASK_POINTER)

/* Symbols are object coercible, covered by JS_TYPE_MASK_STRING. */
#define js_is_object_coercible(ctx,idx) \
	js_check_type_mask((ctx), (idx), JS_TYPE_MASK_BOOLEAN | \
	                                  JS_TYPE_MASK_NUMBER | \
	                                  JS_TYPE_MASK_STRING | \
	                                  JS_TYPE_MASK_OBJECT | \
	                                  JS_TYPE_MASK_BUFFER | \
	                                  JS_TYPE_MASK_POINTER | \
	                                  JS_TYPE_MASK_LIGHTFUNC)

JS_EXTERNAL_DECL js_errcode_t js_get_error_code(js_context *ctx, js_idx_t idx);
#define js_is_error(ctx,idx) \
	(js_get_error_code((ctx), (idx)) != 0)
#define js_is_eval_error(ctx,idx) \
	(js_get_error_code((ctx), (idx)) == JS_ERR_EVAL_ERROR)
#define js_is_range_error(ctx,idx) \
	(js_get_error_code((ctx), (idx)) == JS_ERR_RANGE_ERROR)
#define js_is_reference_error(ctx,idx) \
	(js_get_error_code((ctx), (idx)) == JS_ERR_REFERENCE_ERROR)
#define js_is_syntax_error(ctx,idx) \
	(js_get_error_code((ctx), (idx)) == JS_ERR_SYNTAX_ERROR)
#define js_is_type_error(ctx,idx) \
	(js_get_error_code((ctx), (idx)) == JS_ERR_TYPE_ERROR)
#define js_is_uri_error(ctx,idx) \
	(js_get_error_code((ctx), (idx)) == JS_ERR_URI_ERROR)

/*
 *  Get operations: no coercion, returns default value for invalid
 *  indices and invalid value types.
 *
 *  js_get_undefined() and js_get_null() would be pointless and
 *  are not included.
 */

JS_EXTERNAL_DECL js_bool_t js_get_boolean(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_double_t js_get_number(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_int_t js_get_int(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_uint_t js_get_uint(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL const char *js_get_string(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL const char *js_get_lstring(js_context *ctx, js_idx_t idx, js_size_t *out_len);
JS_EXTERNAL_DECL void *js_get_buffer(js_context *ctx, js_idx_t idx, js_size_t *out_size);
JS_EXTERNAL_DECL void *js_get_buffer_data(js_context *ctx, js_idx_t idx, js_size_t *out_size);
JS_EXTERNAL_DECL void *js_get_pointer(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_c_function js_get_c_function(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_context *js_get_context(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void *js_get_heapptr(js_context *ctx, js_idx_t idx);

/*
 *  Get-with-explicit default operations: like get operations but with an
 *  explicit default value.
 */

JS_EXTERNAL_DECL js_bool_t js_get_boolean_default(js_context *ctx, js_idx_t idx, js_bool_t def_value);
JS_EXTERNAL_DECL js_double_t js_get_number_default(js_context *ctx, js_idx_t idx, js_double_t def_value);
JS_EXTERNAL_DECL js_int_t js_get_int_default(js_context *ctx, js_idx_t idx, js_int_t def_value);
JS_EXTERNAL_DECL js_uint_t js_get_uint_default(js_context *ctx, js_idx_t idx, js_uint_t def_value);
JS_EXTERNAL_DECL const char *js_get_string_default(js_context *ctx, js_idx_t idx, const char *def_value);
JS_EXTERNAL_DECL const char *js_get_lstring_default(js_context *ctx, js_idx_t idx, js_size_t *out_len, const char *def_ptr, js_size_t def_len);
JS_EXTERNAL_DECL void *js_get_buffer_default(js_context *ctx, js_idx_t idx, js_size_t *out_size, void *def_ptr, js_size_t def_len);
JS_EXTERNAL_DECL void *js_get_buffer_data_default(js_context *ctx, js_idx_t idx, js_size_t *out_size, void *def_ptr, js_size_t def_len);
JS_EXTERNAL_DECL void *js_get_pointer_default(js_context *ctx, js_idx_t idx, void *def_value);
JS_EXTERNAL_DECL js_c_function js_get_c_function_default(js_context *ctx, js_idx_t idx, js_c_function def_value);
JS_EXTERNAL_DECL js_context *js_get_context_default(js_context *ctx, js_idx_t idx, js_context *def_value);
JS_EXTERNAL_DECL void *js_get_heapptr_default(js_context *ctx, js_idx_t idx, void *def_value);

/*
 *  Opt operations: like require operations but with an explicit default value
 *  when value is undefined or index is invalid, null and non-matching types
 *  cause a TypeError.
 */

JS_EXTERNAL_DECL js_bool_t js_opt_boolean(js_context *ctx, js_idx_t idx, js_bool_t def_value);
JS_EXTERNAL_DECL js_double_t js_opt_number(js_context *ctx, js_idx_t idx, js_double_t def_value);
JS_EXTERNAL_DECL js_int_t js_opt_int(js_context *ctx, js_idx_t idx, js_int_t def_value);
JS_EXTERNAL_DECL js_uint_t js_opt_uint(js_context *ctx, js_idx_t idx, js_uint_t def_value);
JS_EXTERNAL_DECL const char *js_opt_string(js_context *ctx, js_idx_t idx, const char *def_ptr);
JS_EXTERNAL_DECL const char *js_opt_lstring(js_context *ctx, js_idx_t idx, js_size_t *out_len, const char *def_ptr, js_size_t def_len);
JS_EXTERNAL_DECL void *js_opt_buffer(js_context *ctx, js_idx_t idx, js_size_t *out_size, void *def_ptr, js_size_t def_size);
JS_EXTERNAL_DECL void *js_opt_buffer_data(js_context *ctx, js_idx_t idx, js_size_t *out_size, void *def_ptr, js_size_t def_size);
JS_EXTERNAL_DECL void *js_opt_pointer(js_context *ctx, js_idx_t idx, void *def_value);
JS_EXTERNAL_DECL js_c_function js_opt_c_function(js_context *ctx, js_idx_t idx, js_c_function def_value);
JS_EXTERNAL_DECL js_context *js_opt_context(js_context *ctx, js_idx_t idx, js_context *def_value);
JS_EXTERNAL_DECL void *js_opt_heapptr(js_context *ctx, js_idx_t idx, void *def_value);

/*
 *  Require operations: no coercion, throw error if index or type
 *  is incorrect.  No defaulting.
 */

#define js_require_type_mask(ctx,idx,mask) \
	((void) js_check_type_mask((ctx), (idx), (mask) | JS_TYPE_MASK_THROW))

JS_EXTERNAL_DECL void js_require_undefined(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_require_null(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_require_boolean(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_double_t js_require_number(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_int_t js_require_int(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_uint_t js_require_uint(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL const char *js_require_string(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL const char *js_require_lstring(js_context *ctx, js_idx_t idx, js_size_t *out_len);
JS_EXTERNAL_DECL void js_require_object(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void *js_require_buffer(js_context *ctx, js_idx_t idx, js_size_t *out_size);
JS_EXTERNAL_DECL void *js_require_buffer_data(js_context *ctx, js_idx_t idx, js_size_t *out_size);
JS_EXTERNAL_DECL void *js_require_pointer(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_c_function js_require_c_function(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_context *js_require_context(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_require_function(js_context *ctx, js_idx_t idx);
#define js_require_callable(ctx,idx) \
	js_require_function((ctx), (idx))
JS_EXTERNAL_DECL void js_require_constructor_call(js_context *ctx);
JS_EXTERNAL_DECL void js_require_constructable(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void *js_require_heapptr(js_context *ctx, js_idx_t idx);

/* Symbols are object coercible and covered by JS_TYPE_MASK_STRING. */
#define js_require_object_coercible(ctx,idx) \
	((void) js_check_type_mask((ctx), (idx), JS_TYPE_MASK_BOOLEAN | \
	                                            JS_TYPE_MASK_NUMBER | \
	                                            JS_TYPE_MASK_STRING | \
	                                            JS_TYPE_MASK_OBJECT | \
	                                            JS_TYPE_MASK_BUFFER | \
	                                            JS_TYPE_MASK_POINTER | \
	                                            JS_TYPE_MASK_LIGHTFUNC | \
	                                            JS_TYPE_MASK_THROW))

/*
 *  Coercion operations: in-place coercion, return coerced value where
 *  applicable.  If index is invalid, throw error.  Some coercions may
 *  throw an expected error (e.g. from a toString() or valueOf() call)
 *  or an internal error (e.g. from out of memory).
 */

JS_EXTERNAL_DECL void js_to_undefined(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_to_null(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_bool_t js_to_boolean(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_double_t js_to_number(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_int_t js_to_int(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_uint_t js_to_uint(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_int32_t js_to_int32(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_uint32_t js_to_uint32(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_uint16_t js_to_uint16(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL const char *js_to_string(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL const char *js_to_lstring(js_context *ctx, js_idx_t idx, js_size_t *out_len);
JS_EXTERNAL_DECL void *js_to_buffer_raw(js_context *ctx, js_idx_t idx, js_size_t *out_size, js_uint_t flags);
JS_EXTERNAL_DECL void *js_to_pointer(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_to_object(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_to_primitive(js_context *ctx, js_idx_t idx, js_int_t hint);

#define JS_BUF_MODE_FIXED      0   /* internal: request fixed buffer result */
#define JS_BUF_MODE_DYNAMIC    1   /* internal: request dynamic buffer result */
#define JS_BUF_MODE_DONTCARE   2   /* internal: don't care about fixed/dynamic nature */

#define js_to_buffer(ctx,idx,out_size) \
	js_to_buffer_raw((ctx), (idx), (out_size), JS_BUF_MODE_DONTCARE)
#define js_to_fixed_buffer(ctx,idx,out_size) \
	js_to_buffer_raw((ctx), (idx), (out_size), JS_BUF_MODE_FIXED)
#define js_to_dynamic_buffer(ctx,idx,out_size) \
	js_to_buffer_raw((ctx), (idx), (out_size), JS_BUF_MODE_DYNAMIC)

/* safe variants of a few coercion operations */
JS_EXTERNAL_DECL const char *js_safe_to_lstring(js_context *ctx, js_idx_t idx, js_size_t *out_len);
JS_EXTERNAL_DECL const char *js_to_stacktrace(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL const char *js_safe_to_stacktrace(js_context *ctx, js_idx_t idx);
#define js_safe_to_string(ctx,idx) \
	js_safe_to_lstring((ctx), (idx), NULL)

/*
 *  Value length
 */

JS_EXTERNAL_DECL js_size_t js_get_length(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_set_length(js_context *ctx, js_idx_t idx, js_size_t len);
#if 0
/* js_require_length()? */
/* js_opt_length()? */
#endif

/*
 *  Misc conversion
 */

JS_EXTERNAL_DECL const char *js_base64_encode(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_base64_decode(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL const char *js_hex_encode(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_hex_decode(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL const char *js_json_encode(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_json_decode(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_cbor_encode(js_context *ctx, js_idx_t idx, js_uint_t encode_flags);
JS_EXTERNAL_DECL void js_cbor_decode(js_context *ctx, js_idx_t idx, js_uint_t decode_flags);

JS_EXTERNAL_DECL const char *js_buffer_to_string(js_context *ctx, js_idx_t idx);

/*
 *  Buffer
 */

JS_EXTERNAL_DECL void *js_resize_buffer(js_context *ctx, js_idx_t idx, js_size_t new_size);
JS_EXTERNAL_DECL void *js_steal_buffer(js_context *ctx, js_idx_t idx, js_size_t *out_size);
JS_EXTERNAL_DECL void js_config_buffer(js_context *ctx, js_idx_t idx, void *ptr, js_size_t len);

/*
 *  Property access
 *
 *  The basic function assumes key is on stack.  The _(l)string variant takes
 *  a C string as a property name; the _literal variant takes a C literal.
 *  The _index variant takes an array index as a property name (e.g. 123 is
 *  equivalent to the key "123").  The _heapptr variant takes a raw, borrowed
 *  heap pointer.
 */

JS_EXTERNAL_DECL js_bool_t js_get_prop(js_context *ctx, js_idx_t obj_idx);
JS_EXTERNAL_DECL js_bool_t js_get_prop_string(js_context *ctx, js_idx_t obj_idx, const char *key);
JS_EXTERNAL_DECL js_bool_t js_get_prop_lstring(js_context *ctx, js_idx_t obj_idx, const char *key, js_size_t key_len);
#if defined(JS_USE_PREFER_SIZE)
#define js_get_prop_literal(ctx,obj_idx,key)  js_get_prop_string((ctx), (obj_idx), (key))
#else
JS_EXTERNAL_DECL js_bool_t js_get_prop_literal_raw(js_context *ctx, js_idx_t obj_idx, const char *key, js_size_t key_len);
#define js_get_prop_literal(ctx,obj_idx,key)  js_get_prop_literal_raw((ctx), (obj_idx), (key), sizeof((key)) - 1U)
#endif
JS_EXTERNAL_DECL js_bool_t js_get_prop_index(js_context *ctx, js_idx_t obj_idx, js_uarridx_t arr_idx);
JS_EXTERNAL_DECL js_bool_t js_get_prop_heapptr(js_context *ctx, js_idx_t obj_idx, void *ptr);
JS_EXTERNAL_DECL js_bool_t js_put_prop(js_context *ctx, js_idx_t obj_idx);
JS_EXTERNAL_DECL js_bool_t js_put_prop_string(js_context *ctx, js_idx_t obj_idx, const char *key);
JS_EXTERNAL_DECL js_bool_t js_put_prop_lstring(js_context *ctx, js_idx_t obj_idx, const char *key, js_size_t key_len);
#if defined(JS_USE_PREFER_SIZE)
#define js_put_prop_literal(ctx,obj_idx,key)  js_put_prop_string((ctx), (obj_idx), (key))
#else
JS_EXTERNAL_DECL js_bool_t js_put_prop_literal_raw(js_context *ctx, js_idx_t obj_idx, const char *key, js_size_t key_len);
#define js_put_prop_literal(ctx,obj_idx,key)  js_put_prop_literal_raw((ctx), (obj_idx), (key), sizeof((key)) - 1U)
#endif
JS_EXTERNAL_DECL js_bool_t js_put_prop_index(js_context *ctx, js_idx_t obj_idx, js_uarridx_t arr_idx);
JS_EXTERNAL_DECL js_bool_t js_put_prop_heapptr(js_context *ctx, js_idx_t obj_idx, void *ptr);
JS_EXTERNAL_DECL js_bool_t js_del_prop(js_context *ctx, js_idx_t obj_idx);
JS_EXTERNAL_DECL js_bool_t js_del_prop_string(js_context *ctx, js_idx_t obj_idx, const char *key);
JS_EXTERNAL_DECL js_bool_t js_del_prop_lstring(js_context *ctx, js_idx_t obj_idx, const char *key, js_size_t key_len);
#if defined(JS_USE_PREFER_SIZE)
#define js_del_prop_literal(ctx,obj_idx,key)  js_del_prop_string((ctx), (obj_idx), (key))
#else
JS_EXTERNAL_DECL js_bool_t js_del_prop_literal_raw(js_context *ctx, js_idx_t obj_idx, const char *key, js_size_t key_len);
#define js_del_prop_literal(ctx,obj_idx,key)  js_del_prop_literal_raw((ctx), (obj_idx), (key), sizeof((key)) - 1U)
#endif
JS_EXTERNAL_DECL js_bool_t js_del_prop_index(js_context *ctx, js_idx_t obj_idx, js_uarridx_t arr_idx);
JS_EXTERNAL_DECL js_bool_t js_del_prop_heapptr(js_context *ctx, js_idx_t obj_idx, void *ptr);
JS_EXTERNAL_DECL js_bool_t js_has_prop(js_context *ctx, js_idx_t obj_idx);
JS_EXTERNAL_DECL js_bool_t js_has_prop_string(js_context *ctx, js_idx_t obj_idx, const char *key);
JS_EXTERNAL_DECL js_bool_t js_has_prop_lstring(js_context *ctx, js_idx_t obj_idx, const char *key, js_size_t key_len);
#if defined(JS_USE_PREFER_SIZE)
#define js_has_prop_literal(ctx,obj_idx,key)  js_has_prop_string((ctx), (obj_idx), (key))
#else
JS_EXTERNAL_DECL js_bool_t js_has_prop_literal_raw(js_context *ctx, js_idx_t obj_idx, const char *key, js_size_t key_len);
#define js_has_prop_literal(ctx,obj_idx,key)  js_has_prop_literal_raw((ctx), (obj_idx), (key), sizeof((key)) - 1U)
#endif
JS_EXTERNAL_DECL js_bool_t js_has_prop_index(js_context *ctx, js_idx_t obj_idx, js_uarridx_t arr_idx);
JS_EXTERNAL_DECL js_bool_t js_has_prop_heapptr(js_context *ctx, js_idx_t obj_idx, void *ptr);

JS_EXTERNAL_DECL void js_get_prop_desc(js_context *ctx, js_idx_t obj_idx, js_uint_t flags);
JS_EXTERNAL_DECL void js_def_prop(js_context *ctx, js_idx_t obj_idx, js_uint_t flags);

JS_EXTERNAL_DECL js_bool_t js_get_global_string(js_context *ctx, const char *key);
JS_EXTERNAL_DECL js_bool_t js_get_global_lstring(js_context *ctx, const char *key, js_size_t key_len);
#if defined(JS_USE_PREFER_SIZE)
#define js_get_global_literal(ctx,key)  js_get_global_string((ctx), (key))
#else
JS_EXTERNAL_DECL js_bool_t js_get_global_literal_raw(js_context *ctx, const char *key, js_size_t key_len);
#define js_get_global_literal(ctx,key)  js_get_global_literal_raw((ctx), (key), sizeof((key)) - 1U)
#endif
JS_EXTERNAL_DECL js_bool_t js_get_global_heapptr(js_context *ctx, void *ptr);
JS_EXTERNAL_DECL js_bool_t js_put_global_string(js_context *ctx, const char *key);
JS_EXTERNAL_DECL js_bool_t js_put_global_lstring(js_context *ctx, const char *key, js_size_t key_len);
#if defined(JS_USE_PREFER_SIZE)
#define js_put_global_literal(ctx,key)  js_put_global_string((ctx), (key))
#else
JS_EXTERNAL_DECL js_bool_t js_put_global_literal_raw(js_context *ctx, const char *key, js_size_t key_len);
#define js_put_global_literal(ctx,key)  js_put_global_literal_raw((ctx), (key), sizeof((key)) - 1U)
#endif
JS_EXTERNAL_DECL js_bool_t js_put_global_heapptr(js_context *ctx, void *ptr);

/*
 *  Inspection
 */

JS_EXTERNAL_DECL void js_inspect_value(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_inspect_callstack_entry(js_context *ctx, js_int_t level);

/*
 *  Object prototype
 */

JS_EXTERNAL_DECL void js_get_prototype(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_set_prototype(js_context *ctx, js_idx_t idx);

/*
 *  Object finalizer
 */

JS_EXTERNAL_DECL void js_get_finalizer(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_set_finalizer(js_context *ctx, js_idx_t idx);

/*
 *  Global object
 */

JS_EXTERNAL_DECL void js_set_global_object(js_context *ctx);

/*
 *  Duktape/C function magic value
 */

JS_EXTERNAL_DECL js_int_t js_get_magic(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL void js_set_magic(js_context *ctx, js_idx_t idx, js_int_t magic);
JS_EXTERNAL_DECL js_int_t js_get_current_magic(js_context *ctx);

/*
 *  Module helpers: put multiple function or constant properties
 */

JS_EXTERNAL_DECL void js_put_function_list(js_context *ctx, js_idx_t obj_idx, const js_function_list_entry *funcs);
JS_EXTERNAL_DECL void js_put_number_list(js_context *ctx, js_idx_t obj_idx, const js_number_list_entry *numbers);

/*
 *  Object operations
 */

JS_EXTERNAL_DECL void js_compact(js_context *ctx, js_idx_t obj_idx);
JS_EXTERNAL_DECL void js_enum(js_context *ctx, js_idx_t obj_idx, js_uint_t enum_flags);
JS_EXTERNAL_DECL js_bool_t js_next(js_context *ctx, js_idx_t enum_idx, js_bool_t get_value);
JS_EXTERNAL_DECL void js_seal(js_context *ctx, js_idx_t obj_idx);
JS_EXTERNAL_DECL void js_freeze(js_context *ctx, js_idx_t obj_idx);

/*
 *  String manipulation
 */

JS_EXTERNAL_DECL void js_concat(js_context *ctx, js_idx_t count);
JS_EXTERNAL_DECL void js_join(js_context *ctx, js_idx_t count);
JS_EXTERNAL_DECL void js_decode_string(js_context *ctx, js_idx_t idx, js_decode_char_function callback, void *udata);
JS_EXTERNAL_DECL void js_map_string(js_context *ctx, js_idx_t idx, js_map_char_function callback, void *udata);
JS_EXTERNAL_DECL void js_substring(js_context *ctx, js_idx_t idx, js_size_t start_char_offset, js_size_t end_char_offset);
JS_EXTERNAL_DECL void js_trim(js_context *ctx, js_idx_t idx);
JS_EXTERNAL_DECL js_codepoint_t js_char_code_at(js_context *ctx, js_idx_t idx, js_size_t char_offset);

/*
 *  ECMAScript operators
 */

JS_EXTERNAL_DECL js_bool_t js_equals(js_context *ctx, js_idx_t idx1, js_idx_t idx2);
JS_EXTERNAL_DECL js_bool_t js_strict_equals(js_context *ctx, js_idx_t idx1, js_idx_t idx2);
JS_EXTERNAL_DECL js_bool_t js_samevalue(js_context *ctx, js_idx_t idx1, js_idx_t idx2);
JS_EXTERNAL_DECL js_bool_t js_instanceof(js_context *ctx, js_idx_t idx1, js_idx_t idx2);

/*
 *  Random
 */

JS_EXTERNAL_DECL js_double_t js_random(js_context *ctx);

/*
 *  Function (method) calls
 */

JS_EXTERNAL_DECL void js_call(js_context *ctx, js_idx_t nargs);
JS_EXTERNAL_DECL void js_call_method(js_context *ctx, js_idx_t nargs);
JS_EXTERNAL_DECL void js_call_prop(js_context *ctx, js_idx_t obj_idx, js_idx_t nargs);
JS_EXTERNAL_DECL js_int_t js_pcall(js_context *ctx, js_idx_t nargs);
JS_EXTERNAL_DECL js_int_t js_pcall_method(js_context *ctx, js_idx_t nargs);
JS_EXTERNAL_DECL js_int_t js_pcall_prop(js_context *ctx, js_idx_t obj_idx, js_idx_t nargs);
JS_EXTERNAL_DECL void js_new(js_context *ctx, js_idx_t nargs);
JS_EXTERNAL_DECL js_int_t js_pnew(js_context *ctx, js_idx_t nargs);
JS_EXTERNAL_DECL js_int_t js_safe_call(js_context *ctx, js_safe_call_function func, void *udata, js_idx_t nargs, js_idx_t nrets);

/*
 *  Thread management
 */

/* There are currently no native functions to yield/resume, due to the internal
 * limitations on coroutine handling.  These will be added later.
 */

/*
 *  Compilation and evaluation
 */

JS_EXTERNAL_DECL js_int_t js_eval_raw(js_context *ctx, const char *src_buffer, js_size_t src_length, js_uint_t flags);
JS_EXTERNAL_DECL js_int_t js_compile_raw(js_context *ctx, const char *src_buffer, js_size_t src_length, js_uint_t flags);

/* plain */
#define js_eval(ctx)  \
	((void) js_eval_raw((ctx), NULL, 0, 1 /*args*/ | JS_COMPILE_EVAL | JS_COMPILE_NOFILENAME))

#define js_eval_noresult(ctx)  \
	((void) js_eval_raw((ctx), NULL, 0, 1 /*args*/ | JS_COMPILE_EVAL | JS_COMPILE_NORESULT | JS_COMPILE_NOFILENAME))

#define js_peval(ctx)  \
	(js_eval_raw((ctx), NULL, 0, 1 /*args*/ | JS_COMPILE_EVAL | JS_COMPILE_SAFE | JS_COMPILE_NOFILENAME))

#define js_peval_noresult(ctx)  \
	(js_eval_raw((ctx), NULL, 0, 1 /*args*/ | JS_COMPILE_EVAL | JS_COMPILE_SAFE | JS_COMPILE_NORESULT | JS_COMPILE_NOFILENAME))

#define js_compile(ctx,flags)  \
	((void) js_compile_raw((ctx), NULL, 0, 2 /*args*/ | (flags)))

#define js_pcompile(ctx,flags)  \
	(js_compile_raw((ctx), NULL, 0, 2 /*args*/ | (flags) | JS_COMPILE_SAFE))

/* string */
#define js_eval_string(ctx,src)  \
	((void) js_eval_raw((ctx), (src), 0, 0 /*args*/ | JS_COMPILE_EVAL | JS_COMPILE_NOSOURCE | JS_COMPILE_STRLEN | JS_COMPILE_NOFILENAME))

#define js_eval_string_noresult(ctx,src)  \
	((void) js_eval_raw((ctx), (src), 0, 0 /*args*/ | JS_COMPILE_EVAL | JS_COMPILE_NOSOURCE | JS_COMPILE_STRLEN | JS_COMPILE_NORESULT | JS_COMPILE_NOFILENAME))

#define js_peval_string(ctx,src)  \
	(js_eval_raw((ctx), (src), 0, 0 /*args*/ | JS_COMPILE_EVAL | JS_COMPILE_SAFE | JS_COMPILE_NOSOURCE | JS_COMPILE_STRLEN | JS_COMPILE_NOFILENAME))

#define js_peval_string_noresult(ctx,src)  \
	(js_eval_raw((ctx), (src), 0, 0 /*args*/ | JS_COMPILE_EVAL | JS_COMPILE_SAFE | JS_COMPILE_NOSOURCE | JS_COMPILE_STRLEN | JS_COMPILE_NORESULT | JS_COMPILE_NOFILENAME))

#define js_compile_string(ctx,flags,src)  \
	((void) js_compile_raw((ctx), (src), 0, 0 /*args*/ | (flags) | JS_COMPILE_NOSOURCE | JS_COMPILE_STRLEN | JS_COMPILE_NOFILENAME))

#define js_compile_string_filename(ctx,flags,src)  \
	((void) js_compile_raw((ctx), (src), 0, 1 /*args*/ | (flags) | JS_COMPILE_NOSOURCE | JS_COMPILE_STRLEN))

#define js_pcompile_string(ctx,flags,src)  \
	(js_compile_raw((ctx), (src), 0, 0 /*args*/ | (flags) | JS_COMPILE_SAFE | JS_COMPILE_NOSOURCE | JS_COMPILE_STRLEN | JS_COMPILE_NOFILENAME))

#define js_pcompile_string_filename(ctx,flags,src)  \
	(js_compile_raw((ctx), (src), 0, 1 /*args*/ | (flags) | JS_COMPILE_SAFE | JS_COMPILE_NOSOURCE | JS_COMPILE_STRLEN))

/* lstring */
#define js_eval_lstring(ctx,buf,len)  \
	((void) js_eval_raw((ctx), buf, len, 0 /*args*/ | JS_COMPILE_EVAL | JS_COMPILE_NOSOURCE | JS_COMPILE_NOFILENAME))

#define js_eval_lstring_noresult(ctx,buf,len)  \
	((void) js_eval_raw((ctx), buf, len, 0 /*args*/ | JS_COMPILE_EVAL | JS_COMPILE_NOSOURCE | JS_COMPILE_NORESULT | JS_COMPILE_NOFILENAME))

#define js_peval_lstring(ctx,buf,len)  \
	(js_eval_raw((ctx), buf, len, 0 /*args*/ | JS_COMPILE_EVAL | JS_COMPILE_NOSOURCE | JS_COMPILE_SAFE | JS_COMPILE_NOFILENAME))

#define js_peval_lstring_noresult(ctx,buf,len)  \
	(js_eval_raw((ctx), buf, len, 0 /*args*/ | JS_COMPILE_EVAL | JS_COMPILE_SAFE | JS_COMPILE_NOSOURCE | JS_COMPILE_NORESULT | JS_COMPILE_NOFILENAME))

#define js_compile_lstring(ctx,flags,buf,len)  \
	((void) js_compile_raw((ctx), buf, len, 0 /*args*/ | (flags) | JS_COMPILE_NOSOURCE | JS_COMPILE_NOFILENAME))

#define js_compile_lstring_filename(ctx,flags,buf,len)  \
	((void) js_compile_raw((ctx), buf, len, 1 /*args*/ | (flags) | JS_COMPILE_NOSOURCE))

#define js_pcompile_lstring(ctx,flags,buf,len)  \
	(js_compile_raw((ctx), buf, len, 0 /*args*/ | (flags) | JS_COMPILE_SAFE | JS_COMPILE_NOSOURCE | JS_COMPILE_NOFILENAME))

#define js_pcompile_lstring_filename(ctx,flags,buf,len)  \
	(js_compile_raw((ctx), buf, len, 1 /*args*/ | (flags) | JS_COMPILE_SAFE | JS_COMPILE_NOSOURCE))

/*
 *  Bytecode load/dump
 */

JS_EXTERNAL_DECL void js_dump_function(js_context *ctx);
JS_EXTERNAL_DECL void js_load_function(js_context *ctx);

/*
 *  Debugging
 */

JS_EXTERNAL_DECL void js_push_context_dump(js_context *ctx);

/*
 *  Debugger (debug protocol)
 */

JS_EXTERNAL_DECL void js_debugger_attach(js_context *ctx,
                                           js_debug_read_function read_cb,
                                           js_debug_write_function write_cb,
                                           js_debug_peek_function peek_cb,
                                           js_debug_read_flush_function read_flush_cb,
                                           js_debug_write_flush_function write_flush_cb,
                                           js_debug_request_function request_cb,
                                           js_debug_detached_function detached_cb,
                                           void *udata);
JS_EXTERNAL_DECL void js_debugger_detach(js_context *ctx);
JS_EXTERNAL_DECL void js_debugger_cooperate(js_context *ctx);
JS_EXTERNAL_DECL js_bool_t js_debugger_notify(js_context *ctx, js_idx_t nvalues);
JS_EXTERNAL_DECL void js_debugger_pause(js_context *ctx);

/*
 *  Time handling
 */

JS_EXTERNAL_DECL js_double_t js_get_now(js_context *ctx);
JS_EXTERNAL_DECL void js_time_to_components(js_context *ctx, js_double_t timeval, js_time_components *comp);
JS_EXTERNAL_DECL js_double_t js_components_to_time(js_context *ctx, js_time_components *comp);

/*
 *  Date provider related constants
 *
 *  NOTE: These are "semi public" - you should only use these if you write
 *  your own platform specific Date provider, see doc/datetime.rst.
 */

/* Millisecond count constants. */
#define JS_DATE_MSEC_SECOND          1000L
#define JS_DATE_MSEC_MINUTE          (60L * 1000L)
#define JS_DATE_MSEC_HOUR            (60L * 60L * 1000L)
#define JS_DATE_MSEC_DAY             (24L * 60L * 60L * 1000L)

/* ECMAScript date range is 100 million days from Epoch:
 * > 100e6 * 24 * 60 * 60 * 1000  // 100M days in millisecs
 * 8640000000000000
 * (= 8.64e15)
 */
#define JS_DATE_MSEC_100M_DAYS         (8.64e15)
#define JS_DATE_MSEC_100M_DAYS_LEEWAY  (8.64e15 + 24 * 3600e3)

/* ECMAScript year range:
 * > new Date(100e6 * 24 * 3600e3).toISOString()
 * '+275760-09-13T00:00:00.000Z'
 * > new Date(-100e6 * 24 * 3600e3).toISOString()
 * '-271821-04-20T00:00:00.000Z'
 */
#define JS_DATE_MIN_ECMA_YEAR     (-271821L)
#define JS_DATE_MAX_ECMA_YEAR     275760L

/* Part indices for internal breakdowns.  Part order from JS_DATE_IDX_YEAR
 * to JS_DATE_IDX_MILLISECOND matches argument ordering of ECMAScript API
 * calls (like Date constructor call).  Some functions in js_bi_date.c
 * depend on the specific ordering, so change with care.  16 bits are not
 * enough for all parts (year, specifically).
 *
 * Must be in-sync with genbuiltins.py.
 */
#define JS_DATE_IDX_YEAR           0  /* year */
#define JS_DATE_IDX_MONTH          1  /* month: 0 to 11 */
#define JS_DATE_IDX_DAY            2  /* day within month: 0 to 30 */
#define JS_DATE_IDX_HOUR           3
#define JS_DATE_IDX_MINUTE         4
#define JS_DATE_IDX_SECOND         5
#define JS_DATE_IDX_MILLISECOND    6
#define JS_DATE_IDX_WEEKDAY        7  /* weekday: 0 to 6, 0=sunday, 1=monday, etc */
#define JS_DATE_IDX_NUM_PARTS      8

/* Internal API call flags, used for various functions in js_bi_date.c.
 * Certain flags are used by only certain functions, but since the flags
 * don't overlap, a single flags value can be passed around to multiple
 * functions.
 *
 * The unused top bits of the flags field are also used to pass values
 * to helpers (js__get_part_helper() and js__set_part_helper()).
 *
 * Must be in-sync with genbuiltins.py.
 */

/* NOTE: when writing a Date provider you only need a few specific
 * flags from here, the rest are internal.  Avoid using anything you
 * don't need.
 */

#define JS_DATE_FLAG_NAN_TO_ZERO          (1 << 0)  /* timeval breakdown: internal time value NaN -> zero */
#define JS_DATE_FLAG_NAN_TO_RANGE_ERROR   (1 << 1)  /* timeval breakdown: internal time value NaN -> RangeError (toISOString) */
#define JS_DATE_FLAG_ONEBASED             (1 << 2)  /* timeval breakdown: convert month and day-of-month parts to one-based (default is zero-based) */
#define JS_DATE_FLAG_EQUIVYEAR            (1 << 3)  /* timeval breakdown: replace year with equivalent year in the [1971,2037] range for DST calculations */
#define JS_DATE_FLAG_LOCALTIME            (1 << 4)  /* convert time value to local time */
#define JS_DATE_FLAG_SUB1900              (1 << 5)  /* getter: subtract 1900 from year when getting year part */
#define JS_DATE_FLAG_TOSTRING_DATE        (1 << 6)  /* include date part in string conversion result */
#define JS_DATE_FLAG_TOSTRING_TIME        (1 << 7)  /* include time part in string conversion result */
#define JS_DATE_FLAG_TOSTRING_LOCALE      (1 << 8)  /* use locale specific formatting if available */
#define JS_DATE_FLAG_TIMESETTER           (1 << 9)  /* setter: call is a time setter (affects hour, min, sec, ms); otherwise date setter (affects year, month, day-in-month) */
#define JS_DATE_FLAG_YEAR_FIXUP           (1 << 10) /* setter: perform 2-digit year fixup (00...99 -> 1900...1999) */
#define JS_DATE_FLAG_SEP_T                (1 << 11) /* string conversion: use 'T' instead of ' ' as a separator */
#define JS_DATE_FLAG_VALUE_SHIFT          12        /* additional values begin at bit 12 */

/*
 *  ROM pointer compression
 */

/* Support array for ROM pointer compression.  Only declared when ROM
 * pointer compression is active.
 */
#if defined(JS_USE_ROM_OBJECTS) && defined(JS_USE_HEAPPTR16)
JS_EXTERNAL_DECL const void * const js_rom_compressed_pointers[];
#endif

/*
 *  C++ name mangling
 */

#if defined(__cplusplus)
/* end 'extern "C"' wrapper */
}
#endif

/*
 *  END PUBLIC API
 */

#endif  /* DUKTAPE_H_INCLUDED */
