# Process Lifetime

Processes in chariot are somewhat interesting for a unix-like kernel. There is currently
only one way to create a new process in userspace and that is via the `spawn()` syscall.
fork() is a very inefficient way of creating new systemcalls, so spawn creates an "embryo"
process which can then be started with the `startpidve()` systemcall. A typical fork/exec
from linux might look like this in chariot:

```c

	pid_t pid = spawn();

	// initialize

	if (startpidve(pid, argv[0], argv, envp)) {
		despawn(pid);
	}

```

Where you first spawn the process, you get a pid. You can then start that pid executing a
file with startpidve (quite a mouthful, I know). If it fails to execute, you then despawn
the embryonic process with despawn(). This is very similar to this code in linux:

```c

	pid_t pid = fork();

	if (pid == 0) {
		// initialize
		execve(...);
		exit(0); // equiv. of despawn(pid)
	}

```

After a process has been started, you then wait on it as if it were any other unix.
