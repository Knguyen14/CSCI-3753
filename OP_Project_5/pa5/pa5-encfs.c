/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  Minor modifications and note by Andy Sayler (2012) <www.andysayler.com>

  Source: fuse-2.8.7.tar.gz examples directory
  http://sourceforge.net/projects/fuse/files/fuse-2.X/

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall `pkg-config fuse --cflags` fusexmp.c -o fusexmp `pkg-config fuse --libs`

  Note: This implementation is largely stateless and does not maintain
        open file handels between open and release calls (fi->fh).
        Instead, files are opened and closed as necessary inside read(), write(),
        etc calls. As such, the functions that rely on maintaining file handles are
        not implmented (fgetattr(), etc). Those seeking a more efficient and
        more complete implementation may wish to add fi->fh support to minimize
        open() and close() calls and support fh dependent functions.

*/

#define FUSE_USE_VERSION 28
#define ENCRYPT 1
#define DECRYPT 0
#define IGNORE (-1)

#define HAVE_SETXATTR

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <limits.h>
#include <stdlib.h>
#include "aes-crypt.h"

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

// Root path of the fs
char* ROOT_PATH="";
char* PASSWORD="";

char* get_path(const char* path){
    int len = strlen(ROOT_PATH) + strlen(path) + 1;

    char* full_path = malloc(len * sizeof(char));

    strcpy(full_path, ROOT_PATH);
    strcat(full_path, path);
    return full_path;
}

int get_isenc_xattr(const char* path){
    char xattr[5];
    getxattr(path, "user.pa5-encfs.encrypted", xattr, sizeof(char)*5);
    fprintf(stdout, "xattr set as %s", xattr);

    // Return true if xattr is "true"
    return strcmp("true", xattr) == 0;

}


