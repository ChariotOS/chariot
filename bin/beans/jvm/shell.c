#include <hawkbeans.h>
#include <shell.h>
#include <thread.h>
#include <stack.h>
#include <hashtable.h>
#include <mnemonics.h>
#include <bc_interp.h>
#include <exceptions.h>
#include <arch/x64-linux/bootstrap_loader.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdnoreturn.h>
#include <signal.h>

// #include <readline/history.h>
// #include <readline/readline.h>

extern jthread_t *cur_thread;

#define MAX_BPS 1024


struct bp_method_desc {
  char *class_name;
  char *method_name;
  int nargs;
  char **args;
  char *return_type;
  char *method_desc;
};

struct bp_line_desc {
  char *class_name;
  int line_num;
};


typedef enum {
  BP_TYPE_METHOD,
  BP_TYPE_LINENUM,
} bp_type_t;


struct bp {
  bool valid;
  bp_type_t type;
  union {
    struct bp_method_desc method;
    struct bp_line_desc line;
  };
};


struct bp_info {
  unsigned num_bps;
  struct bp bps[MAX_BPS];
};

static struct bp_info bpi;


// Returns the next whitespace-delimited token in the string pointed to by
// `line_ptr`. A token which starts with the first character in `*line_ptr`
// still counts. This destructively modifies `*line_ptr` by writing a NULL
// character into the token boundry. `*line_ptr` is modified to point to the
// character immediately after that which ended the token, or the end of the
// string if it's reached. If `*line_ptr` contains only whitespace or is empty,
// an empty string is returned. The returned string aliases contents from
// `*line_ptr`, and therefore has the same lifetime.
static inline char *next_token(char **line_ptr) {
  while (isspace(**line_ptr)) {
    (*line_ptr)++;
  }

  char *token_start = *line_ptr;

  bool done = false;
  while (!isspace(**line_ptr) && !(done = !**line_ptr)) {
    (*line_ptr)++;
  }

  if (!done) {
    **line_ptr = 0;
    (*line_ptr)++;
  }

  return token_start;
}

// Tries to parse the next whitespace-delimited token in `*line_ptr` as an
// unsigned hexadecimal integer. If successful, the parsed value is stored in
// `result`, and `NULL` is returned. Otherwise, the contents of `result` are
// undefined, and the token which failed to parse is returned.
static inline char *next_hex(char **line_ptr, size_t *result) {
  char *original_token = next_token(line_ptr);

  if (!*original_token) {
    return original_token;
  }

  char *remaining_token = original_token;
  *result = 0;
  goto skip_shift;

  do {
    *result *= 16;
  skip_shift:
    if (*remaining_token > 47 && *remaining_token < 58) {
      *result += (size_t)(*remaining_token - '0');
    } else if (*remaining_token > 64 && *remaining_token < 71) {
      *result += (size_t)(*remaining_token - 'A' + 10);
    } else if (*remaining_token > 96 && *remaining_token < 103) {
      *result += (size_t)(*remaining_token - 'a' + 10);
    } else {
      return original_token;
    }
    remaining_token++;
  } while (*remaining_token);

  return NULL;
}


// Tries to parse the next whitespace-delimited token in `*line_ptr` as an
// unsigned hexadecimal integer. If successful, the parsed value is stored in
// `result`, and `NULL` is returned. Otherwise, the contents of `result` are
// undefined, and the token which failed to parse is returned.
static inline char *next_dec(char **line_ptr, size_t *result) {
  char *original_token = next_token(line_ptr);

  if (!*original_token) {
    return original_token;
  }

  char *remaining_token = original_token;
  *result = 0;
  goto skip_shift;

  do {
    *result *= 10;
  skip_shift:
    if (*remaining_token > 47 && *remaining_token < 58) {
      *result += (size_t)(*remaining_token - '0');
    } else {
      return original_token;
    }
    remaining_token++;
  } while (*remaining_token);

  return NULL;
}


// Similar to `next_hex`, except it `ERROR_PRINT`s on failure and returns a
// non-zero error code.
static inline int try_next_hex(char **line_ptr, size_t *result) {
  char *token = next_hex(line_ptr, result);
  if (token) {
    HB_ERR("  '%s' is not a valid positive hexadecimal integer", token);
    return -1;
  }
  return 0;
}


