/* Intercepts execve() syscall and logs info.
 *
 * Disables page protection at a processor level by changing the 16th bit in
 * the cr0 register (could be Intel specific)
 *
 * Based on https://github.com/bashrc/LKMPG/blob/d694ae7/4.14.8/examples/syscall.c
 * which is in turn based on example by Peter Jay Salzman and
 * https://bbs.archlinux.org/viewtopic.php?id=139406
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <asm/paravirt.h>
#include <linux/moduleparam.h>  /* which will have params */
#include <linux/unistd.h>       /* The list of system calls */
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

unsigned long **sys_call_table;
unsigned long original_cr0;

asmlinkage int (*orig_execve) (const char *, char *const [], char *const []);

asmlinkage int wrap_execve (const char *filename,
                            char *const argv[],
                            char *const envp[])
{
    int buflen, i;
    char *buf, *p;

    buflen = 0;
    for (i = 0; argv[i]; i++) {
        buflen += strlen(argv[i]) + 1;
    }

    p = buf = kmalloc(buflen, GFP_KERNEL);
    for (i = 0; argv[i]; i++) {
        strcpy(p, argv[i]);
        p += strlen(argv[i]) + 1;
        if (argv[i+1])
            p[-1] = ' ';
    }

    pr_info("akkt.c: execve uid=%d pid=%d filename=\"%s\" args=\"%s\"\n",
            __kuid_val(current_uid()), current->pid, filename, buf);

    kfree(buf);

    return orig_execve(filename, argv, envp);
}

static unsigned long **aquire_sys_call_table(void)
{
    unsigned long int offset = PAGE_OFFSET;
    unsigned long **sct;

    while (offset < ULLONG_MAX) {
        sct = (unsigned long **)offset;

        if (sct[__NR_close] == (unsigned long *) sys_close)
            return sct;

        offset += sizeof(void *);
    }

    return NULL;
}

static int __init akkt_init(void)
{
    pr_info("akkt.c: loading\n");
    if(!(sys_call_table = aquire_sys_call_table()))
        return -1;

    // disable syscall table page protection
    original_cr0 = read_cr0();
    write_cr0(original_cr0 & ~0x00010000);

    // replace execve with our wrapper
    orig_execve = (void*)sys_call_table[__NR_execve];
    sys_call_table[__NR_execve] = (unsigned long *)wrap_execve;

    // re-enable syscall table page protection
    write_cr0(original_cr0);

    pr_info("akkt.c: loaded: now logging calls to execve\n");

    return 0;
}

static void __exit akkt_exit(void)
{
    if(!sys_call_table) {
        pr_info("akkt.c: sys_call_table unset?\n");
        return;
    }
    pr_info("akkt.c: unloading\n");

    /*
     * Return the system call back to normal
     */
    if (sys_call_table[__NR_execve] != (unsigned long *)wrap_execve) {
        pr_alert("Somebody else also played with the execve system call");
        pr_alert("The system may be left in an unstable state.\n");
    }

    write_cr0(original_cr0 & ~0x00010000);
    sys_call_table[__NR_execve] = (unsigned long *)orig_execve;
    write_cr0(original_cr0);

    pr_info("akkt.c: unloaded\n");

    msleep(2000); // XXX why?
}

module_init(akkt_init);
module_exit(akkt_exit);

MODULE_LICENSE("GPL");
