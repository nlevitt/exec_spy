/*
 *  Disables page protection at a processor level by
 *  changing the 16th bit in the cr0 register (could be Intel specific)
 *
 *  Based on example by Peter Jay Salzman and
 *  https://bbs.archlinux.org/viewtopic.php?id=139406
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

unsigned long **sys_call_table;
unsigned long original_cr0;

asmlinkage int (*orig_execve) (const char *, char *const [], char *const []);

asmlinkage int wrap_execve (const char *filename,
                            char *const argv[],
                            char *const envp[])
{
    int i;
    pr_info("execve filename='%s'", filename);
    pr_info("  argv:");
    for (i = 0; argv[i]; i++) {
        pr_info("    %s", argv[i]);
    }
    pr_info("  envp:");
    for (i = 0; envp[i]; i++) {
        pr_info("    %s", envp[i]);
    }

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

static int __init syscall_start(void)
{
    if(!(sys_call_table = aquire_sys_call_table()))
        return -1;

    original_cr0 = read_cr0();

    write_cr0(original_cr0 & ~0x00010000);

    /* keep track of the original open function */
    orig_execve = (void*)sys_call_table[__NR_execve];

    /* use our open function instead */
    sys_call_table[__NR_execve] = (unsigned long *)wrap_execve;

    write_cr0(original_cr0);

    pr_info("will start logging calls to execve (maybe?)");

    return 0;
}

static void __exit syscall_end(void)
{
    if(!sys_call_table) {
        return;
    }

    /*
     * Return the system call back to normal
     */
    if (sys_call_table[__NR_execve] != (unsigned long *)wrap_execve) {
        pr_alert("Somebody else also played with the ");
        pr_alert("execve system call\n");
        pr_alert("The system may be left in ");
        pr_alert("an unstable state.\n");
    }

    write_cr0(original_cr0 & ~0x00010000);
    sys_call_table[__NR_execve] = (unsigned long *)orig_execve;
    write_cr0(original_cr0);

    msleep(2000);
}

module_init(syscall_start);
module_exit(syscall_end);

MODULE_LICENSE("GPL");