// Similar to `next_dec`, except it `ERROR_PRINT`s on failure and returns a
// non-zero error code.
static inline int try_next_dec(char **line_ptr, size_t *result) {
  char *token = next_dec(line_ptr, result);
  if (token) {
    HB_ERR("  '%s' is not a valid positive decimal integer", token);
    return -1;
  }
  return 0;
}


// Checked during stepping to abort early on SIGINT
static bool sigint_recvd;


// Prints a helpful message (for when the PC changes) that indicates the new PC
// and the corresponding instruction
// KCH: should be "Step completed: "thread=??", ClassName.funcname(), line=?? PC=??
// + current instruction dump
static void print_pc_update(jthread_t *t) {
  char buf[32];
  memset(buf, 0, 32);
  java_class_t *cls = t->cur_frame->cls;

  const char *class_nm = hb_get_class_name(cls);
  const char *method_nm = hb_get_const_str(t->cur_frame->minfo->name_idx, cls);

  hb_instr_repr(t, buf, sizeof(buf));

  HB_INFO_NOBRK("  " BRCYAN("%s") "::" YELLOW("%s()") " -- [" BRMAGENTA("PC=0x%02x") "]\n"
            "    at " GREEN("%s") ":%d\n",
            class_nm,
            method_nm,
            t->cur_frame->pc,
            (cls->src_file) ? cls->src_file : "??",
            hb_get_linenum_from_pc(t->cur_frame->pc, t->cur_frame));

  HB_INFO_NOBRK("    %-8s: " BLUE("%s"), "SOURCE", hb_get_srcline_from_pc(t->cur_frame->pc, t->cur_frame));

  HB_INFO("    %-8s: " BRBLUE("%s"), "BYTECODE", buf);
}


static void print_usage(void);

static int cmd_help(jthread_t *t, char *args) {
  print_usage();
  return 0;
}

// TODO
static int cmd_print(jthread_t *t, char *args) {
  // search in fields,
  // locals
  HB_ERR("%s not implemented", __func__);
  return 0;
}

static bool args_match(struct bp_method_desc *a, struct bp_method_desc *b) {
  if (a->nargs != b->nargs) return false;

  for (int i = 0; i < a->nargs; i++) {
    if (strcmp(a->args[i], b->args[i]) != 0) return false;
  }

  return true;
}


static bool method_matches_desc(method_info_t *mi, java_class_t *cls, struct bp_method_desc *desc) {
  if (desc->nargs != mi->nargs) return false;

  for (int i = 0; i < mi->nargs; i++) {
    if (strcmp(mi->args[i], desc->args[i]) != 0) return false;
  }

  return true;
}


// -1 is valid
//  >=0 bp id
static int is_valid_bp(jthread_t *t) {
  for (int i = 0; i < MAX_BPS; i++) {
    if (bpi.bps[i].valid == false) continue;

    // TODO: no linenum support yet
    if (bpi.bps[i].type == BP_TYPE_LINENUM) continue;

    // do we match by method?
    if (bpi.bps[i].type == BP_TYPE_METHOD) {
      if (strcmp(hb_get_class_name(t->cur_frame->cls), bpi.bps[i].method.class_name) != 0) return -1;
      if (strcmp(t->cur_frame->minfo->name, bpi.bps[i].method.method_name) != 0) return -1;
      if (method_matches_desc(t->cur_frame->minfo, t->cur_frame->cls, &(bpi.bps[i].method))) return i;
    }
  }

  return -1;
}


static bool bp_exists(struct bp_method_desc *desc) {
  for (int i = 0; i < MAX_BPS; i++) {
    if (!bpi.bps[i].valid) continue;

    if (strcmp(desc->class_name, bpi.bps[i].method.class_name) != 0) continue;

    if (strcmp(desc->method_name, bpi.bps[i].method.method_name) != 0) continue;

    if (!args_match(desc, &(bpi.bps[i].method))) continue;

    return true;
  }

  return false;
}


