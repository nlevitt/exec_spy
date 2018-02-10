My Toy Kernel Module Just For Fun
---------------------------------

`exec_spy.c` is a linux kernel module which intercepts and wraps the execve(2)
syscall and logs the command being run (or attempted), arguments, and a few
other pieces of information (pid, uid, cwd).

It accomplishes this by messing with the syscall table in a way that might be
architecture (x86_64) and kernel version specific. Developed on Ubuntu 17.10
"artful" and kernel 4.13.0-32-generic. Might have bugs and crash your computer,
so don't run it anywhere that matters.

To build, first make sure `/lib/modules/$(uname -r)/build` contains your
already-built kernel source tree. Then run `make`. For more information see
https://www.kernel.org/doc/Documentation/kbuild/modules.txt

Load with `insmod exec_spy.ko`, unload with `rmmod exec_spy`.

Note: *The kernel already has this functionality (and much more) built into
it.* Try this:

```shell
sudo auditctl -a exit,always -F arch=b64 -S execve
sudo auditctl -a exit,always -F arch=b32 -S execve
sudo auditctl -a exit,always -F arch=b64 -S execveat
sudo auditctl -a exit,always -F arch=b32 -S execveat
sudo less +F /var/log/audit/audit.log
```
