/*
 * NVRAM variable manipulation (Linux kernel half)
 *
 * Copyright 2005, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: nvram_linux.c,v 1.1 2007/06/08 07:38:05 arthur Exp $
 */
#define ASUS_NVRAM

#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/spinlock.h>

#include <linux/mm.h>
#include <linux/version.h>
#include <linux/proc_fs.h>		// create_proc_read_entry()

#include <linux/fs.h>
#include <linux/mtd/mtd.h>

#include <nvram/typedefs.h>
#include <nvram/bcmendian.h>
#include <nvram/bcmnvram.h>
#include <nvram/bcmutils.h>

#include "nvram.c"


#define MTD_NVRAM_NAME	"Config"

#define mem_map_reserve(p)		set_bit(PG_reserved, &((p)->flags))
#define mem_map_unreserve(p)		clear_bit(PG_reserved, &((p)->flags))

extern int ra_mtd_read_nm(char *name, loff_t from, size_t len, u_char *buf);
extern int ra_mtd_write_nm(char *name, loff_t to, size_t len, const u_char *buf);

/* Globals */
static spinlock_t nvram_lock = SPIN_LOCK_UNLOCKED;
static struct semaphore nvram_sem;
static int nvram_major = -1;
static struct proc_dir_entry *g_pdentry = NULL;
static char *nvram_buf = NULL;
static unsigned long nvram_offset = 0;

// from src/shared/bcmutils.c
/*******************************************************************************
 * crc8
 *
 * Computes a crc8 over the input data using the polynomial:
 *
 *       x^8 + x^7 +x^6 + x^4 + x^2 + 1
 *
 * The caller provides the initial value (either CRC8_INIT_VALUE
 * or the previous returned value) to allow for processing of
 * discontiguous blocks of data.  When generating the CRC the
 * caller is responsible for complementing the final return value
 * and inserting it into the byte stream.  When checking, a final
 * return value of CRC8_GOOD_VALUE indicates a valid CRC.
 *
 * Reference: Dallas Semiconductor Application Note 27
 *   Williams, Ross N., "A Painless Guide to CRC Error Detection Algorithms",
 *     ver 3, Aug 1993, ross@guest.adelaide.edu.au, Rocksoft Pty Ltd.,
 *     ftp://ftp.rocksoft.com/clients/rocksoft/papers/crc_v3.txt
 *
 * ****************************************************************************
 */

static uint8 crc8_table[256] = {
    0x00, 0xF7, 0xB9, 0x4E, 0x25, 0xD2, 0x9C, 0x6B,
    0x4A, 0xBD, 0xF3, 0x04, 0x6F, 0x98, 0xD6, 0x21,
    0x94, 0x63, 0x2D, 0xDA, 0xB1, 0x46, 0x08, 0xFF,
    0xDE, 0x29, 0x67, 0x90, 0xFB, 0x0C, 0x42, 0xB5,
    0x7F, 0x88, 0xC6, 0x31, 0x5A, 0xAD, 0xE3, 0x14,
    0x35, 0xC2, 0x8C, 0x7B, 0x10, 0xE7, 0xA9, 0x5E,
    0xEB, 0x1C, 0x52, 0xA5, 0xCE, 0x39, 0x77, 0x80,
    0xA1, 0x56, 0x18, 0xEF, 0x84, 0x73, 0x3D, 0xCA,
    0xFE, 0x09, 0x47, 0xB0, 0xDB, 0x2C, 0x62, 0x95,
    0xB4, 0x43, 0x0D, 0xFA, 0x91, 0x66, 0x28, 0xDF,
    0x6A, 0x9D, 0xD3, 0x24, 0x4F, 0xB8, 0xF6, 0x01,
    0x20, 0xD7, 0x99, 0x6E, 0x05, 0xF2, 0xBC, 0x4B,
    0x81, 0x76, 0x38, 0xCF, 0xA4, 0x53, 0x1D, 0xEA,
    0xCB, 0x3C, 0x72, 0x85, 0xEE, 0x19, 0x57, 0xA0,
    0x15, 0xE2, 0xAC, 0x5B, 0x30, 0xC7, 0x89, 0x7E,
    0x5F, 0xA8, 0xE6, 0x11, 0x7A, 0x8D, 0xC3, 0x34,
    0xAB, 0x5C, 0x12, 0xE5, 0x8E, 0x79, 0x37, 0xC0,
    0xE1, 0x16, 0x58, 0xAF, 0xC4, 0x33, 0x7D, 0x8A,
    0x3F, 0xC8, 0x86, 0x71, 0x1A, 0xED, 0xA3, 0x54,
    0x75, 0x82, 0xCC, 0x3B, 0x50, 0xA7, 0xE9, 0x1E,
    0xD4, 0x23, 0x6D, 0x9A, 0xF1, 0x06, 0x48, 0xBF,
    0x9E, 0x69, 0x27, 0xD0, 0xBB, 0x4C, 0x02, 0xF5,
    0x40, 0xB7, 0xF9, 0x0E, 0x65, 0x92, 0xDC, 0x2B,
    0x0A, 0xFD, 0xB3, 0x44, 0x2F, 0xD8, 0x96, 0x61,
    0x55, 0xA2, 0xEC, 0x1B, 0x70, 0x87, 0xC9, 0x3E,
    0x1F, 0xE8, 0xA6, 0x51, 0x3A, 0xCD, 0x83, 0x74,
    0xC1, 0x36, 0x78, 0x8F, 0xE4, 0x13, 0x5D, 0xAA,
    0x8B, 0x7C, 0x32, 0xC5, 0xAE, 0x59, 0x17, 0xE0,
    0x2A, 0xDD, 0x93, 0x64, 0x0F, 0xF8, 0xB6, 0x41,
    0x60, 0x97, 0xD9, 0x2E, 0x45, 0xB2, 0xFC, 0x0B,
    0xBE, 0x49, 0x07, 0xF0, 0x9B, 0x6C, 0x22, 0xD5,
    0xF4, 0x03, 0x4D, 0xBA, 0xD1, 0x26, 0x68, 0x9F
};

