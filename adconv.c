/*
 * Copyright (c) 2018 [n/a] info@embeddora.com All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *          notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above copyright
 *          notice, this list of conditions and the following disclaimer in the
 *          documentation and/or other materials provided with the distribution.
 *        * Neither the name of The Linux Foundation nor
 *          the names of its contributors may be used to endorse or promote
 *          products derived from this software without specific prior written
 *          permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.    IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* Macro PATH_MAX */
#include <linux/syscalls.h>

/* Macros <MODULE_LICENSE>, <MODULE_AUTHOR>, <MODULE_DESCRIPTION> */
#include <linux/module.h>

/* ulong, MODULE_PARAM()  */
#include <linux/moduleparam.h>

/* proc_create () */
#include <linux/proc_fs.h>

/* Has to be the same as std::ifstream::input(xx) in <public/stressapptest/src/src/os.cc> */
#define PROCDIRENTRY 	"_v2p"

/* Placeholder for 32-bit address in format 0x%08x */
#define ADDR32BIT	"0x12345678"


/* Remove this and you will get the problem on 'proc_create(...)' */
static int satm_proc_show(struct seq_file *m, void *v)
{
	/* Skip any activity */
	return 0;

} /* int satm_proc_show(...) */

/* Target virtual address to compute physicall address for. (Initialized as module receives a parameter.) */
static unsigned long vaddr;

#if defined (CAUTIOUS)

/* In average we've got 50 processes on embedded (OpenWRT ands alike), 100 processes on stationary (Debian family) */
#define MAX_PIDS	300

/* Executed each time some user space applicaiton (cat, stressapptest, vim, ...) tries to read cdata frmo file <file> */
static ssize_t satm_proc_read_cautious(struct file *file, char __user * gBuff, size_t size, loff_t *ppos)
{
/* Counters and Array of actore process IDs */
int i = 0, j, iPids[MAX_PIDS];

/* Local buffer to form a resulting string */
char gStr[0x80];

/* Length of 32-address in format 0x%08x plus TR0 char */
int iStrLen = strlen(ADDR32BIT) + 1;

/* Perform a translation */
phys_addr_t faTranslated = virt_to_phys((void*)vaddr);

/* Tasklist pointer */
struct task_struct * p = NULL;

/* Pointer to PID list */
struct pid *pid_struct;

/* Pointer to MM list */
struct mm_struct* mm;

/* Pointer to VMA list */
struct vm_area_struct * vma;

/* Context syncronization object */
rwlock_t tasklist_lock;

	/* Don't repeat reading of what already has been read */
	if (0 < *ppos ) return 0;

	/* Enter task-state-sensitive context */
	read_lock(&tasklist_lock);

	/* Clear PIDs array */
	memset (iPids, 0, MAX_PIDS * sizeof(int));

	/* Initialize PIDs array with PIDs of currently active processes */
	for_each_process(p)
	{
		/* Store pointer in array */
		iPids[i] = p->pid;

		/* Shift counter ahead */
		i++;

	} /* end of 'for_each_process(p)' */

	/* Leave task-state-sensitive context */
	read_unlock(&tasklist_lock);

	/* for each PID found */
	for (j = 0; (j < i) && ( 0 != iPids[j] ) ; j++)
	{
		/* Get pointer onto PID by its identificator */
		pid_struct = find_get_pid(iPids[j]);

		/* Skip if pointer onto PID is not valid */
		if (NULL == pid_struct) continue;

		/* Get pointer onto process by PID structure */
		p = get_pid_task(pid_struct, PIDTYPE_PID);

		/* Skip if pointer onto process is not valid */
		if (NULL == p) continue;

		/* Leave this structure */
		put_pid(pid_struct);

#if (0)
		/* Take pointer onto <mm_struct> of current process */
		mm = p->mm;
#else
		/* Take pointer onto _active <mm_struct> of current process */
		mm = p->active_mm;
#endif /* (0) */

		/* Skip if MM pointer is not valid */
		if (mm == NULL) continue;

		/* Enter state sensitive context*/
		down_read(&mm->mmap_sem);

		/* Take VMA head of list of current MM */
		vma = mm->mmap;

		while (1)
		{
			/* Process all till terminating VMA */
			if (NULL == vma) break;

			/* Check if address to be translated belongs to currect VMA */
			if ( (vma->vm_end >= vaddr) && (vma->vm_start <= vaddr) )
			{
#if defined (DEBUG_INFO)
				printk("Address <%p> belongs to VMA chain [%p..%p] which is occupied by process #%d, named '%s'.\n",
							(void*) vaddr, (void*) vma->vm_start, (void*)vma->vm_end, p->pid, p->comm );
#endif /* defined (DEBUG_INFO) */

				/* We can not afford V2P translation for process occupied by some process. Returning non translated */
				faTranslated = vaddr;

				/* Need to exit: breakout from internal loop */
				break;

			} /* end of 'VADDR belongs to [VM_START..VM_END]?'*/

			/* Transit to next VMA chain */
			vma = vma->vm_next;

		} /* while (1) */

		/* Leave state sensitive context*/
		up_read(&mm->mmap_sem);

		/* Need to exit: breakout from external loop */
		if (vaddr == faTranslated) break;

#if defined (DEBUG_INFO)
		printk("Prc.#%d, '%s' does not posses target virtual address %p\n", p->pid, p->comm, (void*)vaddr);
#endif /* defined (DEBUG_INFO) */

	} /* end of 'for each PID found' */

	/* Form a string with translated address represented as 32-bit hexadecimal */
	snprintf(gStr, iStrLen, "0x%08x", (unsigned int) faTranslated);

	/* Copy string buffer to userspace buffer */
	copy_to_user(gBuff, gStr, iStrLen);

	/* Shift position on amount of chars passed to a user */
	*ppos = iStrLen;

	/* Return an amount of bytes copied */
	return iStrLen;

} /* ssize_t satm_proc_read_safe(..) */

