#define _CHARIOT_SRC
#include <chariot.h>
#include <ck/command.h>
#include <ck/func.h>
#include <ck/io.h>
#include <ck/map.h>
#include <ck/parser.h>
#include <ck/ptr.h>
#include <ck/string.h>
#include <ck/vec.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysbind.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#define C_RED "\x1b[31m"
#define C_GREEN "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_BLUE "\x1b[34m"
#define C_MAGENTA "\x1b[35m"
#define C_CYAN "\x1b[36m"
#define C_RESET "\x1b[0m"
#define C_GRAY "\x1b[90m"


size_t current_us() { return syscall(SYS_gettime_microsecond); }

extern char **environ;


#define MAX_ARGS 255

struct readline_history_entry {
  char *value;

  // this is represented as a singly linked list, so it's not super fast to walk
  // backwards (down arrow)
  struct readline_history_entry *next;
};

struct readline_context {
  struct readline_history_entry *history;
};

auto read_line(int fd, char *prompt) {
  printf("%s", prompt);
  fflush(stdout);
  char *buf = (char *)malloc(4096);
  memset(buf, 0, 4096);
  fgets(buf, 4096, stdin);
  buf[strlen(buf) - 1] = '\0';

  ck::string s = (const char *)buf;
  // ck::hexdump((void*)s.get(), s.len());
  free(buf);
  return s;
}

struct exec_cmd;

struct cmd {
  using ptr = ck::unique_ptr<cmd>;
  virtual ~cmd(){};

  virtual void exec(void){};
  virtual void dump(void){};

  virtual exec_cmd *as_exec(void) { return nullptr; }
};

struct exec_cmd : public cmd {
  ck::vec<ck::string> argv;

  virtual ~exec_cmd(void) {}

  virtual void exec(void) override {
    /* idk how to bubble this up... */
    if (argv.size() == 0) {
      /* TODO: parse better? */
      exit(EXIT_FAILURE);
    }

    /* the first argument to exec */
    auto args = new const char *[argv.size() + 1];
    args[argv.size()] = NULL;

    for (int i = 0; i < argv.size(); i++) {
      args[i] = argv[i].get();
    }

    execvpe((const char *)args[0], (const char **)args, (const char **)environ);
    exit(EXIT_FAILURE);
  }

  virtual void dump(void) override {
    printf("exec");
    for (auto &arg : argv) {
      printf(" '%s'", arg.get());
    }
  }


  bool try_builtin(void) {
    if (argv.size() > 0) {
			/* Change directory builtin */
      if (argv[0] == "cd") {
        const char *destination = NULL;
        if (argv.size() == 1) {
          // CD to home
          uid_t uid = getuid();
          struct passwd *pwd = getpwuid(uid);
          // TODO: get user $HOME and go there instead
          destination = pwd->pw_dir;
        } else {
          destination = argv[1].get();
        }


        int res = chdir(destination);
        if (res != 0) {
          printf("cd: '%s' could not be entered\n", destination);
        }
				return true;
      }
    }
    return false;
  }

  virtual exec_cmd *as_exec(void) override { return (exec_cmd *)this; }
};


struct seq_cmd : public cmd {
  seq_cmd(struct cmd *left, struct cmd *right) {
    this->left = left;
    this->right = right;
  }

  virtual ~seq_cmd(void) {}
  virtual void exec(void) override {
    int pid = fork();
    if (pid == 0) {
      left->exec();
      exit(EXIT_SUCCESS);
    }

    waitpid(pid, NULL, 0);
    right->exec();
    exit(EXIT_SUCCESS);
  }

  virtual void dump(void) override {
    printf("(seq (");
    left->dump();
    printf(") (");
    right->dump();
    printf("))");
  }

  cmd::ptr left, right;
};




struct bg_cmd : public cmd {
  bg_cmd(struct cmd *c) { this->cmd = c; }
  virtual ~bg_cmd(void) {}

