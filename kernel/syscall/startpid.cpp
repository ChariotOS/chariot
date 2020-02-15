#include <cpu.h>
#include <process.h>

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

    proc->embryos.remove(emb);
  }

  if (upath == NULL) return -1;
  if (uargv == NULL) return -1;

  // TODO validate the pointers
  string path = upath;
  vec<string> argv;
  vec<string> envp;

  for (int i = 0; uargv[i] != NULL; i++) {
    // printk("a[%d] = %p\n", i, uargv[i]);
    argv.push(uargv[i]);
  }

  if (uenvp != NULL) {
    for (int i = 0; uenvp[i] != NULL; i++) {
      // printk("e[%d] = %p\n", i, uenvp[i]);
      envp.push(uenvp[i]);
    }
  }

  int res = np->exec(path, argv, envp);

  // printk("res = %d\n", res);


  if (res != 0) {
    sched::proc::ptable_remove(np->pid);
  }

  // TODO: delete the process on failure to start
  return res;
}