uint8
hndcrc8(
	uint8 *pdata,	/* pointer to array of data to process */
	uint  nbytes,	/* number of input data bytes to process */
	uint8 crc	/* either CRC8_INIT_VALUE or previous return value */
)
{
	/* hard code the crc loop instead of using CRC_INNER_LOOP macro
	 * to avoid the undefined and unnecessary (uint8 >> 8) operation.
	 */
	while (nbytes-- > 0)
		crc = crc8_table[(crc ^ *pdata++) & 0xff];

	return crc;
}

#define NVRAM_DRIVER_VERSION	"0.02"

static int 
nvram_proc_version_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	int len = 0;
	struct mtd_info *nvram_mtd = get_mtd_device_nm(MTD_NVRAM_NAME);

	len += snprintf (buf+len, count-len, "nvram driver : v" NVRAM_DRIVER_VERSION "\n");
	len += snprintf (buf+len, count-len, "nvram space  : 0x%x\n", NVRAM_SPACE);
	len += snprintf (buf+len, count-len, "major number : %d\n", nvram_major);
	if (nvram_mtd)
	{
		len += snprintf (buf+len, count-len, "MTD            \n");
		len += snprintf (buf+len, count-len, "  name       : %s\n", nvram_mtd->name);
		len += snprintf (buf+len, count-len, "  index      : %d\n", nvram_mtd->index);
		len += snprintf (buf+len, count-len, "  flags      : 0x%x\n", nvram_mtd->flags);
		len += snprintf (buf+len, count-len, "  size       : 0x%x\n", nvram_mtd->size);
		len += snprintf (buf+len, count-len, "  erasesize  : 0x%x\n", nvram_mtd->erasesize);
		
		put_mtd_device(nvram_mtd);
	}

	*eof = 1;
	return len;
}

int
_nvram_read_mtd(unsigned char *buf)
{
	int ret = ra_mtd_read_nm(MTD_NVRAM_NAME, NVRAM_SPACE, NVRAM_SPACE, buf);
	if (ret) {
		return -EIO;
	}

	return 0;
}

struct nvram_tuple *
_nvram_realloc(struct nvram_tuple *t, const char *name, const char *value)
{
	if ((nvram_offset + strlen(value) + 1) > NVRAM_SPACE)	{
		return NULL;
	}

	if (!t) {
		if (!(t = kmalloc(sizeof(struct nvram_tuple) + strlen(name) + 1, GFP_ATOMIC)))	{
			return NULL;
		}

		/* Copy name */
		t->name = (char *) &t[1];
		strcpy(t->name, name);

		t->value = NULL;
	}

	/* Copy value */
	if (!t->value || strcmp(t->value, value)) {
		t->value = &nvram_buf[nvram_offset];
		strcpy(t->value, value);
		nvram_offset += strlen(value) + 1;
	}

	return t;
}