static void check_res_bump_pc(int res, jthread_t *t) {
  if (res == -1) {
    HB_ERR("Instruction execution failed");
    exit(EXIT_FAILURE);
  } else if (res == -ESHOULD_BRANCH) {
    // branch handlers set PC explicitly, so
    // we shouldn't update it here
    return;
  } else if (res == -ESHOULD_RETURN) {
    // stack frame goes away on method returns, no need to bump
    if (!t->cur_frame) exit(EXIT_SUCCESS);
    return;
  } else {
    t->cur_frame->pc += res;
  }
}

static int bp_rm(int id) {
  if (id >= MAX_BPS || bpi.bps[id].valid == false) return -1;

  // TODO:  free fields allocated by method parser
  bpi.bps[id].valid = false;
  bpi.num_bps--;
  return 0;
}


static void clear_bps(void) {
  for (int i = 0; i < MAX_BPS; i++)
    bpi.bps[i].valid = false;

  bpi.num_bps = 0;
}


static inline void check_bp(int bp_id) {
  if (bp_id != -1) {
    HB_INFO("Breakpoint " BRCYAN("[#%d]") " reached", bp_id);
    bp_rm(bp_id);
  }
}


static int cmd_step(jthread_t *t, char *args) {
  int bp_hit;
  int res = 0;
  stack_frame_t *frame = t->cur_frame;

  u2 line_num = hb_get_linenum_from_pc(frame->pc, frame);
  u2 new_line = line_num;

  while (((bp_hit = is_valid_bp(t)) == -1) && !sigint_recvd && line_num == new_line) {
    res = hb_exec_one(t);
    check_res_bump_pc(res, t);
    new_line = hb_get_linenum_from_pc(t->cur_frame->pc, t->cur_frame);
  }

  check_bp(bp_hit);

  print_pc_update(t);

  return 0;
}



/*
 * This one is tricky because we can land in any frame, so we want to be able
 * to continue from a frame *up* from where we started, but we also want to be
 * able to detect when we're returning from main
 */
static int cmd_cont(jthread_t *t, char *args) {
  int bp_hit;
  int res = 0;

  while (((bp_hit = is_valid_bp(t)) == -1) && !sigint_recvd) {
    res = hb_exec_one(t);
    check_res_bump_pc(res, t);
  }

  check_bp(bp_hit);
  print_pc_update(t);

  return 0;
}


static int cmd_stepi(jthread_t *t, char *args) {
  int bp_hit;
  int res = 0;

  if (((bp_hit = is_valid_bp(t)) == -1) && !sigint_recvd) res = hb_exec_one(t);

  if (bp_hit != -1)
    check_bp(bp_hit);
  else
    check_res_bump_pc(res, t);

  print_pc_update(t);

  return 0;
}

#define BEFORE 8

static int cmd_list(jthread_t *t, char *args) {
  stack_frame_t *frame = t->cur_frame;
  int line = hb_get_linenum_from_pc(frame->pc, frame);
  u2 start = ((line - BEFORE) > 0) ? line - BEFORE : 0;

  hb_read_source_file(frame->cls);
  for (; start < line + BEFORE && start < frame->cls->num_src_lines; start++) {
    HB_INFO_NOBRK("%03d: %3s %s", start, (start == line) ? BRYELLOW("==>") : "  ", hb_get_srcline(start, frame));
  }

  return 0;
}


static int cmd_classes(jthread_t *t, char *args) {
  struct nk_hashtable_iter *iter = nk_create_htable_iter(hb_get_classmap());

  if (!iter) {
    HB_ERR("Could not create class map iterator");
    return -1;
  }

  HB_INFO("** Loaded classes **");
  do {
    char *name = (char *)nk_htable_get_iter_key(iter);
    java_class_t *cls = (java_class_t *)nk_htable_get_iter_value(iter);
    HB_INFO("%s", name);
  } while (nk_htable_iter_advance(iter) != 0);

  nk_destroy_htable_iter(iter);

  return 0;
}

// TODO: probably going to use helpers from other guys
static int cmd_class(jthread_t *t, char *args) {
  HB_ERR("%s not implemented", __func__);
  return 0;
}

#define GET_CLASS                                   \
  if (!args || *args == '\0') {                     \
    cls = t->class;                                 \
  } else {                                          \
    cls = hb_get_class(args);                       \
    if (!cls) {                                     \
      HB_ERR("Class %s has not been loaded", args); \
      return 0;                                     \
    }                                               \
  }