  virtual void exec(void) override { exit(EXIT_FAILURE); }

  virtual void dump(void) override {
    printf("(bg (");
    cmd->dump();
    printf("))");
  }

  cmd::ptr cmd;
};


struct pipe_cmd : public cmd {
  pipe_cmd(struct cmd *left, struct cmd *right) {
    this->left = left;
    this->right = right;
  }

  virtual ~pipe_cmd(void) {}

  virtual void exec(void) override { printf("pipe_cmd\n"); }

  virtual void dump(void) override {
    printf("(pipe (");
    left->dump();
    printf(") (");
    right->dump();
    printf("))");
  }

  cmd::ptr left;
  cmd::ptr right;
};


struct redir_cmd : public cmd {
  redir_cmd(struct cmd *subcmd, ck::string file, int mode, int fd) {
    this->cmd = subcmd;
    this->file = file;
    this->mode = mode;
    this->fd = fd;
  }

  virtual ~redir_cmd(void) {}

  virtual void exec(void) override { printf("redir_cmd\n"); }


  virtual void dump(void) override {
    printf("(redir (");
    cmd->dump();
    printf(") \"%s\")", file.get());
  }

  ck::string file;
  cmd::ptr cmd;
  int mode;
  int fd;
};




char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int gettoken(char **ps, char *es, char **q, char **eq) {
  char *s;
  int ret;

  s = *ps;
  while (s < es && strchr(whitespace, *s)) s++;
  if (q) *q = s;
  ret = *s;
  switch (*s) {
    case 0:
      break;
    case '|':
    case '(':
    case ')':
    case ';':
    case '&':
    case '<':
      s++;
      break;
    case '>':
      s++;
      if (*s == '>') {
        ret = '+';
        s++;
      }
      break;
    default:
      ret = 'a';
      while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s)) s++;
      break;
  }
  if (eq) *eq = s;

  while (s < es && strchr(whitespace, *s)) s++;
  *ps = s;
  return ret;
}

int peek(char **ps, char *es, const char *toks) {
  char *s;

  s = *ps;
  while (s < es && strchr(whitespace, *s)) s++;
  *ps = s;
  return *s && strchr(toks, *s);
}


struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);

struct cmd *parse_cmd(char *s) {
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if (s != es) {
    fprintf(stderr, "leftovers: %s\n", s);
    panic("syntax");
  }
  return cmd;
}


struct cmd *parseredirs(struct cmd *cmd, char **ps, char *es) {
  int tok;
  char *q, *eq;

  while (peek(ps, es, "<>")) {
    tok = gettoken(ps, es, 0, 0);
    if (gettoken(ps, es, &q, &eq) != 'a') panic("missing file for redirection");
    ck::string s = ck::string(q, ((off_t)es - (off_t)q));
    switch (tok) {
      case '<':
        cmd = new redir_cmd(cmd, s, O_RDONLY, 0);
        break;
      case '>':
        cmd = new redir_cmd(cmd, s, O_WRONLY | O_CREAT | O_TRUNC, 1);
        break;
      case '+':  // >>
        cmd = new redir_cmd(cmd, s, O_WRONLY | O_CREAT, 1);
        break;
    }
  }
  return cmd;
}




struct cmd *parseblock(char **ps, char *es) {
  struct cmd *cmd;