void
_nvram_free(struct nvram_tuple *t)
{
	if (!t)
		nvram_offset = 0;
	else
		kfree(t);
}

int
nvram_set(const char *name, const char *value)
{
	unsigned long flags;
	struct nvram_header *header;
	int ret;

	if (!name)
		return -EINVAL;

	// Check early write
	if (nvram_major < 0)
		return 0;

	spin_lock_irqsave(&nvram_lock, flags);
	if ((ret = _nvram_set(name, value))) {
		/* Consolidate space and try again */
		if ((header = kmalloc(NVRAM_SPACE, GFP_ATOMIC))) {
			if (_nvram_commit(header) == 0) {
				ret = _nvram_set(name, value);
			}
			kfree(header);
		}
	}
	spin_unlock_irqrestore(&nvram_lock, flags);

	return ret;
}

int
nvram_unset(const char *name)
{
	unsigned long flags;
	int ret;

	if (!name)
		return -EINVAL;

	// Check early write
	if (nvram_major < 0)
		return 0;

	spin_lock_irqsave(&nvram_lock, flags);
	ret = _nvram_unset(name);
	spin_unlock_irqrestore(&nvram_lock, flags);

	return ret;
}

char *
nvram_get(const char *name)
{
	unsigned long flags;
	char *value;
	
	if (!name)
		return NULL;
	
	// Check early read
	if (nvram_major < 0)
		return NULL;

	spin_lock_irqsave(&nvram_lock, flags);
	value = _nvram_get(name);
	spin_unlock_irqrestore(&nvram_lock, flags);

	return value;
}

int
nvram_getall(char *buf, int count)
{
	unsigned long flags;
	int ret;
	
	if (!buf || count < 1)
		return -EINVAL;

	memset(buf, 0, count);

	// Check early write
	if (nvram_major < 0)
		return 0;

	spin_lock_irqsave(&nvram_lock, flags);
	ret = _nvram_getall(buf, count);
	spin_unlock_irqrestore(&nvram_lock, flags);

	return ret;
}


int
nvram_commit(void)
{
	unsigned long flags;
	unsigned char *buf;
	int ret;
	
	// Check early commit
	if (nvram_major < 0)
		return 0;
	
	if (!(buf = kmalloc(NVRAM_SPACE, GFP_KERNEL))) {
		printk("nvram_commit: out of memory\n");
		return -ENOMEM;
	}
	
	/* Regenerate NVRAM */
	spin_lock_irqsave(&nvram_lock, flags);
	ret = _nvram_commit((struct nvram_header *)buf);
	spin_unlock_irqrestore(&nvram_lock, flags);
	if (ret)
		goto done;
	
	down(&nvram_sem);
	
	/* Write partition up to end of data area */
	ret = ra_mtd_write_nm(MTD_NVRAM_NAME, NVRAM_SPACE, NVRAM_SPACE, buf);
	if (ret) {
		printk("nvram_commit: write error\n");
	}
	
	up(&nvram_sem);
	
 done:
	
	kfree(buf);
	return ret;
}


EXPORT_SYMBOL(nvram_get);
EXPORT_SYMBOL(nvram_getall);
EXPORT_SYMBOL(nvram_set);
EXPORT_SYMBOL(nvram_unset);
EXPORT_SYMBOL(nvram_commit);

/* User mode interface below */

static ssize_t
dev_nvram_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	char tmp[100], *name = tmp, *value;
	ssize_t ret;
	unsigned long off;
	
	if (count > sizeof(tmp)) {
		if (!(name = kmalloc(count, GFP_KERNEL)))
			return -ENOMEM;
	}

	if (copy_from_user(name, buf, count)) {
		ret = -EFAULT;
		goto done;
	}

	if (*name == '\0') {
		/* Get all variables */
		ret = nvram_getall(name, count);
		if (ret == 0) {
			if (copy_to_user(buf, name, count)) {
				ret = -EFAULT;
				goto done;
			}
			ret = count;
		}
	} else {
		if (!(value = nvram_get(name))) {
			ret = 0;
			goto done;
		}

		/* Provide the offset into mmap() space */
		off = (unsigned long) value - (unsigned long) nvram_buf;

		if (put_user(off, (unsigned long *) buf)) {
			ret = -EFAULT;
			goto done;
		}

		ret = sizeof(unsigned long);
	}