static int cmd_locals(jthread_t *t, char *args) {
  hb_dump_locals();
  return 0;
}


static int cmd_methods(jthread_t *t, char *args) {
  java_class_t *cls = NULL;
  GET_CLASS;

  if (hb_get_method_count(cls) == 0) {
    HB_INFO("** Class %s has no methods **", hb_get_class_name(cls));
    return 0;
  }

  HB_INFO("** Method list (%s) **", hb_get_class_name(t->class));

  while (cls) {
    for (int i = 0; i < hb_get_method_count(cls); i++) {
      method_info_t *mi = &cls->methods[i];
      HB_INFO(CYAN("%-20s") " " GREEN("%s %s"), hb_get_class_name(cls), cls->methods[i].name, cls->methods[i].desc);
    }
    cls = hb_get_super_class(cls);
  }

  return 0;
}

static int cmd_fields(jthread_t *t, char *args) {
  java_class_t *cls = NULL;

  GET_CLASS;

  if (hb_get_field_count(cls) == 0) {
    HB_INFO("** Class %s has no fields **", hb_get_class_name(cls));
    return 0;
  }

  HB_INFO("** Field list (%s) **", hb_get_class_name(t->class));

  for (int i = 0; i < hb_get_field_count(cls); i++) {
    field_info_t *fi = &cls->fields[i];
    HB_INFO("  %s %s", cls->fields[i].desc_str, cls->fields[i].name_str);
  }

  return 0;
}


static int get_method_matches(method_info_t **matches, java_class_t *cls, char *method_name) {
  int nmatches = 0;
  char buf[128];
  strcpy(buf, method_name);
  char *mname = strtok(buf, "(");

  for (int i = 0; i < hb_get_method_count(cls); i++) {
    method_info_t *mi = &cls->methods[i];
    if (strcmp(mname, hb_get_method_name(mi)) == 0) matches[nmatches++] = mi;
  }

  return nmatches;
}




static int insert_bp(struct bp_method_desc *desc) {
  int id = -1;

  method_info_t *target_method = NULL;

  java_class_t *cls = hb_get_class(desc->class_name);
  if (!cls) return -1;

  if (hb_get_method_count(cls) == 0) return -1;

  if (!desc->method_name) return -1;

  if (bp_exists(desc)) {
    HB_INFO("Breakpoint already exists");
    return -1;
  }

  method_info_t *matches[128];
  memset(matches, 0, sizeof(method_info_t *));
  int n = get_method_matches((method_info_t **)&matches, cls, desc->method_name);

  // we didn't find it
  if (n == 0) {
    HB_INFO("No matching method found");
    return -1;
  }

  if (n == 1) {
    target_method = matches[0];
  } else {
    if (!desc->args) {
      HB_INFO("Ambiguous method provided");
      return -1;
    }

    for (int i = 0; i < n; i++) {
      if (method_matches_desc(matches[i], cls, desc)) {
        target_method = matches[i];
        break;
      }
    }
  }

  if (!target_method) {
    HB_INFO("No matching method found");
    return -1;
  }

  for (int i = 0; i < MAX_BPS; i++) {
    if (bpi.bps[i].valid == false) {
      bpi.bps[i].method.method_name = desc->method_name;
      bpi.bps[i].method.method_desc = desc->method_desc;
      bpi.bps[i].method.nargs = desc->nargs;
      bpi.bps[i].method.class_name = desc->class_name;
      bpi.bps[i].method.args = desc->args;
      bpi.bps[i].method.return_type = desc->return_type;
      bpi.bps[i].valid = true;
      id = i;
      bpi.num_bps++;
      break;
    }
  }

  return id;
}




static void bp_list(void) {
  for (int i = 0; i < MAX_BPS; i++) {
    if (bpi.bps[i].valid == true) {
      HB_INFO("[" BRCYAN("%3d") "]  " GREEN("%s.") YELLOW("%s %s"), i, bpi.bps[i].method.class_name, bpi.bps[i].method.method_name,
          bpi.bps[i].method.method_desc);
    }
  }
}


static struct bp_method_desc *parse_m(char *where) {
  struct bp_method_desc *desc = malloc(sizeof(*desc));
  int nargs = 0;