  if (!peek(ps, es, "(")) panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if (!peek(ps, es, ")")) panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd *parseexec(char **ps, char *es) {
  char *q, *eq;
  int tok;
  struct exec_cmd *cmd;
  struct cmd *ret;

  if (peek(ps, es, "(")) return parseblock(ps, es);

  ret = new exec_cmd();
  cmd = (struct exec_cmd *)ret;

  ret = parseredirs(ret, ps, es);
  while (!peek(ps, es, "|)&;")) {
    if ((tok = gettoken(ps, es, &q, &eq)) == 0) break;
    if (tok != 'a') panic("syntax");
    int len = (off_t)eq - (off_t)q;
    cmd->argv.push(ck::string(q, len));
    ret = parseredirs(ret, ps, es);
  }
  return ret;
}


struct cmd *parsepipe(char **ps, char *es) {
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if (peek(ps, es, "|")) {
    gettoken(ps, es, 0, 0);
    cmd = new pipe_cmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}



struct cmd *parseline(char **ps, char *es) {
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while (peek(ps, es, "&")) {
    gettoken(ps, es, 0, 0);
    cmd = new bg_cmd(cmd);
  }
  if (peek(ps, es, ";")) {
    gettoken(ps, es, 0, 0);
    cmd = new seq_cmd(cmd, parseline(ps, es));
  }
  return cmd;
}



#define RUN_NOFORK 1



int run_line(ck::string line, int flags = 0) {
  auto *cmd = parse_cmd((char *)line.get());

  if (cmd == NULL) {
    fprintf(stderr, "sh: Syntax error in '%s'\n", line.get());
    return -1;
  }

  // printf("command: ");
  // cmd->dump();
  // printf("\n");

  if (auto *e = cmd->as_exec(); e != NULL) {
    if (e->try_builtin()) {
      // delete cmd;
      return 0;
    }
  }

  if ((flags & RUN_NOFORK) == 0) {
    int pid = fork();
    if (pid == 0) {
      cmd->exec();
      exit(EXIT_FAILURE);
    }

    /* wait for the subproc */
    do waitpid(pid, NULL, 0);
    while (errno == EINTR);
  } else {
    /* Don't fork */
    cmd->exec();
  }


  // delete cmd;
  return 0;


  int err = 0;
  ck::vec<ck::string> parts = line.split(' ', false);
  ck::vec<const char *> args;

  if (parts.size() == 0) {
    return -1;
  }

  // convert it to a format that is usable by execvpe
  for (auto &p : parts) args.push(p.get());
  args.push(nullptr);

  if (parts[0] == "cd") {
    const char *path = args[1];
    if (path == NULL) {
      uid_t uid = getuid();
      struct passwd *pwd = getpwuid(uid);
      // TODO: get user $HOME and go there instead
      path = pwd->pw_dir;
    }
    int res = chdir(path);
    if (res != 0) {
      printf("cd: '%s' could not be entered\n", args[1]);
    }
    return err;
  }

  if (parts[0] == "exit") {
    exit(0);
  }


  pid_t pid = fork();
  if (pid == 0) {
    execvpe(args[0], args.data(), (const char **)environ);
    printf("execvpe returned errno '%s'\n", strerror(errno));
    exit(-1);
  }

  int stat = 0;
  waitpid(pid, &stat, 0);

  int exit_code = WEXITSTATUS(stat);
  if (exit_code != 0) {
    fprintf(stderr, "%s: exited with code %d\n", args[0], exit_code);
  }


  return err;
}



void lispy_thing(ck::string &command);

int main(int argc, char **argv, char **envp) {

  char ch;
  const char *flags = "c:";
  while ((ch = getopt(argc, argv, flags)) != -1) {
    switch (ch) {
      case 'c': {
        run_line(optarg, RUN_NOFORK);
        exit(EXIT_SUCCESS);
        break;
      }

      case '?':
        puts("sh: invalid option\n");
        return -1;
    }
  }

  char prompt[256];
  char uname[32];
  char *cwd[255];
  char hostname[50];

  int hn = open("/cfg/hostname", O_RDONLY);

  int n = read(hn, (void *)hostname, 50);
  if (n >= 0) {
    hostname[n] = 0;
    for (int i = n; i > 0; i--) {
      if (hostname[i] == '\n') hostname[i] = 0;
    }
  }

  close(hn);


  uid_t uid = getuid();
  struct passwd *pwd = getpwuid(uid);
  strncpy(uname, pwd->pw_name, 32);


  setenv("USER", pwd->pw_name, 1);
  setenv("SHELL", pwd->pw_shell, 1);
  setenv("HOME", pwd->pw_dir, 1);

  while (1) {
    syscall(SYS_getcwd, cwd, 255);
    setenv("CWD", (const char *)cwd, 1);


    const char *disp_cwd = (const char *)cwd;
    if (strcmp((const char *)cwd, pwd->pw_dir) == 0) {
      disp_cwd = "~";
    }

    snprintf(prompt, 256, "[%s@%s %s]%c ", uname, hostname, disp_cwd, uid == 0 ? '#' : '$');


    ck::string line = read_line(0, prompt);
    if (line.len() == 0) continue;

    // ck::hexdump((void *)line.get(), line.len());
    run_line(line);
  }

  return 0;
}



#if 0
// allow programatic access to defining token
// types, but by only actually defining them once
#define FOREACH_TOKEN_TYPE(V) \
  V(tok_number, 1)            \
  V(tok_string, 2)            \
  V(tok_right_paren, 3)       \
  V(tok_left_paren, 4)        \
  V(tok_symbol, 6)            \
  V(tok_keyword, 7)           \
  V(tok_quote, 8)             \
  V(tok_unq, 9)               \
  V(tok_splice, 10)           \
  V(tok_backquote, 11)        \
  V(tok_left_bracket, 12)     \
  V(tok_right_bracket, 13)    \
  V(tok_left_curly, 14)       \
  V(tok_right_curly, 15)      \
  V(tok_hash_modifier, 16)    \
  V(tok_backslash, 17)

enum tok {
#define V(name, code) name = code,
  FOREACH_TOKEN_TYPE(V)
#undef V
};


class lispy_lexer : public ck::lexer {
  ck::map<char, char> esc_mappings;

