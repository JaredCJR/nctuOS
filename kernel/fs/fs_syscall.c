/* This file use for NCTU OSDI course */


// It's handel the file system APIs 
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <fs.h>
#include <inc/string.h>
#include <kernel/fs/fat/ff.h>
#include <kernel/mem.h>
#include <kernel/cpu.h>

/*TODO: Lab7, file I/O system call interface.*/
/*Note: Here you need handle the file system call from user.
 *       1. When user open a new file, you can use the fd_new() to alloc a file object(struct fs_fd)
 *       2. When user R/W or seek the file, use the fd_get() to get file object.
 *       3. After get file object call file_* functions into VFS level
 *       4. Update the file objet's position or size when user R/W or seek the file.(You can find the useful marco in ff.h)
 *       5. Remember to use fd_put() to put file object back after user R/W, seek or close the file.
 *       6. Handle the error code, for example, if user call open() but no fd slot can be use, sys_open should return -STATUS_ENOSPC.
 *
 *  Call flow example:
 *        ┌──────────────┐
 *        │     open     │
 *        └──────────────┘
 *               ↓
 *        ╔══════════════╗
 *   ==>  ║   sys_open   ║  file I/O system call interface
 *        ╚══════════════╝
 *               ↓
 *        ┌──────────────┐
 *        │  file_open   │  VFS level file API
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │   fat_open   │  fat level file operator
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │    f_open    │  FAT File System Module
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │    diskio    │  low level file operator
 *        └──────────────┘
 *               ↓
 *        ┌──────────────┐
 *        │     disk     │  simple ATA disk dirver
 *        └──────────────┘
 */

extern struct fs_fd fd_table[FS_FD_MAX];
static int check_valid_fd(struct fs_fd *fd) {
    bool is_valid = false;
    int i;
    for (i = 0; i < FS_FD_MAX; ++i)
        if (fd == &fd_table[i]) {
            is_valid = true;
            break;
        }
    return is_valid;
}


// Below is POSIX like I/O system call 
int sys_open(const char *file, int flags, int mode)
{
    //We dont care the mode.
/* TODO */
	int fd;
    int findFlag = false;
    struct fs_fd *fd_file = NULL;
	extern struct fs_fd fd_table[FS_FD_MAX];

    for(fd = 0; fd < FS_FD_MAX; fd++)
	{
        fd_file = &fd_table[fd];
        if(!strcmp(fd_file->path, file))
		{
            findFlag = true;
            break;
        }   
    }   

    if(findFlag == false)
	{
	    fd = fd_new();
		if(fd == -1)
		{
			return -1;
		}
        fd_file = fd_get(fd);
		fd_put(fd_file);
        strcpy(fd_file->path,file);	
    }else
	{
		fd_file = fd_get(fd);
		if(fd_file->ref_count > FS_FD_MAX)
		{
			return -1;
		}
	}
	//printk("[SYS OPEN] fd=%d, file=%s\n",fd,file);
    int ret = file_open(fd_file, file, flags);

    FIL *object = fd_file->data; 
    size_t size = object->obj.objsize;
    fd_file->size = size;  

    if (ret < 0)
    {
        sys_close(fd);
        return ret;         
    }
    return fd;
}

int sys_close(int fd)
{
/* TODO */
    if (fd >= FS_FD_MAX)
	{
        return -STATUS_EINVAL;
	}
    struct fs_fd* fd_file = fd_get(fd);
	//printk("[SYS CLOSE] fd=%d\n",fd);

    //clear size and pos
    fd_file->size = 0;
    fd_file->pos = 0;
    memset(fd_file->path,0,sizeof(fd_file->path));

    int ret = file_close(fd_file);
   	fd_put(fd_file);//for previous get
   	fd_put(fd_file);//for sys_open
    return ret;
}