static int xmp_getattr(const char *short_path, struct stat *stbuf)
{
    char* path = get_path(short_path);

	int res;

	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_access(const char *short_path, int mask)
{
    char* path = get_path(short_path);
	int res;

	res = access(path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_readlink(const char *short_path, char *buf, size_t size)
{
    char* path = get_path(short_path);
	int res;

	res = readlink(path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}


static int xmp_readdir(const char *short_path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
    char* path = get_path(short_path);
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	dp = opendir(path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int xmp_mknod(const char *short_path, mode_t mode, dev_t rdev)
{
    char* path = get_path(short_path);
	int res;

	/* On Linux this could just be 'mknod(path, mode, rdev)' but this
	   is more portable */
	if (S_ISREG(mode)) {
		res = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = close(res);
	} else if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_mkdir(const char *short_path, mode_t mode)
{
    char* path = get_path(short_path);
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_unlink(const char *short_path)
{
    char* path = get_path(short_path);
	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rmdir(const char *short_path)
{
    char* path = get_path(short_path);
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
	int res;

	res = symlink(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rename(const char *from, const char *to)
{
	int res;

	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_link(const char *from, const char *to)
{
	int res;

	res = link(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chmod(const char *short_path, mode_t mode)
{
    char* path = get_path(short_path);
	int res;

	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chown(const char *short_path, uid_t uid, gid_t gid)
{
    char* path = get_path(short_path);
	int res;

	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_truncate(const char *short_path, off_t size)
{
    char* path = get_path(short_path);
	int res;

	res = truncate(path, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_utimens(const char *short_path, const struct timespec ts[2])
{
    char* path = get_path(short_path);
	int res;
	struct timeval tv[2];

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	res = utimes(path, tv);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_open(const char *short_path, struct fuse_file_info *fi)
{
    char* path = get_path(short_path);
	int res;

	res = open(path, fi->flags);
	if (res == -1) return -errno;

	close(res);
	return 0;
}

static inline int f_size(FILE *file){
    struct stat stats;

    int has_err= fstat(fileno(file), &stats);
    if(has_err){
        return -1;
    }
    return stats.st_size;
}

static int xmp_read(const char *short_path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
    FILE *f_ptr, *tmp_ptr;
    char* path = get_path(short_path);
	int res;

	// fd = open(path, O_RDONLY);
    // Open file and create temp file for intermediary
    f_ptr = fopen(path, "r");
    tmp_ptr = tmpfile();

    // Only decrypt if encrypted
    int action = IGNORE;
    if (get_isenc_xattr(path))
        action = DECRYPT;

    int crypt_err = do_crypt(f_ptr, tmp_ptr, action, PASSWORD);
    if (crypt_err == 0 || f_ptr == NULL || tmp_ptr == NULL)
        return -errno;


    /* Clean memory, etc. of tmp */
    fflush(tmp_ptr);
    fseek(tmp_ptr, offset, SEEK_SET);

	res = fread(buf, 1, f_size(tmp_ptr), tmp_ptr);
	if (res == -1)
		res = -errno;

    fclose(tmp_ptr);
    fclose(f_ptr);

	return res;
}

static int xmp_write(const char *short_path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
    FILE *f_ptr, *tmp_ptr;
    char* path = get_path(short_path);
	//int fd;
	int res;

	(void) fi;
    f_ptr = fopen(path, "r+");
    tmp_ptr = tmpfile();
    int tmp_num = fileno(tmp_ptr);


    // Decrypt
    int action = IGNORE;

    if(xmp_access(short_path, R_OK) == 0 && f_size(f_ptr) > 0){
        if (get_isenc_xattr(path))
            action = DECRYPT;
        int crypt_err = do_crypt(f_ptr, tmp_ptr, action, PASSWORD);
        if (crypt_err == 0) return errno;

        // Reset files to beginning
        rewind(f_ptr);
        rewind(tmp_ptr);
    }


	res = pwrite(tmp_num, buf, size, offset);
	if (res == -1)
		res = -errno;

    // Now encrypt the file again

    action = IGNORE;
    if (get_isenc_xattr(path))
        action = ENCRYPT;

    int crypt_err = do_crypt(tmp_ptr, f_ptr, action, PASSWORD);
    if(crypt_err == 0) return errno;

    fclose(f_ptr);
    fclose(tmp_ptr);

	return res;
}

static int xmp_statfs(const char *short_path, struct statvfs *stbuf)
{
    char* path = get_path(short_path);
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_create(const char* short_path, mode_t mode, struct fuse_file_info* fi) {

    char* path = get_path(short_path);
    (void) fi;

    int res;
    res = creat(path, mode);
    if(res == -1)
	return -errno;

    close(res);

    // Set encrypted attribute 0 = no flags
    setxattr(path, "user.pa5-encfs.encrypted", "true", (sizeof(char) * 5), 0);

    return 0;
}


static int xmp_release(const char *short_path, struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */
    char* path = get_path(short_path);

	(void) path;
	(void) fi;
	return 0;
}

static int xmp_fsync(const char *short_path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/* Just a stub.	 This method is optional and can safely be left
	   unimplemented */
    char* path = get_path(short_path);

	(void) path;
	(void) isdatasync;
	(void) fi;
	return 0;
}

#ifdef HAVE_SETXATTR
static int xmp_setxattr(const char *short_path, const char *name, const char *value,
			size_t size, int flags)
{
    char* path = get_path(short_path);
	int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_getxattr(const char *short_path, const char *name, char *value,
			size_t size)
{
    char* path = get_path(short_path);
	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(const char *short_path, char *list, size_t size)
{
    char* path = get_path(short_path);
	int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(const char *short_path, const char *name)
{
    char* path = get_path(short_path);
	int res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

static struct fuse_operations xmp_oper = {
	.getattr	= xmp_getattr,
	.access		= xmp_access,
	.readlink	= xmp_readlink,
	.readdir	= xmp_readdir,
	.mknod		= xmp_mknod,
	.mkdir		= xmp_mkdir,
	.symlink	= xmp_symlink,
	.unlink		= xmp_unlink,
	.rmdir		= xmp_rmdir,
	.rename		= xmp_rename,
	.link		= xmp_link,
	.chmod		= xmp_chmod,
	.chown		= xmp_chown,
	.truncate	= xmp_truncate,
	.utimens	= xmp_utimens,
	.open		= xmp_open,
	.read		= xmp_read,
	.write		= xmp_write,
	.statfs		= xmp_statfs,
	.create         = xmp_create,
	.release	= xmp_release,
	.fsync		= xmp_fsync,
#ifdef HAVE_SETXATTR
	.setxattr	= xmp_setxattr,
	.getxattr	= xmp_getxattr,
	.listxattr	= xmp_listxattr,
	.removexattr	= xmp_removexattr,
#endif
};

int main(int argc, char *argv[])
{
	umask(0);

    // set global root
    ROOT_PATH = realpath(argv[argc - 2], NULL);
    if(ROOT_PATH == NULL){
	printf("Please provide a valid mirror dir");
	return EXIT_FAILURE;
    }

    // Set password (first arg)
    PASSWORD = argv[argc-3];

    // Shift args
    argv[argc-3] = argv[argc-1];
    argv[argc-1] = NULL;
    argc = argc - 2;

	return fuse_main(argc, argv, &xmp_oper, NULL);
}