 public:
  lispy_lexer(ck::string &s) : ck::lexer(s) {
    esc_mappings['a'] = 0x07;
    esc_mappings['b'] = 0x08;
    esc_mappings['f'] = 0x0C;
    esc_mappings['n'] = 0x0A;
    esc_mappings['r'] = 0x0D;
    esc_mappings['t'] = 0x09;
    esc_mappings['v'] = 0x0B;
    esc_mappings['\\'] = 0x5C;
    esc_mappings['"'] = 0x22;
    esc_mappings['e'] = 0x1B;
  }
  virtual ~lispy_lexer(void) {
    // nah
  }

  virtual ck::token lex(void) {
    skip_spaces();
    int32_t c = next();

    auto in_set = [](ck::string &set, int c) {
      for (auto &n : set) {
        if (n == c) return true;
      }
      return false;
    };

    auto accept_run = [&](ck::string set) {
      ck::string buf;
      while (in_set(set, peek())) {
        buf += next();
      }
      return buf;
    };

    if (c == ';' || c == '#') {
      if (c == '#' && peek() != '!') goto skip_comment;
      while (peek() != '\n' && (int32_t)peek() != -1) next();

      return lex();
    }
  skip_comment:

    // skip over spaces by calling again
    if (isspace(c) || c == ',') return lex();

    if (c == EOF || c == 0) {
      return tok(TOK_EOF, "");
    }
    if (c == '`') {
      return tok(tok_backquote, "`");
    }

    if (c == '~') {
      if (peek() == '@') {
        next();
        return tok(tok_splice, ",@");
      }
      return tok(tok_unq, "~");
    }

    if (c == '#') {
      ck::string t;
      t += '#';

      /* Parse raw string literals */
      if (peek() == '"') {
        next();
        ck::string buf;
        while (true) {
          c = next();
          if ((int32_t)c == -1) return tok(TOK_ERR, "unterminated string");
          if (c == '"') break;
          if (c == '\\') {
            if (peek() == '"') {
              buf += '"';
              next();
              continue;
            } else if (peek() == '\\') {
              buf += '\\';
              next();
              continue;
            } else {
              buf += '\\';
              continue;
            }
          }
          buf += c;
        }
        return tok(tok_string, buf);
      }

      if (isalpha(peek())) {
        t += next();
      } else {
        return tok(TOK_ERR, "invalid hash modifier syntax");
      }
      return tok(tok_hash_modifier, t);
    }
    if (c == '\\') return tok(tok_backslash, "\\");

    if (c == '(') return tok(tok_left_paren, "(");

    if (c == ')') return tok(tok_right_paren, ")");

    if (c == '[') return tok(tok_left_bracket, "[");

    if (c == ']') return tok(tok_right_bracket, "]");

    if (c == '{') return tok(tok_left_curly, "{");

    if (c == '}') return tok(tok_right_curly, "}");

    if (c == '\'') return tok(tok_quote, "'");

    if (c == '"') {
      ck::string buf;

      bool escaped = false;
      // ignore the first quote because a string shouldn't
      // contain the encapsulating quotes in it's internal representation
      while (true) {
        c = next();
        if ((int32_t)c == EOF) return tok(TOK_ERR, "unterminated string");
        // also ignore the last double quote for the same reason as above
        if (c == '"') break;
        escaped = c == '\\';
        if (escaped) {
          char e = next();
          if (e == 'U' || e == 'u') {
            /*
// parse 32 bit unicode literals
std::string hex;

int l = 8;

if (e == 'u') l = 4;

for (int i = 0; i < l; i++) {
hex += next();
}
std::cout << hex << std::endl;
c = (int32_t)std::stoul(hex, nullptr, 16);
printf("%u\n", c);
            */
          } else {
            c = esc_mappings[e];
            if (c == 0) {
              return tok(TOK_ERR, "unknown escape sequence");
            }
          }
          escaped = false;
        }
        buf += c;
      }
      return tok(tok_string, buf);
    }

    // parse a number
    if (isdigit(c) || c == '.' || (c == '-' && isdigit(peek()))) {
      // it's a number (or it should be) so we should parse it as such

      ck::string buf;

      buf += c;
      bool has_decimal = c == '.';


      if (peek() == 'x') {
        next();
        ck::string hex = accept_run("0123456789abcdefABCDEF");
        unsigned long long x;
        sscanf(hex.get(), "%llx", &x);
        char buf[64];
        snprintf(buf, 64, "%lld", x);
        return tok(tok_number, buf);
      }


      if (peek() == 'o') {
        next();
        ck::string oct = accept_run("012345567");
        unsigned long long x;
        sscanf(oct.get(), "%llo", &x);
        char buf[64];
        snprintf(buf, 64, "%lld", x);
        return tok(tok_number, buf);
      }


      while (isdigit(peek()) || peek() == '.') {
        c = next();
        buf += c;
        if (c == '.') {
          if (has_decimal) {
            return tok(tok_symbol, buf);
          }
          has_decimal = true;
        }
      }

      if (buf == ".") {
        return tok(tok_symbol, buf);
      }
      return tok(tok_number, buf);
    }  // digit parsing

    // it wasn't anything else, so it must be
    // either an symbol or a keyword token

    ck::string symbol;
    symbol += c;

    while (!in_charset(peek(), " \n\t(){}[],'`@")) {
      auto v = next();
      if (v == EOF || v == 0) break;
      symbol += v;
    }

    if (symbol.len() == 0) return tok(TOK_ERR, "lexer encountered zero-length identifier");

    uint8_t type = tok_symbol;

    if (symbol[0] == ':') {
      if (symbol.size() == 1) return tok(TOK_ERR, "Keyword token must have at least one character after the ':'");
      type = tok_keyword;
    }

    return tok(type, symbol);
  }
};

#endif

enum tok {
  tok_arg,
  tok_str,
  tok_var,
  tok_right_paren,
  tok_left_paren,
  tok_dollar,
  tok_left_bracket,
  tok_right_bracket,
  tok_left_curly,
  tok_right_curly
};

class shell_lexer : public ck::lexer {
  ck::map<char, char> esc_mappings;

