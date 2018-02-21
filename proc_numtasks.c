#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>

static int print_numtasks(struct seq_file *m, void *v) {
    struct task_struct *p, *t;
    long num_procs = 0, num_threads = 0;
    for_each_process(p) {
        num_procs++;
        for_each_thread(p, t)
            num_threads++;
    }
    seq_printf(m, "%ld total processes\n", num_procs);
    seq_printf(m, "%ld total threads\n", num_threads);
    return 0;
}

static int open_proc_numtasks(struct inode *inode, struct file *file) {
    return single_open(file, print_numtasks, NULL);
}

static struct proc_dir_entry *proc_numtasks;
static const struct file_operations proc_numtasks_fops = {
    .owner = THIS_MODULE,
    .open  = open_proc_numtasks,
    .read  = seq_read,
};

static int __init init(void) {
    // XXX what if it already exists
    proc_numtasks = proc_create("numtasks", 0, NULL, &proc_numtasks_fops);
    pr_info("installed /proc/numtasks");
    return 0;
}

static void __exit cleanup(void) {
    remove_proc_entry("numtasks", NULL);
    pr_info("uninstalled /proc/numtasks");
}

module_init(init);
module_exit(cleanup);