  memset(desc, 0, sizeof(*desc));

  char *arg_cursor = strchr(where, '(');
  char *end_args = strchr(where, ')');
  char *class = strtok(where, ".");
  char *name = strdup(strtok(NULL, "("));
  if (arg_cursor) *(arg_cursor++) = '(';
  char **new_args = NULL;
  char *args[32];
  char *ret_type = NULL;

  memset(args, 0, sizeof(char *) * 32);

  // we have arguments
  if (arg_cursor) {
    if (!end_args) {
      HB_INFO("Invalid breakpoint syntax");
      return NULL;
    }

    char *arg = strtok(arg_cursor, ",)");
    do {
      if (arg) args[nargs++] = strdup(arg);
    } while ((arg = strtok(NULL, ",)")));
  }

  if (nargs) {
    new_args = malloc(sizeof(char *) * nargs);
    for (int i = 0; i < nargs; i++)
      new_args[i] = args[i];
  }

  desc->nargs = nargs;
  desc->args = new_args;
  desc->class_name = strdup(class);
  desc->method_name = strdup(name);
  desc->return_type = NULL;  // KCH TODO: we ignore return types for now

  return desc;
}

// Currently only the "in" syntax is supported
// TODO: support "at" syntax (line number)
static int cmd_stop(jthread_t *t, char *args) {
  char *which = next_token(&args);

  if (strcmp(which, "in") == 0) {
    char *where = next_token(&args);
    struct bp_method_desc *desc = parse_m(where);

    int bpid = insert_bp(desc);

    if (bpid >= 0) {
      HB_INFO("Breakpoint [" BRCYAN("#%d") "] created for " YELLOW("%s.%s"), bpid, desc->class_name, desc->method_name);
      return 0;
    } else {
      HB_INFO("Could not create breakpoint");
    }
  } else {
    HB_INFO("Unsupported breakpoint syntax");
  }

  return 0;
}


static int cmd_throw(jthread_t *t, char *args) {
  char *excp = next_token(&args);
  int ret = hb_excp_str_to_type(excp);

  if (ret < 0) {
    HB_INFO("Unknown exception (%s)", excp);
    return 0;
  } else {
    hb_throw_and_create_excp(ret);
    return 0;
  }

  return 0;
}


static int cmd_bc(jthread_t *t, char *args) {
  print_pc_update(t);
  return 0;
}

static int cmd_where(jthread_t *t, char *args) {
  stack_frame_t *frame = t->cur_frame;
  int frame_num = 1;

  while (frame) {
    java_class_t *cls = frame->cls;
    const char *class_nm = hb_get_class_name(cls);
    const char *method_nm = hb_get_method_name(frame->minfo);

    HB_INFO_NOBRK("#%d  " BRCYAN("%s") "." YELLOW("%s()") " -- [" BRMAGENTA("PC: 0x%02x") "]\n"
                "    at " GREEN("%s") ":%d\n",
                frame_num,
                class_nm,
                method_nm,
                frame->pc,
                (cls->src_file) ? cls->src_file : "??",
                hb_get_linenum_from_pc(frame->pc, frame));

    frame = frame->prev;
    frame_num++;
  }

  return 0;
}

static int cmd_clear(jthread_t *t, char *args) {
  clear_bps();
  return 0;
}


static int cmd_bp_list(jthread_t *t, char *args) {
  bp_list();
  return 0;
}

static int cmd_bp_rm(jthread_t *t, char *args) {
  char *tok = next_token(&args);
  int id = atoi(tok);
  if (bp_rm(id) < 0) HB_INFO("No such breakpoint");

  return 0;
}


static int cmd_quit(jthread_t *t, char *args) {
  printf("  Quitting. Goodbye.\n");
  exit(EXIT_SUCCESS);
	return 0;
}

#define SPELLINGS(...) ((const char *[]){__VA_ARGS__, NULL})

typedef struct command_descriptor {
  const char *const *spellings;
  const char *usage;
  const char *description;
  int (*handler)(jthread_t *, char *);
} command_descriptor_t;