int sys_read(int fd, void *buf, size_t len)
{
/* TODO */
    if (len < 0 || buf == NULL)
	{
        return -STATUS_EINVAL;
	}
    if (fd >= FS_FD_MAX)
	{
        return -STATUS_EBADF;
	}
    struct fs_fd* fd_file;
    fd_file = fd_get(fd);

    struct PageInfo* page = page_lookup(thiscpu->cpu_task->pgdir, buf, NULL);
    if (page == NULL)
        return -STATUS_EINVAL;

	if (!check_valid_fd(fd_file))
	{
		printk("[SYS READ] fd invalid\n");
        return -STATUS_EBADF;
	}

	//printk("[SYS READ] Start fd pos = %d, size = %d\n", fd_file->pos, fd_file->size);
	//printk("[SYS READ] count= %d \n", len);

    int count = 0;
    if (fd_file->pos + len > fd_file->size)
	{
        count = fd_file->size - fd_file->pos;
	}else
	{
        count = len;
	}

	//printk("[SYS READ] correced count = %d\n", count);
    int ret = file_read(fd_file, buf, count);
	//printk("[SYS READ] ret = %d, %s\n", ret, buf);
    fd_put(fd_file);
    return ret;
}

int sys_write(int fd, const void *buf, size_t len)
{
/* TODO */
    if (len < 0 || buf == NULL)
	{
        return -STATUS_EINVAL;
	}
    if (fd >= FS_FD_MAX)
	{
        return -STATUS_EBADF;
	}
    struct fs_fd* fd_file;
    fd_file = fd_get(fd);

    struct PageInfo* page = page_lookup(thiscpu->cpu_task->pgdir, buf, NULL);
    if (page == NULL)
        return -STATUS_EINVAL;

	//printk("[SYS WRITE] Start pos = %d, size = %d\n", fd_file->pos, fd_file->size);
    int ret = file_write(fd_file, buf, len);
	//printk("[SYS WRITE] ret = %d, %s\n", ret, buf);
    fd_put(fd_file);

    FIL *object;
    object = fd_file->data;
    size_t size = object->obj.objsize;
    fd_file->size = size;

    return ret;
}

/* Note: Check the whence parameter and calcuate the new offset value before do file_seek() */
off_t sys_lseek(int fd, off_t offset, int whence)
{
/* TODO */
    if (fd >= FS_FD_MAX || offset < 0 || whence < 0)
	{
        return -STATUS_EINVAL;
	}
    struct fs_fd* fd_file = fd_get(fd);

    off_t new_offset;
    if (whence == SEEK_SET) 
	{	
		new_offset = offset;
	}else if (whence == SEEK_CUR)
	{
		new_offset = fd_file->pos + offset;
	}else if (whence == SEEK_END) 
	{
		new_offset = fd_file->size + offset;
	}
	fd_file->pos = new_offset;

	//printk("[SYS LSEEK] Start offset = %d, size = %d\n", offset, whence);
    int ret = file_lseek(fd_file, new_offset);
	//printk("[SYS LSEEK] result = %d\n", ret);
    fd_put(fd_file);
    return ret;
}

int sys_unlink(const char *pathname)
{
/* TODO */
	int ret = file_unlink(pathname);
	return ret;
}

int sys_ls(const char *pathname)
{
    DIR dir;
    FILINFO fno;
    int res;
    if(f_opendir(&dir, pathname) != 0)
	{
		printk("path not exist\n");
		return 0;
	}
    res = f_readdir(&dir, &fno);
    while (strlen(fno.fname)) {
		if(fno.fattrib == 32)
		{
        	printk("filename = %s, size = %d, type = FILE\n", fno.fname, fno.fsize);
		}else
		{
        	printk("filename = %s, fsize = %d, type = %d\n", fno.fname, fno.fsize, fno.fattrib);
		}
        res = f_readdir(&dir, &fno);
    }   
    f_closedir(&dir);
    return 0;
}
              
int sys_rm(const char *pathname)
{
	int ret = file_unlink(pathname);
	if(ret != 0)
	{
		printk("Cannot remove %s\n",pathname);
	}
	return ret;
}

int sys_touch(const char *pathname)
{
	FIL fil;
	//create new file
	if(f_open(&fil, pathname, FA_CREATE_ALWAYS | FA_WRITE) != 0)
	{
		printk("create new file failed\n");
	}
	f_close(&fil);
    return 0;
}
