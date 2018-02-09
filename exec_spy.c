/*
 * Wraps execve() syscall to log info each time it's called.
 *
 * Messes with the syscall table in a way that may be architecture and kernel
 * version specific. Developed on ubuntu artful 4.13.0-32-generic.
 *
 * Based on
 * https://github.com/bashrc/LKMPG/blob/d694ae7/4.14.8/examples/syscall.c
 * which is in turn based on example by Peter Jay Salzman and
 * https://bbs.archlinux.org/viewtopic.php?id=139406
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs_struct.h>

unsigned long **sys_call_table;
unsigned long original_cr0;

asmlinkage int (*orig_execve) (const char *, char *const [], char *const []);

asmlinkage int wrap_execve (const char *filename,
			    char *const argv[],
			    char *const envp[])
{
	int buflen, i;
	char *argv_buf, *p;
	struct path cwd_struct;
	char *cwd_buf, *cwd_path_str;
	// int result;

	buflen = 0;
	for (i = 0; argv[i]; i++) {
		buflen += strlen(argv[i]) + 1;
	}

	p = argv_buf = kmalloc(buflen, GFP_TEMPORARY);
	for (i = 0; argv[i]; i++) {
		strcpy(p, argv[i]);
		p += strlen(argv[i]) + 1;
		if (argv[i+1])
			p[-1] = ' ';
	}

	get_fs_pwd(current->fs, &cwd_struct);
	cwd_buf = (char *)__get_free_page(GFP_TEMPORARY);
	cwd_path_str = d_path(&cwd_struct, cwd_buf, PAGE_SIZE);
	if (IS_ERR(cwd_path_str))
		cwd_path_str = "<error>";

	pr_info("exec_spy.c: execve pid=%d uid=%d cwd=%s exe=%s "
		":: %s\n",
		current->pid, __kuid_val(current_uid()), cwd_path_str,
		filename, argv_buf);

	kfree(argv_buf);
	free_page((unsigned long)cwd_buf);
	path_put(&cwd_struct);

	/*
	   result = orig_execve(filename, argv, envp);

	   pr_info("exec-spy.c: execve returned %d\n", result);

	   return result;
	   */
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

static int __init exec_spy_init(void)
{
	pr_info("exec_spy.c: loading\n");
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

	pr_info("exec_spy.c: loaded: now logging calls to execve\n");

	return 0;
}

static void __exit exec_spy_exit(void)
{
	if(!sys_call_table) {
		pr_info("exec_spy.c: sys_call_table unset?\n");
		return;
	}
	pr_info("exec_spy.c: unloading\n");

	// put the original execve back in place
	if (sys_call_table[__NR_execve] != (unsigned long *)wrap_execve) {
		pr_alert("Somebody else also played with the execve system call");
		pr_alert("The system may be left in an unstable state.\n");
	}

	write_cr0(original_cr0 & ~0x00010000);
	sys_call_table[__NR_execve] = (unsigned long *)orig_execve;
	write_cr0(original_cr0);

	pr_info("exec_spy.c: unloaded\n");

	msleep(2000); // XXX why?
}

module_init(exec_spy_init);
module_exit(exec_spy_exit);

MODULE_LICENSE("GPL");