 public:
  shell_lexer(ck::string &s) : ck::lexer(s) {
    esc_mappings['a'] = 0x07;
    esc_mappings['b'] = 0x08;
    esc_mappings['f'] = 0x0C;
    esc_mappings['n'] = 0x0A;
    esc_mappings['r'] = 0x0D;
    esc_mappings['t'] = 0x09;
    esc_mappings['v'] = 0x0B;
    esc_mappings['\\'] = 0x5C;
    esc_mappings['"'] = 0x22;
    esc_mappings['e'] = 0x1B;
  }
  virtual ~shell_lexer(void) {
    // nah
  }

  virtual ck::token lex(void) {
    skip_spaces();
    int32_t c = next();

    auto in_set = [](ck::string &set, int c) {
      for (auto &n : set) {
        if (n == c) return true;
      }
      return false;
    };

    auto accept_run = [&](ck::string set) {
      ck::string buf;
      while (in_set(set, peek())) {
        buf += next();
      }
      return buf;
    };

    if (c == '#') {
      // if (c == '#' && peek() != '!') goto skip_comment;
      while (peek() != '\n' && (int32_t)peek() != -1) next();

      return lex();
    }
  skip_comment:

    skip_spaces();

    if (c == EOF || c == 0) {
      return tok(TOK_EOF, "");
    }

    // if (c == '\\') return tok(tok_backslash, "\\");
    if (c == '(') return tok(tok_left_paren, "(");

    if (c == ')') return tok(tok_right_paren, ")");

    if (c == '[') return tok(tok_left_bracket, "[");

    if (c == ']') return tok(tok_right_bracket, "]");

    if (c == '{') return tok(tok_left_curly, "{");

    if (c == '}') return tok(tok_right_curly, "}");

    if (c == '"') {
      ck::string buf;

      bool escaped = false;
      // ignore the first quote because a string shouldn't
      // contain the encapsulating quotes in it's internal representation
      while (true) {
        c = next();
        if ((int32_t)c == EOF) return tok(TOK_ERR, "unterminated string");
        // also ignore the last double quote for the same reason as above
        if (c == '"') break;
        escaped = c == '\\';
        if (escaped) {
          char e = next();
          if (e == 'U' || e == 'u') {
            /*
// parse 32 bit unicode literals
std::string hex;

int l = 8;

if (e == 'u') l = 4;

for (int i = 0; i < l; i++) {
hex += next();
}
std::cout << hex << std::endl;
c = (int32_t)std::stoul(hex, nullptr, 16);
printf("%u\n", c);
            */
          } else {
            c = esc_mappings[e];
            if (c == 0) {
              return tok(TOK_ERR, "unknown escape sequence");
            }
          }
          escaped = false;
        }
        buf += c;
      }
      return tok(tok_str, buf);
    }


    // it wasn't anything else, so it must be
    // either an symbol or a keyword token

    ck::string symbol;
    symbol += c;

    while (!in_charset(peek(), " \n\t(){}[],'`@")) {
      auto v = next();
      if (v == EOF || v == 0) break;
      symbol += v;
    }

    if (symbol.len() == 0) return tok(TOK_ERR, "lexer encountered zero-length identifier");

    uint8_t type = tok_arg;

    if (symbol[0] == '$') {
      if (symbol.size() == 1) return tok(tok_dollar, "$");
      type = tok_var;
    }

    return tok(type, symbol);
  }
};


void lispy_thing(ck::string &src) {
  shell_lexer l(src);

  while (1) {
    auto t = l.lex();

    if (t.type == TOK_EOF) break;

    // auto val = src.substring(t.start, t.end + 1);

    printf("%d '%s' %d %d\n", t.type, t.value.get(), t.start, t.end);
  }
}