#else

/* Executed each time some user space applicaiton (cat, stressapptest, vim, ...) tries to read cdata frmo file <file> */
static ssize_t satm_proc_read(struct file *file, char __user * gBuff, size_t size, loff_t *ppos)
{
/* Local buffer to form a resulting string */
char gStr[0x80];

/* Length of 32-address in format 0x%08x plus TR0 char */
int iStrLen = strlen(ADDR32BIT) + 1;

/* Perform a translation. To avoid bulky casting use <-Wint-to-pointer-cast>, as an option. */
void * vpTranslated = (void *)((unsigned long) virt_to_phys((volatile void *)vaddr));

	/* Don't repeat reading of what already has been read */
	if (0 < *ppos ) return 0;

#if defined (DEBUG_INFO)
	printk("'stressapptest' translated address is <0x%p>\n", vpTranslated );
#endif /* defined (DEBUG_INFO) */

	/* Form a string with translated address represented as 32-bit hexadecimal */
	snprintf(gStr, iStrLen, "0x%08x", (unsigned int) vpTranslated );

	/* Copy string buffer to userspace buffer */
	copy_to_user(gBuff, gStr, iStrLen);

	/* Shift position on amount of chars passed to a user */
	*ppos = iStrLen;

	/* Return an amount of bytes copied */
	return iStrLen;

} /* ssize_t satm_proc_read(..) */

#endif /* defined (CAUTIOUS) */

/* An 'open' callback */
static int satm_proc_open(struct inode *inode, struct  file *file)
{
	/* Entire read output is upfront so let's use <single_open()> for opening */
	return single_open(file, satm_proc_show, NULL);

} /* int satm_proc_open(...) */

/* Set of pointers onto functions to take an effect while referring file <PROCDIRENTRY> */
static const struct file_operations satm_proc_fops =
{
	/* A pointer to a module owning this structure */
	.owner = THIS_MODULE,

	/* A function to notify driver the <PROCDIRENTRY> was successfully opened */
	.open = satm_proc_open,

	/* A function to serve userspace's apps read requests (including those of 'stressaptest') which is a main purpose of this driver */
	.read = 
#if defined (CAUTIOUS)
	/* Cautios translation - leave memory occupied by active processes intact, and translate the rest addresses */
	satm_proc_read_cautious,
#else
	/* Brute force translation - translate any address */
	satm_proc_read,
#endif /* defined (CAUTIOUS) */

	/* With <lseek> we navigate through a file */
	.llseek = seq_lseek,

	/* A function to take an effect on when this sctructure is being released */
	.release = single_release,

}; /* struct file_operations satm_proc_fops */

/* Module insertion into a kernel */
static int __init satm_proc_init(void)
{
#if defined (DEBUG_INFO)
	printk("'stressapptest' virt-to-phys translation module loaded with parameter vaddr=<0x%p>\n", (void*)vaddr);
#endif /* defined (DEBUG_INFO) */

	/* Create <PROCDIRENTRY> filesystem entry and assign pointers onto its file operations (see callbacks in <satm_proc_fops>) */
	proc_create(PROCDIRENTRY, 0, NULL, &satm_proc_fops);

	/* Successul funalization */
	return 0;

} /* int __init satm_proc_init() */


/* Module removal from a kernel */
static void __exit satm_proc_exit(void)
{
#if defined (DEBUG_INFO)
	printk("'stressapptest' virt-to-phys translation module removed\n");
#endif /* defined (DEBUG_INFO) */

	/* Create <PROCDIRENTRY> filesystem entry, dispose this module's structures */
	remove_proc_entry(PROCDIRENTRY, NULL);

} /* void exit satm_proc_exit() */


/* Module insertion into a kernel */
module_init(satm_proc_init);

/* Module removal from a kernel */
module_exit(satm_proc_exit);

/* Turn the <vaddr> into command line parameter */
module_param(vaddr, ulong, S_IRUGO);


/* To not 'taint' the kernel let's define 'GNU Public License v2 and more' */
MODULE_LICENSE("GPL and additional rights");

/* Author's badge */
MODULE_AUTHOR("Software Developer, <info@embeddora.com>, [n/a]");

/* For those inquisitive running 'modinfo' to learn what the module is purposed for */
MODULE_DESCRIPTION("Module to supply '/proc/self/pagemap' data to 'stressapptest' application");
