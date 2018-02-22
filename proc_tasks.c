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

static void recurse(struct seq_file *m, struct task_struct *p, int depth) {
    struct task_struct *th, *ch;
    struct list_head *list;
    int thread_count = 0;

    seq_printf(m, "%*spid=%d tgid=%d %s\n", depth*2, "", p->pid, p->tgid, p->comm);
    for_each_thread(p, th) {
        if (thread_count > 0)
            seq_printf(m, "%*s (thread) pid=%d tgid=%d ppid=%d %s\n",
                       depth*2, "", th->pid, th->tgid, th->parent->pid, th->comm);
        thread_count += 1;
    }

    list_for_each(list, &p->children) {
        ch = list_entry(list, struct task_struct, sibling);
        recurse(m, ch, depth+1);
    }
}

static int print_tasks(struct seq_file *m, void *v) {
    recurse(m, &init_task, 0);
    return 0;
}

static int open_proc_tasks(struct inode *inode, struct file *file) {
    return single_open(file, print_tasks, NULL);
}

static struct proc_dir_entry *proc_tasks;
static const struct file_operations proc_tasks_fops = {
    .owner = THIS_MODULE,
    .open  = open_proc_tasks,
    .read  = seq_read,
};

static int __init init(void) {
    // XXX what if it already exists
    proc_numtasks = proc_create("numtasks", 0, NULL, &proc_numtasks_fops);
    proc_tasks = proc_create("tasks", 0, NULL, &proc_tasks_fops);
    pr_info("proc_tasks.c: installed /proc/numtasks and /proc/tasks\n");
    return 0;
}

static void __exit cleanup(void) {
    remove_proc_entry("numtasks", NULL);
    remove_proc_entry("tasks", NULL);
    pr_info("proc_tasks.c: uninstalled /proc/numtasks and /proc/tasks\n");
}

module_init(init);
module_exit(cleanup);
