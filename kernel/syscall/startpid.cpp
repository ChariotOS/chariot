#include <cpu.h>
#include <syscall.h>
#include <util.h>

int sys::startpidve(int pid, char const *upath, char const **uargv,
                    char const **uenvp) {

  auto proc = curproc;
  assert(proc != NULL);


  int emb = 0;
  process::ptr np = nullptr;
  {
    scoped_lock lck(proc->datalock);

    for (emb = 0; emb < proc->embryos.size(); emb++) {
      auto p = proc->embryos[emb];
      assert(p);

      if (p->pid == pid) {
        np = p;
        break;
      }
    }

    if (!np) return -ENOENT;
  }

  if (upath == NULL) return -EINVAL;
  if (uargv == NULL) return -EINVAL;

	if (!proc->mm->validate_string(upath)) return -EINVAL;

  // TODO validate the pointers
  string path = upath;
  vec<string> argv;
  vec<string> envp;
  for (int i = 0; uargv[i] != NULL; i++) {
    argv.push(uargv[i]);
  }

  if (uenvp != NULL) {
    for (int i = 0; uenvp[i] != NULL; i++) {
      envp.push(uenvp[i]);
    }
  }

  int res = np->exec(path, argv, envp);

  if (res == 0) {
    proc->embryos.remove(emb);
    proc->children.push(np);
  }

  return res;
}