static const command_descriptor_t commands[] = {
    {SPELLINGS("help", "?", "h"), "", "Prints a list of the available commands", cmd_help},

    {SPELLINGS("step", "s"), "", "Execute current line", cmd_step},

    {SPELLINGS("stepi", "i"), "", "Execute current instruction", cmd_stepi},

    {SPELLINGS("cont", "c"), "", "Continues program execution", cmd_cont},

    {SPELLINGS("list", "ls"), "", "Show source listing at current PC", cmd_list},

    {SPELLINGS("where", "w"), "", "Dumps the stack of the current thread, with PC info", cmd_where},

    {SPELLINGS("print", "pr"), "<variable>", "Prints a primitive value, an object's instance fields, or if it's a class, its static fields",
        cmd_print},

    {SPELLINGS("quit", "exit", "q"), "", "Quits this program", cmd_quit},

    {SPELLINGS("throw", "t"), "<Class ID>", "Raises an Exception of named class", cmd_throw},

    {SPELLINGS("instr", "bc"), "", "Disassembles the current bytecode instruction (if applicable)", cmd_bc},

    {SPELLINGS("locals", "l"), "", "Print all local variables in current stack frame", cmd_locals},

    {SPELLINGS("class", "cl"), "<class ID>", "Show details of named class", cmd_class},

    {SPELLINGS("classes", "cls"), "", "List currently known classes", cmd_classes},

    {SPELLINGS("methods", "m"), "<class ID>", "List a class's methods", cmd_methods},

    {SPELLINGS("fields", "f"), "<class ID>", "List a class's fields", cmd_fields},

    {SPELLINGS("stop"),
        //"<in|at> <Class ID>.<method>[(argument_type,...)] | <class ID>:<line> >",
        "in <Class ID>.<method>[(argument_type,...)]", "Set a breakpoint in a method", cmd_stop},

    {SPELLINGS("breaks", "bp"), "", "Show all active breakpoints", cmd_bp_list},

    {SPELLINGS("remove", "rm"), "<breakpoint ID>", "Removes the given breakpoint", cmd_bp_rm},

    {SPELLINGS("clear"), "", "Clear all breakpoints", cmd_clear},
};

static void print_usage(void) {
  for (size_t i = 0; i < sizeof(commands) / sizeof(*commands); i++) {
    const char *const *spelling_ptr = commands[i].spellings;
    HB_SUGGEST_NOBRK("  ");
    goto skip_or;

    for (; *spelling_ptr; spelling_ptr++) {
      HB_SUGGEST_NOBRK(UNBOLD(" or "));
    skip_or:
      HB_SUGGEST_NOBRK("%s", *spelling_ptr);
    }

    HB_SUGGEST("  %s" UNBOLD(" -- %s"), commands[i].usage, commands[i].description);
  }
}

static int handle_cmd(jthread_t *t, char *line) {
  char *cmd = next_token(&line);
  for (size_t i = 0; i < sizeof(commands) / sizeof(command_descriptor_t); i++) {
    for (const char *const *spelling_ptr = commands[i].spellings; *spelling_ptr; spelling_ptr++) {
      if (!strcmp(cmd, *spelling_ptr)) {
        sigint_recvd = false;

        int retcode = commands[i].handler(t, line);
        if (retcode) {
          HB_ERR("  Invalid syntax");
          HB_SUGGEST("  Usage: %s %s", *spelling_ptr, commands[i].usage);
        }

        return retcode;
      }
    }
  }
  HB_ERR("  Unknown command");
  return -1;
}


static void handle_sigint(int signum) { sigint_recvd = true; }




static struct sigaction sigint_action = {.sa_handler = handle_sigint};
void run_shell(jthread_t *t, bool interactive) {
  printf("TODO: the shell doesn't work lol %d\n", interactive);
  while (1) {
    cmd_cont(t, "");
  }
#if 0
    if (sigaction(SIGINT, &sigint_action, NULL)) {
        HB_ERR("Couldn't register a SIGINT handler");
        return;
    }

    if (interactive) {
        goto prompt;
    }

    char * line = NULL;
    do {
        if (line && line[0]) {
            add_history(line);
            handle_cmd(t, line);
        } else if (!line) {
            sigint_recvd = false;
            cmd_cont(t, "");
        }
        if (line) {
            free(line);
        }
    prompt:
        continue;
    } while ((line = readline(PROMPT_STR)));
#endif
}