done:
	if (name != tmp)
		kfree(name);

	return ret;
}

static ssize_t
dev_nvram_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	char tmp[100], *name = tmp, *value;
	ssize_t ret;

	if (count > sizeof(tmp)) {
		if (!(name = kmalloc(count, GFP_KERNEL)))
			return -ENOMEM;
	}

	if (copy_from_user(name, buf, count)) {
		ret = -EFAULT;
		goto done;
	}

	value = name;
	name = strsep(&value, "=");

	if (value)
		ret = nvram_set(name, value) ? : count;
	else
		ret = nvram_unset(name) ? : count;

 done:
	if (name != tmp)
		kfree(name);

	return ret;
}

static long dev_nvram_ioctl(struct file *file, unsigned int req, unsigned long arg)
{
	if (req != NVRAM_MAGIC)
		return -EINVAL;

	if(arg==0)
		return nvram_commit();
	
	return 0;
}

static int
dev_nvram_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long offset = virt_to_phys(nvram_buf);

	int ret;
	if ((ret = remap_pfn_range(vma, vma->vm_start, offset >> PAGE_SHIFT, vma->vm_end-vma->vm_start, vma->vm_page_prot)))
	{
		return -EAGAIN;
	}

	return 0;
}

static int
dev_nvram_open(struct inode *inode, struct file * file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	MOD_INC_USE_COUNT;
#else
	try_module_get(THIS_MODULE);
#endif
	return 0;
}

static int
dev_nvram_release(struct inode *inode, struct file * file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	MOD_DEC_USE_COUNT;
#else
	module_put(THIS_MODULE);
#endif
	return 0;
}

static struct file_operations dev_nvram_fops = {
	owner:		THIS_MODULE,
	open:		dev_nvram_open,
	release:	dev_nvram_release,
	read:		dev_nvram_read,
	write:		dev_nvram_write,
	unlocked_ioctl:	dev_nvram_ioctl,
	mmap:		dev_nvram_mmap,
};

static void
dev_nvram_exit(void)
{
	int order = 0;
	struct page *page, *end;

	if (nvram_major >= 0)
		unregister_chrdev(nvram_major, MTD_NVRAM_NAME);

	if (g_pdentry != NULL)	{
		remove_proc_entry(MTD_NVRAM_NAME, NULL);
	}

	if (nvram_buf) {
		while ((PAGE_SIZE << order) < NVRAM_SPACE)
			order++;
		end = virt_to_page(nvram_buf + (PAGE_SIZE << order) - 1);
		for (page = virt_to_page(nvram_buf); page <= end; page++)
			mem_map_unreserve(page);
	
		kfree (nvram_buf);
	}

	_nvram_uninit();
}

static int __init
dev_nvram_init(void)
{
	int order = 0, ret = 0;
	struct page *page, *end;

	/* Initialize hash table lock */
	spin_lock_init(&nvram_lock);

	/* Initialize commit semaphore */
	init_MUTEX(&nvram_sem);

	nvram_buf = kmalloc(NVRAM_SPACE, GFP_ATOMIC);
	if (!nvram_buf)
		return -ENOMEM;
	
	memset(nvram_buf, 0, NVRAM_SPACE);

	/* Allocate and reserve memory to mmap() */
	while ((PAGE_SIZE << order) < NVRAM_SPACE)
		order++;
	end = virt_to_page(nvram_buf + (PAGE_SIZE << order) - 1);
	for (page = virt_to_page(nvram_buf); page <= end; page++)	{
		mem_map_reserve(page);
	}

	/* Initialize hash table */
	_nvram_init();

	/* Register char device */
	ret = register_chrdev(NVRAM_MAJOR, MTD_NVRAM_NAME, &dev_nvram_fops);
	if (ret < 0) {
		printk(KERN_ERR "NVRAM: unable to register character device\n");
		goto err;
	}
	
	nvram_major = NVRAM_MAJOR;
	
	g_pdentry = create_proc_read_entry(MTD_NVRAM_NAME, 0444, NULL, nvram_proc_version_read, NULL);
	if (!g_pdentry) {
		ret  = -ENOMEM;
		goto err;
	}

	g_pdentry->owner = THIS_MODULE;
	
	printk("ASUS NVRAM: initialized\n");

	return 0;

err:
	dev_nvram_exit();
	return ret;
}

module_init(dev_nvram_init);
module_exit(dev_nvram_exit);
MODULE_LICENSE("GPL");
MODULE_VERSION("V0.02");

