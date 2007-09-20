/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/* This is the System Call Tester program. It tests supported system calls
	in a fairly robust way and spits out the output in a perl readable format.
	The tests were designed with incremental testing in mind. For instance,
	simple file operations are checked before complicated ones and so on.

	Whenever you add a system call please place a test for it in here.

	Make sure to compile it with the correct define flags: e.g.,
	-DIRIX
	-DSolaris
	-DLINUX
	-DOSF1

	-pete

	Phase 1 is the systematic testing of the return values from the calls.
	Phase 2 is testing to see whether or not the system call actually worked.
	Phase 3 is whether or not the block test worked.
	Phase 4 is whether or not the entire syscall test failed.

*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <utime.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <stdarg.h>

#if defined(LINUX)
#include <sys/uio.h>
#include <sys/vfs.h>
#endif

#if defined(OSF1)
#include <sys/mount.h>
#include <sys/uio.h>
#endif

#if defined(IRIX) || defined(Solaris)
#include <sys/statfs.h>
#endif

#if defined(HPUX)
#include <sys/vfs.h>
#include <nfs/nfs.h>
#endif

/* These are here because compilers are stupid... */
#if defined (Solaris251)
#ifdef __cplusplus
extern "C" int getrusage(int who, struct rusage *rusage);
extern "C" int gethostname(char *name, size_t len);
extern "C" int utimes(char *filename, struct timeval *tvp);
extern "C" int setregid(gid_t rgid, gid_t egid);
extern "C" int setreuid(uid_t ruid, uid_t euid);
#else
int getrusage(int who, struct rusage *rusage);
int gethostname(char *name, size_t len);
int utimes(char *filename, struct timeval *tvp);
int setregid(gid_t rgid, gid_t egid);
int setreuid(uid_t ruid, uid_t euid);
#endif
#endif

#if defined(Solaris26)
#ifdef __cplusplus
extern "C" int utimes(char *filename, struct timeval *tvp);
#else
int utimes(char *filename, struct timeval *tvp);
#endif
#endif

/* for some reason, g++ needs this extern definition */
#if defined(OSF1) && defined(__cplusplus) && defined(__GNUC__)
extern "C" int fchdir(int);
extern "C" int getdomainname(char *name, size_t len);
#endif
#if defined(DUX4) && defined(__cplusplus) && defined(__GNUC__)
/* these are only a problem on dux4, it appears */
extern "C" int statfs(char *, struct statfs *);
extern "C" int fstatfs(int, struct statfs *);
extern "C" int mknod(const char *, mode_t, dev_t );
#endif

#if defined(LINUX) && defined(GLIBC)
#define getpriority __hide_getpriority
#define setpriority __hide_setpriority
#define getrlimit __hide_getrlimit
#define __getrlimit __hide__getrlimit
#define setrlimit __hide_setrlimit
#define getrusage __hide_getrusage
#define __getrusage __hide___getrusage
#endif

#include <sys/resource.h>

#if defined(LINUX) && defined(GLIBC)
#undef getpriority
#undef setpriority
#undef getrlimit
#undef __getrlimit
#undef setrlimit
#undef getrusage
#undef __getrusage
#ifdef __cplusplus
extern "C" int getrlimit(int, struct rlimit *);
extern "C" int __getrlimit(int, struct rlimit *);
extern "C" int setrlimit(int, const struct rlimit *);
extern "C" int getpriority(int, int);
extern "C" int setpriority(int, int, int);
extern "C" int getrusage(int, struct rusage * );
extern "C" int __getrusage(int, struct rusage * );
#else
int getrlimit(int, struct rlimit *);
int __getrlimit(int, struct rlimit *);
int setrlimit(int, const struct rlimit *);
int getpriority(int, int);
int setpriority(int, int, int);
int getrusage(int, struct rusage * );
int __getrusage(int, struct rusage * );
#endif
#endif


/* this MUST be an int quantity. It is used for list termination on 
	is_errno_valid(). */
const int ENDLIST = -1;

/* How big certain buffers should be for names and things */
/* XXX I should really use OS defined things for this, but I'm lazy */
#define NAMEBUF  8192

/* How did the block tests do? DO NOT CHANGE THESE! I use them based upon
	how the OS returns a success or failure. */
#define SUCCESS 0
#define FAILURE -1
#define UNDEFINED -2

/* a really generic way of spitting out analysis that isn't the important one
	in the test, used in the block test code.  Also, it is marked as
	comparing against SUCCESS, which is what the specific test should always
	do. The expect_* call ensures that you should always get what your
	expectation resulted in. If you expect your test to fail, and it does,
	then the expected result is a success. :) */
#define EXPECTED_RESP \
	if (passed != SUCCESS) { \
		printf("\tFailed Phase 2: This call did something unexpected.\n"); \
		fflush(NULL); \
		block = FAILURE; \
	}

/* Some helpers for fast aborts of a block test when it blows up */
#define IF_FAILED \
if (passed == FAILURE)
	
#define ABORT_TEST \
{ \
	printf("\tFail Phase 2: Aborting test because of catastrophic failure\n"); \
	fflush(NULL); \
	return passed; \
}
	

/* a safe string modifer for printf, if it is null, print out that fact */
#define STR(x) \
	(((x)==NULL)?"(null)":(x))

/* do not change this passage, the test program has hard coded values that
	assume the length of the passage is what you see here */
char passage[] = "This is tedious and lonely code. There is no salvation "
					"in writing this code.";

/* Print a spacer at the debug level */
void testbreak(void)
{
	int i;

	for (i = 0; i < 3; i++) {
		printf("-");
	}
	printf("\n");
}

/* a simple utility routine */
void *xmalloc(size_t len)
{
	void *vec = NULL;

	vec = malloc(len);
	if (vec == NULL) {
		printf("Out of Memory. Exiting.\n");
		fflush(NULL);
		exit(EXIT_FAILURE);
	}
	memset(vec, 0, len);
	return vec;
}

/* a simple wrapper around tmpnam */
char *xtmpnam(char *space)
{
	char *buf = NULL;
	errno = 0;
	if ((buf = tmpnam(space)) == NULL) {
		printf("Could not determine unique file name.(%s)\n", strerror(errno));
		fflush(NULL);
		exit(EXIT_FAILURE);
	}
	return buf;
}

/* These few calls are to translate what the OS tells us about certain call
	into something more meaningful, like FAILURE, SUCCESS, or UNDEFINED */

/* If result is 0 it is success, if it is -1 it is an error, anything else
	is undefined */
int handle_zng(int ret)
{
	switch(ret) {
		case -1:
			return FAILURE;
			break;
		case 0:
			return SUCCESS;
			break;
		default:
			return UNDEFINED;
			break;
	}
	return UNDEFINED;
}

/* return success on a non null pointer */
int handle_ptr(void *ret)
{
	if (ret == NULL) {
		return FAILURE;
	}

	return SUCCESS;
}

/* return a success if ret is >= 0, failure on -1, and undefined for ret < -1 */
int handle_gez(int ret)
{
	if (ret >= 0) {
		return SUCCESS;
	}

	if (ret == -1)
	{
		return FAILURE;
	}

	return UNDEFINED;
}

/* return a success if the off_t value is NOT (off_t)-1 */
int handle_off(off_t ret)
{
	if (ret == (off_t)-1)
	{
		return FAILURE;
	}

	return SUCCESS;
}

/* return a success if the ret isn't an EOF and it is a zero */
int handle_eof(int ret)
{
	switch(ret) {
		case EOF:
			return FAILURE;
			break;
		case 0:
			return SUCCESS;
			break;
		default:
			return UNDEFINED;
			break;
	}

	return UNDEFINED;
}

/* handle a long type given that ret >= zero is success */
int handle_lng(long ret)
{
	if (ret >= 0) {
		return SUCCESS;
	}

	if (ret == -1) {
		return FAILURE;
	}

	if (ret < -1) {
		return UNDEFINED;
	}

	return UNDEFINED;
}


/* The tests of the calls */

/* this makes sure that I read the number of bytes I ask for */
int full_read(int fd, char *buf, int size)
{
	int	 bytes_read;
	int	 this_read;

	bytes_read = 0;
	do {
		this_read = read(fd, buf, size - bytes_read);
		if (this_read < 0) {
			return this_read;
		} else if (this_read == 0) { /* end of file marker */
			return bytes_read;
		}
		bytes_read += this_read;
		buf += this_read;
	} while (bytes_read < size);
	return bytes_read;
}

/* this makes sure I write the number of bytes I ask for */
int full_write(int fd, char *buf, int size)
{
	int	 bytes_write;
	int	 this_write;

	bytes_write = 0;
	do {
		this_write = write(fd, buf, size - bytes_write);
		if (this_write < 0) {
			return this_write;
		}
		bytes_write += this_write;
		buf += this_write;
	} while (bytes_write < size);
	return bytes_write;
}

/* this makes sure that I read the number of bytes I ask for */
int full_fread(void *buf, size_t size, size_t nmemb, FILE *fp)
{
	int items_read = 0;
	int this_read = 0;
	
	do {
		this_read = fread(buf, size, nmemb - items_read, fp);
		if (this_read < 0) {
			/* The caller should catch this */
			return this_read;
		}
		if (this_read == 0 && (feof(fp) || ferror(fp))) {
			return items_read;
		}

		items_read += this_read;

	} while(items_read < nmemb);

	return items_read;
}


/* this makes sure I write the number of bytes I ask for */
int full_fwrite(char *buf, size_t size, size_t nmemb, FILE *fp)
{
	int items_wrote = 0;
	int this_write = 0;
	
	do {
		this_write = fwrite(buf, size, nmemb - items_wrote, fp);
		if (this_write < 0) {
			/* Caller should catch this, this value is undefined for fwrite */
			return this_write;
		}
		if (this_write == 0 && ferror(fp)) {
			return this_write;
		}

		items_wrote += this_write;

	} while(items_wrote < nmemb);

	return items_wrote;
}

/* This will check to see if the errno is of the set that you pass in a 
	comma delimited format. You MUST use ENDLIST to dictate the end of list.
	This call only makes sense in the event of a failure by the system
	call. As far as I know, ENDLIST isn't a valid errno number. */
	
void is_errno_valid(int err, ...)
{
	int eval;
	int found = 0;
	va_list ap;

	/* XXX errno needs to be ceaderified in order to be supported. So for now,
		just return all the time. */
	return;

	va_start(ap, err);

	/* consume all of the valid errno values for this architecture, this could
		run off the end of the stack if you do not give a ENDLIST at the end
		of the call list. */
	while(1) {
		eval = va_arg(ap, int);
		if (eval == ENDLIST) {
			/* didn't find it */
			break;
		}
		if (err == eval) {
			found = 1;
			break;
		}
	}

	va_end(ap);

	if (found == 0) {
		printf("\tFailed: OS returned errno(%s) that is not in the valid "
			"set for function!\n", strerror(err));
	}
}

FILE* freopen_test(char *file, char *type, FILE *stream)
{
	FILE *newfp = NULL;
	int save_errno;
	int passed;

	printf("freopen(): file=%s, type=%s, stream=0x%x\n", STR(file), STR(type),
		stream);
	fflush(NULL);

	passed = handle_ptr(newfp = freopen(file, type, stream));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			printf("\tFailed Phase 1: returned 0x%x, expected 0x%x\n",
				newfp, stream);
			fflush(NULL);
			break;
		case SUCCESS:
			printf("\t\tnewfp =  0x%x\n", newfp);
			fflush(NULL);

			if (newfp != stream) {
				printf("\tFailed Phase 1: returned 0x%x, expected 0x%x\n",
					newfp, stream);
				fflush(NULL);
			} else {
				printf("\tSucceeded Phase 1\n");
				fflush(NULL);
			}
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: undefined return code 0x%x\n",
				newfp);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}


	errno = save_errno;
	return newfp;
}

int access_test(char *file, int mode)
{
	int ret, save_errno;
	int passed;

	printf("access(): file=%s, mode=0x%x(0%o)\n", STR(file), mode, mode);
	fflush(NULL);

	passed = handle_zng(ret = access(file, mode));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
		#if defined(Solaris26)
			is_errno_valid(save_errno, EACCES, EFAULT, EINTR, ELOOP, EMULTIHOP, 
				ENAMETOOLONG, ENOENT, ENOLINK, ENOTDIR, EROFS, EINVAL, 
				ETXTBSY, ENDLIST);
		#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:	
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
			
	}

	errno = save_errno;
	return ret;
}

int chmod_test(char *file, mode_t mode)
{
	int ret, save_errno;
	int passed;

	printf("chmod(): file=%s, mode=0x%x(0%o)\n", STR(file), mode, mode);
	fflush(NULL);

	passed = handle_zng(ret = chmod(file, mode));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EFAULT, EINTR, EIO, ELOOP,
					EMULTIHOP, ENAMETOOLONG, ENOENT, ENOLINK, ENOTDIR, EPERM,
					EROFS, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int chown_test(char *file, uid_t owner, gid_t group)
{
	int ret, save_errno;
	int passed;

	printf("chown(): file=%s, owner=%u, group=%u\n", STR(file), owner, group);
	fflush(NULL);

	passed = handle_zng(ret = chown(file, owner, group));
	save_errno = errno;

	switch(ret) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EFAULT, EINTR, EINVAL, EIO,
					ELOOP, EMULTIHOP, ENAMETOOLONG, ENOLINK, ENOENT, ENOTDIR,
					EPERM, EROFS, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
				printf("\t\tret = %d\n", ret);
				printf("\tSucceeded Phase 1\n");
				fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);	
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}	

int chdir_test(char *dir)
{
	int ret, save_errno;
	int passed;

	printf("chdir(): dir=%s\n", STR(dir));
	fflush(NULL);

	passed = handle_zng(ret = chdir(dir));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EFAULT, EINTR, EIO, ELOOP,
					ENAMETOOLONG, ENOENT, ENOLINK, ENOTDIR, EMULTIHOP, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int close_test(int fd)
{
	int ret, save_errno;
	int passed;

	printf("close(): fd=%d\n", fd); 
	fflush(NULL);

	passed = handle_zng(ret = close(fd));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EBADF, EINTR, ENOLINK, ENOSPC, EIO, 
					ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* Good return values */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int creat_test(const char *path, mode_t mode)
{
	int fd;
	int save_errno;
	int passed;

	printf("creat(): file=%s, mode=0x%x(0%o)\n", STR(path), mode, mode);
	fflush(NULL);

	passed = handle_gez(fd = creat(path, mode));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EAGAIN, EDQUOT, EFAULT, 
					EINTR, EISDIR, ELOOP, EMFILE, EMULTIHOP, ENAMETOOLONG,
					ENFILE, ENOENT, ENOLINK, ENOSPC, ENOTDIR, EOVERFLOW, EROFS,
					ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			printf("\t\tfd = %d\n", fd);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"fd = %d\n", fd);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return fd;
}

int sync_test(void)
{
	printf("sync():\n"); 
	fflush(NULL);

	sync();

	/* sync always returns zero if it returns an int, otherwise it returns
		a void quantity, which for our purposes we can view as zero */

	printf("\t\tret = %d\n", 0);
	printf("\tSucceeded Phase 1\n");
	fflush(NULL);

	return 0;
}

int dup_test(int fd)
{
	int newfd;
	int save_errno;
	int passed;

	printf("dup():fd=%d\n", fd);
	fflush(NULL);

	passed = handle_gez(newfd = dup(fd));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EBADF, EINTR, EMFILE, ENOLINK, 
					ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			printf("\t\tnewfd = %d\n", newfd);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"newfd = %d\n",newfd);
			fflush(NULL);
		break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return newfd;
}

int fchdir_test(int fd)
{
	int ret, save_errno;
	int passed;

	printf("fchdir(): fd=%d\n", fd);
	fflush(NULL);

	passed = handle_zng(ret = fchdir(fd));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EBADF, EINTR, EIO, ENOLINK,
					ENOTDIR, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int fchmod_test(int fd, mode_t mode)
{
	int ret, save_errno;
	int passed;

	printf("fchmod(): fd=%d, mode=0x%x(0%o)\n", fd, mode, mode);
	fflush(NULL);

	passed = handle_zng(ret = fchmod(fd, mode));
	save_errno = errno;

	switch(passed)
	{
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EBADF, EIO, EINTR, ENOLINK, EPERM,
					EROFS, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int fchown_test(int fd, uid_t owner, gid_t group)
{
	int ret, save_errno;
	int passed;

	printf("fchown(): fd=%d, uid=%u, gid=%u\n", fd, owner, group);
	fflush(NULL);

	passed = handle_zng(ret = fchown(fd, owner, group));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EBADF, EIO, EINTR, ENOLINK, EINVAL,
					EPERM, EROFS, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno; 
	return ret;
}

/* only test the ones we support */
int fcntl_test(int fd, int cmd, ...)
{
	int ret, save_errno;
	va_list ap;
	int arg;
	struct flock *flp = NULL;

	/* fcntl is really special, don't use the 'passed' construct here */

	switch(cmd) {
		/* these have one integer argument */
		#ifdef F_GETFD
		case F_GETFD:
			va_start(ap, cmd);
			arg = va_arg(ap, int);
			va_end(ap);
			
			printf("fcntl(): fd=%d, cmd=F_GETFD, arg=0x%x\n", fd, arg);
			fflush(NULL);

			ret = fcntl(fd, cmd, arg);
			save_errno = errno;
			if (ret < 0) {
				#if defined(Solaris26)
					is_errno_valid(save_errno, EAGAIN, EBADF, EFAULT, EINTR, 
						EINVAL, EMFILE, ENOLCK, ENOLINK, EOVERFLOW, EDEADLK,
						ENDLIST);
				#endif
			} 

			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);

			errno = save_errno;
			return ret;
			break;
		#endif

		#ifdef F_GETFL
		case F_GETFL:
			va_start(ap, cmd);
			arg = va_arg(ap, int);
			va_end(ap);

			printf("fcntl(): fd=%d, cmd=F_GETFL, arg=0x%x\n", fd, arg);
			fflush(NULL);

			ret = fcntl(fd, cmd, arg);
			save_errno = errno;
			if (ret < 0) {
				#if defined(Solaris26)
					is_errno_valid(save_errno, EAGAIN, EBADF, EFAULT, EINTR, 
						EINVAL, EMFILE, ENOLCK, ENOLINK, EOVERFLOW, EDEADLK,
						ENDLIST);
				#endif
			}

			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);

			errno = save_errno;
			return ret;
			break;
		#endif

		#ifdef F_SETFD
		case F_SETFD:
			va_start(ap, cmd);
			arg = va_arg(ap, int);
			va_end(ap);

			printf("fcntl(): fd=%d, cmd=F_SETFD, arg=0x%x\n", fd, arg);
			fflush(NULL);

			ret = fcntl(fd, cmd, arg);
			save_errno = errno;
			if (ret == -1) {
				#if defined(Solaris26)
					is_errno_valid(save_errno, EAGAIN, EBADF, EFAULT, EINTR, 
						EINVAL, EMFILE, ENOLCK, ENOLINK, EOVERFLOW, EDEADLK,
						ENDLIST);
				#endif
			} 

			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);

			errno = save_errno;
			return ret;
			break;
		#endif

		#ifdef F_SETFL
		case F_SETFL:
			va_start(ap, cmd);
			arg = va_arg(ap, int);
			va_end(ap);

			printf("fcntl(): fd=%d, cmd=F_SETFL, arg=0x%x\n", fd, arg);
			fflush(NULL);

			ret = fcntl(fd, cmd, arg);
			save_errno = errno;
			if (ret == -1) {
				#if defined(Solaris26)
					is_errno_valid(save_errno, EAGAIN, EBADF, EFAULT, EINTR, 
						EINVAL, EMFILE, ENOLCK, ENOLINK, EOVERFLOW, EDEADLK,
						ENDLIST);
				#endif
			}

			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);

			errno = save_errno;
			return ret;
			break;
		#endif

		#ifdef F_DUPFD
		case F_DUPFD:
			va_start(ap, cmd);
			arg = va_arg(ap, int);
			va_end(ap);

			printf("fcntl(): fd=%d, cmd=F_DUPFD, arg=0x%x\n", fd, arg);
			fflush(NULL);

			ret = fcntl(fd, cmd, arg);
			save_errno = errno;
			if (ret < 0) {
				#if defined(Solaris26)
					is_errno_valid(save_errno, EAGAIN, EBADF, EFAULT, EINTR, 
						EINVAL, EMFILE, ENOLCK, ENOLINK, EOVERFLOW, EDEADLK,
						ENDLIST);
				#endif

				printf("\t\tret = %d\n", ret);
				printf("\tSucceeded Phase 1\n");
				fflush(NULL);

				errno = save_errno;
				return ret;
			}

			if (ret < arg) {
				printf("\tFailed Phase 1: returned ret less than arg! "
					"ret = %d, arg = %d\n", ret, arg);
				fflush(NULL);

				errno = save_errno;
				return ret;
			} 

			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);

			errno = save_errno;
			return ret;
			break;
		#endif

		#ifdef F_DUP2FD
		case F_DUP2FD:
			va_start(ap, cmd);
			arg = va_arg(ap, int);
			va_end(ap);

			printf("fcntl(): fd=%d, cmd=F_DUP2FD, arg=0x%x\n", fd, arg);
			fflush(NULL);

			ret = fcntl(fd, cmd, arg);
			save_errno = errno;
			if (ret < 0) {
				#if defined(Solaris26)
					is_errno_valid(save_errno, EAGAIN, EBADF, EFAULT, EINTR, 
						EINVAL, EMFILE, ENOLCK, ENOLINK, EOVERFLOW, EDEADLK,
						ENDLIST);
				#endif

				printf("\t\tret = %d\n", ret);
				printf("\tSucceeded Phase 1\n");
				fflush(NULL);

				errno = save_errno;
				return ret;
			}

			if (ret != arg) {
				printf("\tFailed: did not return ret == arg!\n");
				fflush(NULL);
			} else {
				printf("\t\tret = %d\n", ret);
				printf("\tSucceeded Phase 1\n");
				fflush(NULL);
			}

			errno = save_errno;
			return ret;
			break;
		#endif

		#ifdef F_FREESP
		case F_FREESP:
			va_start(ap, cmd);
			flp = va_arg(ap, struct flock*);
			va_end(ap);

			printf("fcntl(): fd=%d, cmd=F_FREESP, flp=0x%x\n", fd, flp);
			printf("\t\tl_type = %d\n", flp->l_type);
			printf("\t\tl_whence = %d\n", flp->l_whence);
			printf("\t\tl_start = %u\n", flp->l_start);
			printf("\t\tl_len = %u\n", flp->l_len);
			printf("\t\tl_sysid = %d\n", flp->l_sysid);
			printf("\t\tl_pid = %u\n", flp->l_pid);
			fflush(NULL);

			ret = fcntl(fd, cmd, flp);
			save_errno = errno;

			switch(ret) {
				case -1:
					#if defined(Solaris26)
						is_errno_valid(save_errno, EAGAIN, EBADF, EFAULT,
							EINTR, EINVAL, EMFILE, ENOLCK, ENOLINK, EOVERFLOW,
							EDEADLK, ENDLIST);
					#endif
					/* FALL THROUGH */
				case 0:
					/* good return value */
					printf("\t\tret = %d\n", ret);
					printf("\tSucceeded Phase 1\n");
					fflush(NULL);
					break;
				default:	
					printf("\tFailed Phase 1: returned undefined value! "
						"ret = %d\n", ret);
					fflush(NULL);
					break;	
			}

			errno = save_errno;
			return ret;
			break;
		#endif	

/* 
		#ifdef F_FREESP64
		case F_FREESP64:
			break;
		#endif	
*/
		default:
			printf("\tFailed Phase 1: Unknown fcntl command, "
				"returning failure with EINVAL\n");
			errno = EINVAL;
			return -1;
	}


	/* never gets here */
	errno = 0;
	return -1;
}

int fstat_test(int fd, struct stat *buf)
{
	int ret, save_errno;
	int passed;

	printf("fstat(): fd=%d, buf=0x%x\n", fd, buf); 
	fflush(NULL);

	passed = handle_zng(ret = fstat(fd, buf));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EOVERFLOW, EBADF, EFAULT, EINTR,
					ENOLINK, ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			/* good return call */
			printf("\t\tst_dev = %u\n", buf->st_dev);
			printf("\t\tst_ino = %u\n", buf->st_ino);
			printf("\t\tst_mode = 0x%x(0%o)\n",
				buf->st_mode,buf->st_mode);
			printf("\t\tst_nlink = %u\n", buf->st_nlink);
			printf("\t\tst_uid = %u\n", buf->st_uid);
			printf("\t\tst_gid = %u\n", buf->st_gid);
			printf("\t\tst_rdev = %u\n", buf->st_rdev);
			printf("\t\tst_size = %u\n", buf->st_size);
			printf("\t\tst_atime = %u\n", buf->st_atime);
			printf("\t\tst_mtime = %u\n", buf->st_mtime);
			printf("\t\tst_ctime = %u\n", buf->st_ctime);
			printf("\t\tst_blksize = %lu\n", buf->st_blksize);
			printf("\t\tst_blocks = %lu\n", buf->st_blocks);
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int fsync_test(int fd)
{
	int ret, save_errno;
	int passed;

	printf("fsync(): fd=%d\n", fd); 
	fflush(NULL);

	passed = handle_zng(ret = fsync(fd));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
			is_errno_valid(save_errno, EBADF, EINTR, EIO, ENOSPC, ETIMEDOUT,
				ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int getgroups_test(int ngroups, gid_t *grouplist)
{
	int ret, save_errno;
	int passed;
	int i;

	printf("getgroups(): ngroups=%d, grouplist=0x%x\n", ngroups, grouplist);
	fflush(NULL);

	passed = handle_gez(ret = getgroups(ngroups, grouplist));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EINVAL, ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			if (ngroups != 0) {
				for (i = 0; i < ret; i++) {
					printf("\t\t\tgrouplist[%i] = %u\n", i, grouplist[i]);
					fflush(NULL);
				}
			}
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}


/* WARNING!
 *
 * This test ONLY tests RLIMIT_CORE now; it used to be able to test more
 * but no longer does because of funky enum types on Linux
 * -Erik, May 18 2001 (A beautiful friday that I should be drinking instead of
 *                     coding on)
 */ 
int getrlimit_test(struct rlimit *rlp)
{
	int ret, save_errno;
	int passed;

	printf("getrlimit(): rlimit=0x%x\n",  rlp);
	fflush(NULL);

	passed = handle_zng(ret = getrlimit(RLIMIT_CORE, rlp));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EFAULT, EINVAL, EPERM, ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			/* good return value */
			printf("\t\trlim_cur = %u\n", rlp->rlim_cur);
			printf("\t\trlim_max = %u\n", rlp->rlim_max);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

/* Any possible thing this call can return is considered valid */
uid_t getuid_test(void)
{
	uid_t uid;
	int save_errno;

	printf("getuid():\n");
	fflush(NULL);

	uid = getuid();
	save_errno = errno;
	printf("\t\tret = %u\n", uid);
	printf("\tSucceeded Phase 1\n");

	errno = save_errno;
	return uid;
}

/* Any possible thing this call can return is considered valid */
gid_t getgid_test(void)
{
	gid_t gid;
	int save_errno;

	printf("getgid():\n");
	fflush(NULL);

	gid = getgid();
	save_errno = errno;
	printf("\t\tgid = %u\n", gid);
	printf("\tSucceeded Phase 1\n");

	errno = save_errno;
	return gid;
}

int rename_test(char *old, char *newf)
{
	int ret, save_errno;
	int passed;

	printf("rename(): old=%s, newf=%s\n", STR(old), STR(newf));
	fflush(NULL);

	passed = handle_zng(ret = rename(old, newf));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EBUSY, EDQUOT, EEXIST,
					EINVAL, EISDIR, ELOOP, ENAMETOOLONG, EMLINK, ENOENT,
					ENOSPC, ENOTDIR, EROFS, EXDEV, EIO, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int statfs_test(char *path, struct statfs *buf)
{
	int ret, save_errno;
	int passed;

	printf("statfs(): path=%s\n", STR(path)); 
	fflush(NULL);

#if defined(IRIX) || defined(Solaris)
	passed = handle_zng(ret = statfs(path, buf, sizeof(struct statfs), 0));
#else
	passed = handle_zng(ret = statfs(path, buf));
#endif
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, ENOTDIR, ENAMETOOLONG, ENOENT,
					EACCES, ELOOP, EFAULT, EIO, ENOMEM, ENOSYS, ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			/* good return value */
			printf(	"\t\tPath: %s\n"
				"\t\tFree Blocks(User): %ld\n"
				"\t\tFree Blocks: %ld\n"
				"\t\tTotal Blocks: %ld\n"
				"\t\tBlock Size: %ld\n"
				"\t\tFree Inodes: %ld\n"
				"\t\tTotal Inodes: %ld\n",
				STR(path),
				#if defined(Solaris) || defined(IRIX)
					buf->f_bfree,
				#else
					buf->f_bavail,
				#endif
				buf->f_bfree,
				buf->f_blocks,
				buf->f_bsize,
				buf->f_ffree,
				buf->f_files);
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int fstatfs_test(int fd, struct statfs *buf)
{
	int ret, save_errno;
	int passed;

	printf("fstatfs(): fd=%d\n", fd); 
	fflush(NULL);

#if defined(IRIX) || defined(Solaris)
	passed = handle_zng(ret = fstatfs(fd, buf, sizeof(struct statfs), 0));
#else
	passed = handle_zng(ret = fstatfs(fd, buf));
#endif
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EBADF, EFAULT, EIO, ENOSYS, ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			/* good return value */
			printf(	"\t\tFd: %d\n"
					"\t\tFree Blocks(User): %ld\n"
					"\t\tFree Blocks: %ld\n"
					"\t\tTotal Blocks: %ld\n"
					"\t\tBlock Size: %ld\n"
					"\t\tFree Inodes: %ld\n"
					"\t\tTotal Inodes: %ld\n",
					fd,
					#if defined(Solaris) || defined(IRIX)
						buf->f_bfree,
					#else
						buf->f_bavail,
					#endif
					buf->f_bfree,
					buf->f_blocks,
					buf->f_bsize,
					buf->f_ffree,
					buf->f_files);
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

#ifndef Solaris
int getdomainname_test(char *name, int namelen)
{
	int ret, save_errno;
	int passed;

	printf("getdomainname(): name=0x%x, namelen=%d\n", name, namelen); 
	fflush(NULL);

	passed = handle_zng(ret = getdomainname(name, namelen));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(IRIX65)
				is_errno_valid(save_errno, EFAULT, EPERM, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tname = %s\n", STR(name));
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}
#endif

int gettimeofday_test(struct timeval *tv, struct timezone *tz)
{
	int ret, save_errno;
	int passed;

	printf("gettimeofday(): tv=0x%x, tz=0x%x\n", tv, tz);
	fflush(NULL);

	passed = handle_zng(ret = gettimeofday(tv, tz));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EINVAL, EPERM, ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			/* good return value */
			printf("\t\ttv_sec = %u\n", tv->tv_sec);
			printf("\t\ttv_usec = %ld\n", tv->tv_usec);
			printf("\t\ttz_minuteswest = %d\n", tz->tz_minuteswest);
			printf("\t\ttz_dsttime = %ld\n", tz->tz_dsttime);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

#ifndef LINUX
int lchown_test(char *path, uid_t owner, gid_t group) /* link chown... */
{
	int ret, save_errno;
	int passed;

	printf("lchown(): path=%s, owner=%u, group=%u\n", STR(path), owner, group); 
	fflush(NULL);

	passed = handle_zng(ret = lchown(path, owner, group));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EFAULT, EINTR, EINVAL,
					EIO, ELOOP, EMULTIHOP, ENAMETOOLONG, ENOLINK, ENOENT,
					ENOTDIR, EPERM, EROFS, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}
#endif

int link_test(char *existing, char *newf)
{
	int ret, save_errno;
	int passed;

	printf("link(): existing=%s, newf=%s\n", STR(existing), STR(newf));
	fflush(NULL);

	passed = handle_zng(ret = link(existing, newf));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EDQUOT, EEXIST, EFAULT,
					EINTR, ELOOP, EMLINK, EMULTIHOP, ENAMETOOLONG, ENOENT,
					ENOLINK, ENOSPC, ENOTDIR, EPERM, EROFS, EXDEV, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed: OS returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

off_t lseek_test(int fd, off_t off, int whence)
{
	off_t ret;
	int save_errno;
	int passed;

	printf("lseek(): fd=%d, off=%u, whence=%d\n", fd, off, whence);
	fflush(NULL);

	passed = handle_off(ret = lseek(fd, off, whence));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EBADF, EINVAL, EOVERFLOW, ESPIPE,
					ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			printf("\t\tret = %u\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed: OS returned undefined value! "
				"ret = %u\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;

	}

	errno = save_errno;
	return ret;
}

int fseek_test(FILE *fp, int off, int whence)
{
	int ret, save_errno;
	int passed;

	printf("fseek(): fp=0x%x, off=%d\n", fp, off); 
	fflush(NULL);

	passed = handle_zng(ret = fseek(fp, off, whence));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EAGAIN, EBADF, EFBIG, EINTR,
					EINVAL, EIO, ENOSPC, EPIPE, ENXIO, EOVERFLOW, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}
	
	errno = save_errno;
	return ret;
}

int lstat_test(char *path, struct stat *buf)
{
	int ret, save_errno;
	int passed;

	printf("lstat(): path=%s\n", STR(path));
	fflush(NULL);

	passed = handle_zng(ret = lstat(path, buf));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EOVERFLOW, EACCES, EFAULT, EINTR,
					ELOOP, EMULTIHOP, ENAMETOOLONG, ENOENT, ENOLINK,
					ENOTDIR, EOVERFLOW, ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			/* good return value */
			printf("\t\tst_dev = %u\n", buf->st_dev);
			printf("\t\tst_ino = %u\n", buf->st_ino);
			printf("\t\tst_mode = 0x%x(0%o)\n",
				buf->st_mode,buf->st_mode);
			printf("\t\tst_nlink = %u\n", buf->st_nlink);
			printf("\t\tst_uid = %u\n", buf->st_uid);
			printf("\t\tst_gid = %u\n", buf->st_gid);
			printf("\t\tst_rdev = %u\n", buf->st_rdev);
			printf("\t\tst_size = %u\n", buf->st_size);
			printf("\t\tst_atime = %u\n", buf->st_atime);
			printf("\t\tst_mtime = %u\n", buf->st_mtime);
			printf("\t\tst_ctime = %u\n", buf->st_ctime);
			printf("\t\tst_blksize = %lu\n", buf->st_blksize);
			printf("\t\tst_blocks = %lu\n", buf->st_blocks);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int mkdir_test(char *dir, mode_t mode)
{
	int ret, save_errno;
	int passed;

	printf("mkdir(): dir=%s\n", STR(dir)); 
	fflush(NULL);

	passed = handle_zng(ret = mkdir(dir, mode));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EDQUOT, EEXIST, EFAULT,
					EIO, ELOOP, EMLINK, EMULTIHOP, ENAMETOOLONG, ENOENT,
					ENOLINK, ENOSPC, ENOTDIR, EROFS, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

char *getcwd_test(char *buf, size_t size)
{
	char *ret;
	int save_errno;
	int passed;

	printf("getcwd(): buf=0x%x, size=%u\n", buf, size);
	fflush(NULL);

	passed = handle_ptr(ret = getcwd(buf, size));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EINVAL, ERANGE, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %s\n", STR(ret));
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int mknod_test(char *path, mode_t mode, dev_t dev)
{
	int ret, save_errno;
	int passed;

	printf("mknod(): path=%s, mode=0x%x(0%o), dev=%u\n", STR(path), 
		mode, mode, dev);
	fflush(NULL);

	passed = handle_zng(ret = mknod(path, mode, dev));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EDQUOT, EEXIST, EFAULT, 
					EINTR, EINVAL, EIO, ELOOP, EMULTIHOP, ENAMETOOLONG,
					ENOENT, ENOLINK, ENOSPC, ENOTDIR, EPERM, EROFS, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int open_test(char *path, int oflags, /* mode_t mode */ ...)
{
	va_list ap;
	int ret, save_errno;
	mode_t mode;
	int passed;

	va_start(ap, oflags);

	/* if O_CREAT is set in the oflags, then mode is valid */
	if (O_CREAT & oflags) {
		mode = va_arg(ap, mode_t);

		printf("open(): path=%s, flags=0x%x(0%o), mode=0x%x(0%o)\n", STR(path), 
			oflags, oflags, mode, mode); 
		fflush(NULL);
	} else {
		printf("open(): path=%s, flags=0x%x(0%o)\n", STR(path), 
			oflags, oflags); 
		fflush(NULL);
	}
	va_end(ap);

	/* if O_CREAT isn't used, then mode will be ignored, so it doesn't matter
		if it is undefined at this point. If O_CREAT was in the oflags, then
		mode will be defined as above. */
	passed = handle_gez(ret = open(path, oflags, mode));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno,EACCES, EDQUOT, EEXIST, EINTR, EFAULT,
					EIO, EISDIR, ELOOP, EMFILE, EMULTIHOP, ENAMETOOLONG, ENFILE,
					ENOENT, ENOLINK, ENOSR, ENOSPC, ENOTDIR, ENXIO, EOPNOTSUPP,
					EOVERFLOW, EROFS, EAGAIN, EINVAL, ENOMEM, ETXTBSY, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
		break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
		break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}


FILE* fopen_test(char *file, char *mode)
{
	FILE *fp;
	int save_errno;
	int passed;

	printf("fopen(): file=%s, mode=\"%s\"\n", STR(file), mode); 
	fflush(NULL);

	passed = handle_ptr(fp = fopen(file, mode));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EINTR, EISDIR, ELOOP, EMFILE,
					ENAMETOOLONG, ENFILE, ENOENT, ENOSPC, ENOTDIR, ENXIO, 
					EOVERFLOW, EROFS, EINVAL, ENOMEM, ETXTBSY, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			printf("\t\tfp = 0x%x\n", fp);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"fp = 0x%x\n", fp);
			fflush(NULL);
		break;
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}
	
	errno = save_errno;
	return fp;
}

int utimes_test(char *filename, struct timeval tvp[2])
{
	int ret, save_errno;
	int passed;

	printf("utimes(): filename=%s, tvp=0x%x\n", STR(filename), tvp);
	fflush(NULL);

	passed = handle_zng(ret = utimes(filename, tvp));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EFAULT, EINTR, EINVAL,
					EIO, ELOOP, EMULTIHOP, ENAMETOOLONG, ENOLINK, ENOENT,
					ENOTDIR, EPERM, EROFS, ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			/* good return value */
			printf("\t\ttvp[0].tv_sec = %ld\n", tvp[0].tv_sec);
			printf("\t\ttvp[0].tv_usec = %ld\n", tvp[0].tv_usec);
			printf("\t\ttvp[1].tv_sec = %ld\n", tvp[1].tv_sec);
			printf("\t\ttvp[1].tv_usec = %ld\n", tvp[1].tv_usec);
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int fclose_test(FILE *fp)
{
	int ret, save_errno;
	int passed;

	printf("fclose(): fp=0x%x\n", fp);
	fflush(NULL);

	passed = handle_eof(ret = fclose(fp));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EAGAIN, EBADF, EFBIG, EINTR, EIO,
					ENOSPC, EPIPE, ENXIO, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int read_test(int fd, char *buf, size_t len)
{
	int ret, save_errno;
	int passed;

	printf("read(): fd=%d, buf=0x%x, len=%d\n", fd, buf, len); 
	fflush(NULL);

	/* This looks like a read() for this purpose */
	passed = handle_gez(ret = full_read(fd, buf, len));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EAGAIN, EBADF, EBADMSG, EDEADLK,
					EFAULT, EINTR, EINVAL, EIO, EISDIR, ENOLCK, ENOLINK,
					ENXIO, EOVERFLOW, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			printf("\t\tRead %d blocks\n", ret);
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;

	}

	errno = save_errno;
	return ret;
}

int fread_test(void *ptr, size_t size, size_t nitems, FILE *stream)
{
	int ret, save_errno;
	int passed;

	printf("fread(): ptr=0x%x, size=%u, nitmes=%u, stream=0x%x\n",ptr,size,
		nitems, stream);
	fflush(NULL);

	/* You may treat this as a libc call for this purpose */
	passed = handle_gez(ret = full_fread(ptr, size, nitems, stream));
	save_errno = errno;

	/* This test is a little different than the rest because of the really
		wierd behaviour of fread(). Notice the check in SUCCESS. */
	switch(passed) {
		case FAILURE:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		case SUCCESS:
			if((ret == 0) && ferror(stream)) {
				#if defined(Solaris26)
					is_errno_valid(save_errno, EOVERFLOW, ENDLIST);
				#endif

				printf("\t\tHit end of file.\n");
				fflush(NULL);
			}

			printf("\t\tRead %d blocks\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

long ftell_test(FILE *fp)
{
	long ret; 
	int save_errno;
	int passed;

	printf("ftell(): fp=0x%x\n", fp); 
	fflush(NULL);

	passed = handle_lng(ret = ftell(fp));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EBADF, ESPIPE, EOVERFLOW, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int readlink_test(char *path, char *buf, size_t bufsiz)
{
	int ret, save_errno;
	int passed;

	printf("readlink(): path=%s, buf=0x%x, bufsiz=%u\n",STR(path),buf,bufsiz); 
	fflush(NULL);

	/* do not assume buf will be null terminated */
	passed = handle_gez(ret = readlink(path, buf, bufsiz));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EFAULT, EINVAL, EIO, ENOENT,
					ELOOP, ENAMETOOLONG, ENOTDIR, ENOSYS, ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			/* print no more than 31 characters if buf isn't null
				terminated. */
			printf("\t\tLink contents =  [%.*s...]\n", 
				bufsiz<31?bufsiz:31, buf);

			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int readv_test(int fd, struct iovec *iov, int iovcnt)
{
	int ret, save_errno;
	int passed;

	printf("readv(): fd=%d, iov=0x%x, iovcnt=%d\n", fd, iov, iovcnt); 
	fflush(NULL);

	passed = handle_gez(ret = readv(fd, iov, iovcnt));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EAGAIN, EBADF, EBADMSG, EDEADLK,
					EFAULT, EINTR, EINVAL, EIO, EISDIR, ENOLCK, ENOLINK,
					ENXIO, EOVERFLOW, EFAULT, EINVAL, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			if (ret != -1) {
				printf("\t\tRead %d blocks\n", ret);
			}
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}


int rmdir_test(char *path)
{
	int ret, save_errno;
	int passed;

	printf("rmdir(): path=%s\n", STR(path)); 
	fflush(NULL);

	passed = handle_zng(ret = rmdir(path));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EBUSY, EEXIST, EFAULT,
					EINVAL, EIO, ELOOP, EMULTIHOP, ENAMETOOLONG, ENOENT,
					ENOLINK, ENOTDIR, EROFS, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int setregid_test(gid_t rgid, gid_t egid)
{
	int ret, save_errno;
	int passed;

	printf("setregid(): rgid=%d, egid=%d\n", rgid, egid); 
	fflush(NULL);

	passed = handle_zng(ret = setregid(rgid, egid));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EINVAL, EPERM, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}
	errno = save_errno;
	return ret;
}

int setreuid_test(uid_t ruid, uid_t euid)
{
	int ret, save_errno;
	int passed;

	printf("setreuid(): ruid=%u, euid=%u\n", ruid, euid);
	fflush(NULL);

	passed = handle_zng(ret = setreuid(ruid, euid));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EINVAL, EPERM, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}
	errno = save_errno;
	return ret;
}

/* WARNING!
 *
 * This test ONLY tests RLIMIT_CORE now; it used to be able to test more
 * but no longer does because of funky enum types on Linux
 * -Erik, May 18 2001 (A beautiful friday that I should be drinking instead of
 *                     coding on)
 */ 
int setrlimit_test(struct rlimit *rlp)
{
	int ret, save_errno;
	int passed;

	printf("setrlimit(): rlp=0x%x\n", rlp); 
	printf("\t\trlim_cur = %u\n", rlp->rlim_cur);
	printf("\t\trlim_max = %u\n", rlp->rlim_max);
	fflush(NULL);

	passed = handle_zng(ret = setrlimit(RLIMIT_CORE, rlp));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EFAULT, EINVAL, EPERM, EINVAL,
					ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}
	errno = save_errno;
	return ret;
}

int stat_test(char *path, struct stat *buf)
{
	int ret, save_errno;
	int passed;

	printf("stat(): path=%s, buf=0x%x\n", STR(path), buf);
	fflush(NULL);

	passed = handle_zng(ret = stat(path, buf));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EOVERFLOW, EACCES, EFAULT, EINTR,
					ELOOP, EMULTIHOP, ENAMETOOLONG, ENOENT, ENOLINK, ENOTDIR,
					ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			/* good return value */
			printf("\t\tst_dev = %u\n", buf->st_dev);
			printf("\t\tst_ino = %u\n", buf->st_ino);
			printf("\t\tst_mode = 0x%x(0%o)\n",
				buf->st_mode,buf->st_mode);
			printf("\t\tst_nlink = %u\n", buf->st_nlink);
			printf("\t\tst_uid = %u\n", buf->st_uid);
			printf("\t\tst_gid = %u\n", buf->st_gid);
			printf("\t\tst_rdev = %u\n", buf->st_rdev);
			printf("\t\tst_size = %u\n", buf->st_size);
			printf("\t\tst_atime = %u\n", buf->st_atime);
			printf("\t\tst_mtime = %u\n", buf->st_mtime);
			printf("\t\tst_ctime = %u\n", buf->st_ctime);
			printf("\t\tst_blksize = %lu\n", buf->st_blksize);
			printf("\t\tst_blocks = %lu\n", buf->st_blocks);
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}
	errno = save_errno;
	return ret;
}

int symlink_test(char *old, char *newf)
{
	int ret, save_errno;
	int passed;

	printf("symlink(): old=%s, new=%s\n", STR(old), STR(newf));
	fflush(NULL);

	passed = handle_zng(ret = symlink(old, newf));
	save_errno = errno;

	switch(passed)
	{
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EDQUOT, EEXIST, EFAULT,
					EIO, ELOOP, ENAMETOOLONG, ENOENT, ENOSPC, ENOSYS,
					ENOTDIR, EROFS, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}
	errno = save_errno;
	return ret;
}

int truncate_test(char *file, size_t size)
{
	int ret, save_errno;
	int passed;

	printf("truncate(): file=%s, size=%d\n", STR(file), size);
	fflush(NULL);

	passed = handle_zng(ret = truncate(file, size));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EINTR, EINVAL, EFBIG, EIO, EACCES,
					EFAULT, EINVAL, EISDIR, ELOOP, EMFILE, EMULTIHOP, 
					ENAMETOOLONG, ENOENT, ENFILE, ENOTDIR, ENOLINK, EROFS,
					ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}
	errno = save_errno;
	return ret;
}

int ftruncate_test(int fd, size_t size)
{
	int ret, save_errno;
	int passed;

	printf("ftruncate(): fd=%d, size=%d\n", fd, size); 
	fflush(NULL);

	passed = handle_zng(ret = ftruncate(fd, size));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EINTR, EFBIG, EIO, EAGAIN, EBADF,
					EINVAL, ENOLINK, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}
	errno = save_errno;
	return ret;
}

/* XXX think about how to check the return value on this one.... */
mode_t umask_test(mode_t cmask)
{
	int ret;

	printf("umask(): cmask=0x%x(O%o)\n", cmask, cmask);
	fflush(NULL);

	ret = umask(cmask);

	printf("\t\tret = 0x%x(0%o)\n", ret, ret);
	printf("\tSucceeded Phase 1\n");
	fflush(NULL);

	return ret;
}

int unlink_test(char *path)
{
	int ret, save_errno;
	int passed;

	printf("unlink(): path=%s\n", STR(path));
	fflush(NULL);

	passed = handle_zng(ret = unlink(path));
	save_errno = errno;
	
	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EBUSY, EFAULT, EINTR, ELOOP,
					EMULTIHOP, ENAMETOOLONG, ENOENT, ENOLINK, ENOTDIR, EPERM,
					EROFS, ETXTBSY, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			/* good return value */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int gethostname_test(char *name, size_t len)
{
	int ret, save_errno;
	int passed;

	printf("gethostname(): name=0x%x, len=%u\n", name, len);
	fflush(NULL);

	passed = handle_zng(ret = gethostname(name, len));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EINVAL, EPERM, EFAULT, ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			/* good return value */
			printf("\t\tname = %.80s\n", name); /* may not be null terminated */
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int utime_test(char *file, struct utimbuf *times)
{
	int ret, save_errno;
	int passed;

	printf("utime(): file=%s, times=0x%x\n", STR(file), times);
	fflush(NULL);

	passed = handle_zng(ret = utime(file, times));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EACCES, EFAULT, EINTR, EIO, ELOOP,
					EMULTIHOP, ENAMETOOLONG, ENOENT, ENOLINK, ENOTDIR, EPERM,
					EROFS, ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			/* good return value */
			printf("\t\tactime = %u\n", times->actime);
			printf("\t\tmodtime = %u\n", times->modtime);
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

/* This function normally returns a void, so for us it is always a success, we
	must check the semantics of rewind to see if it actually worked. */
int rewind_test(FILE *stream)
{
	printf("rewind(): stream=0x%x\n", stream);
	fflush(NULL);

	rewind(stream);

	printf("\t\tret = (void)\n");
	fflush(NULL);

	return SUCCESS;
}

int uname_test(struct utsname *name)
{
	int ret, save_errno;
	int passed;

	printf("uname(): name=0x%x\n", name); 
	fflush(NULL);

	passed = handle_gez(ret = uname(name));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EFAULT, ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			/* good return value */
			printf("\t\tsysname = %s\n", STR(name->sysname));
			printf("\t\tnodename = %s\n", STR(name->nodename));
			printf("\t\trelease = %s\n", STR(name->release));
			printf("\t\tversion = %s\n", STR(name->version));
			printf("\t\tmachine = %s\n", STR(name->machine));
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int write_test(int fd, char *buf, size_t count)
{
	int ret, save_errno;
	int passed;

	printf("write(): fd=%d, buf=[%.31s...], count=%d\n", 
		fd, buf, count); 
	fflush(NULL);

	/* you can treat full_write as just a write */
	passed = handle_gez(ret = full_write(fd, buf, strlen(buf)));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EAGAIN, EBADF, EDEADLK, EDQUOT,
					EFAULT, EFBIG, EINTR, EIO, ENOLCK, ENOLINK, ENOSPC,
					ENOSR, ENXIO, EPIPE, ERANGE, EINVAL, ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			if (ret != -1) { /* check to make sure good */
				printf("\t\tWrote %d blocks\n", ret);
				printf("\t\tret = %d\n", ret);
			}
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}
	
	errno = save_errno;
	return ret;
}

int fwrite_test(char *ptr, size_t size, size_t nitems, FILE *stream)
{
	int ret, save_errno;
	int passed;

	printf("fwrite(): ptr=[%.31s...], fp=0x%x\n", (char*)ptr, stream);
	fflush(NULL);

	/* You may treat this as a normal fwrite */
	passed = handle_gez(ret = full_fwrite(ptr, size, nitems, stream));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		case SUCCESS:
			if ((ret == 0) && ferror(stream)) {
					#if defined(Solaris26)
						is_errno_valid(save_errno, EFBIG, ENDLIST);
					#endif
			}
			if (!ferror(stream)) {
				printf("\t\tWrote %d blocks\n", ret);
			} else {
				printf("\t\tHit end of file.\n");
			}
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

FILE *tmpfile_test(void)
{
	FILE *ret;
	int save_errno;
	int passed;

	printf("tmpfile():\n");
	fflush(NULL);

	passed = handle_ptr(ret = tmpfile());
	save_errno = errno;

	switch(passed)
	{
		case FAILURE:
			#if defined(Solaris)
				is_errno_valid(save_errno, EINTR, EMFILE, ENOSPC, ENOMEM,
					ENDLIST);
			#endif
			/* FALL THROUGH */
		case SUCCESS:
			printf("\tret = 0x%x\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
		break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = 0x%x\n", ret);
			fflush(NULL);
		break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}

	errno = save_errno;
	return ret;
}

int writev_test(int fd, struct iovec *iov, int iovcnt)
{
	int ret, save_errno;
	int passed;

	printf("writev(): fd=%d, iov=0x%x, iovcnt=%d\n", fd, iov, iovcnt); 
	fflush(NULL);

	passed = handle_gez(ret = writev(fd, iov, iovcnt));
	save_errno = errno;

	switch(passed) {
		case FAILURE:
			#if defined(Solaris26)
				is_errno_valid(save_errno, EAGAIN, EBADF, EDEADLK, EDQUOT,
					EFAULT, EFBIG, EINTR, EIO, ENOLCK, ENOLINK, ENOSPC,
					ENOSR, ENXIO, EPIPE, ERANGE, EINVAL, ENDLIST);
			#endif
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case SUCCESS:
			printf("\t\tWrote %d blocks\n", ret);
			printf("\t\tret = %d\n", ret);
			printf("\tSucceeded Phase 1\n");
			fflush(NULL);
			break;
		case UNDEFINED:
			printf("\tFailed Phase 1: returned undefined value! "
				"ret = %d\n", ret);
			fflush(NULL);
			break;
		default:
			printf("Internal syscalltester error: passed = %d\n", passed);
			fflush(NULL);
			break;
	}
	errno = save_errno;
	return ret;
}

/* These functions do the self analysis, I expect something to happen,
	these tell me if it did.  Expected is SUCCESS, or FAILURE,
	depending upon what I am looking for. These are Phase 2 tests. */

/* handle when something returns a -1 as failure and zero on success */
int expect_zng(int expected, int result)
{
	printf("\tZNG Expected %s, Got %s\n", 
		expected==SUCCESS?"SUCCESS":"FAILURE", 
		result==SUCCESS?"SUCCESS":"FAILURE");
	fflush(NULL);

	if(result == expected) {
		return SUCCESS;
	}
	return FAILURE;
}

/* handle when something returns a NULL as failure */
/* expected is SUCCESS, or FAILURE, depending upon what I am looking for */
int expect_ptr(int expected, void *result)
{
	printf("\tPTR Expected %s, Got %s\n", 
		expected==SUCCESS?"NON-NULL":"NULL", 
		result==NULL?"NULL":"NON-NULL");
	fflush(NULL);

	if (expected == SUCCESS)
	{
		if (result == NULL)
		{
			return FAILURE;
		}

		return SUCCESS;
	}

	if (expected == FAILURE)
	{
		if (result == NULL)
		{
			return SUCCESS;
		}

		return FAILURE;
	}
}

/* handle something where result >= 0 is success */
/* expected is SUCCESS, or FAILURE, depending upon what I am looking for */
int expect_gez(int expected, int result)
{
	printf("\tGEZ Expected %s, Got %s\n", 
		expected==SUCCESS?"SUCCESS":"FAILURE", 
		result>=0?"SUCCESS":"FAILURE");
	fflush(NULL);

	if((result >= 0) && (expected == SUCCESS)) {
		return SUCCESS;
	}
	return FAILURE;
}

/* handle something that can return an unsigned positive number */
int expect_off(off_t expected, off_t ret)
{
	printf("\tOFF Expected %s, Got %s\n", 
		expected==SUCCESS?"SUCCESS":"FAILURE", 
		ret>=0?"SUCCESS":"FAILURE");
	fflush(NULL);

	if ((ret == (off_t)-1) && expected == SUCCESS) {
		return FAILURE;
	}

	return SUCCESS;
}

/* this is for when I'm expecting a particular long value(or any other type of
	normal int */
int expect_val(long expected, long ret)
{
	printf("\tVAL Expected %d, Got %d\n", expected, ret);
	fflush(NULL);

	if (ret == expected) {
		return SUCCESS;
	}

	return FAILURE;
}

/* this is for when I'm expecting a particular off_t value */
int expect_vao(off_t expected, off_t ret)
{
	printf("\tVAO Expected %d, Got %d\n", expected, ret);
	fflush(NULL);

	if (ret == expected) {
		return SUCCESS;
	}

	return FAILURE;
}

/* this is for when I'm expecting a particular pointer value */
int expect_vap(void *expected, void *ret)
{
	printf("\tVAP Expected 0x%x, Got 0x%x\n", expected, ret);
	fflush(NULL);

	if (ret == expected) {
		return SUCCESS;
	}

	return FAILURE;
}

/* these next two calls are kinda funky because getuid/getgid/umask
	don't have error return codes, anything can be valid */
int expect_uid(int expected, uid_t ret)
{
	printf("\tUID Expected %s, Got %u "
		"(Will always return expected))\n", 
		expected==SUCCESS?"SUCCESS":"FAILURE", ret);
	fflush(NULL);
	
	if (expected == FAILURE) {
		printf("\tFailed Phase 2: You may not expect failure on a uid_t.\n");
		fflush(NULL);
	}
	return SUCCESS;
}

int expect_gid(int expected, gid_t ret)
{
	printf("\tGID Expected %s, Got %u "
		"(Will always return expected))\n", 
		expected==SUCCESS?"SUCCESS":"FAILURE", ret);
	fflush(NULL);
	
	if (expected == FAILURE) {
		printf("\tFailed Phase 2: You may not expect failure on a gid_t.\n");
		fflush(NULL);
	}
	return SUCCESS;
}

int expect_msk(int expected, mode_t ret)
{
	printf("\tMSK Expected %s, Got 0x%x(0%o) "
		"(Will always return expected))\n", 
		expected==SUCCESS?"SUCCESS":"FAILURE", ret, ret);
	fflush(NULL);
	
	if (expected == FAILURE) {
		printf("\tFailed Phase 2: You may not expect failure on a mode_t.\n");
		fflush(NULL);
	}
	return SUCCESS;
}

/* This is for when I'm expecting a boolean value from something */
/* WARNING! This is a 'C' boolean, meaning you may not assign a SUCCESS to
	a variable and then expect_bol it to be equal to SUCCESS because SUCCESS
	is defined to be ZERO! */
int expect_bol(int expected, int ret)
{
	printf("\tBOL Expected %s, Got %s\n", 
		expected==SUCCESS?"TRUE(SUCCESS)":"FALSE(FAILURE)",
		ret?"TRUE(SUCCESS)":"FALSE(FAILURE)");
	fflush(NULL);

	if ((expected == SUCCESS) && (ret == 0)) {
		return FAILURE;
	}
	return SUCCESS;
}

/* --------------------------------------------------------------------- */
/* Begin the actual test code */

/* Do a super basic test of creat(), access(), close(), and unlink() */
int BasicFile(void)
{
	char tf[NAMEBUF] = {0};
	int fd;
	FILE *fp;
	int passed;
	int block = SUCCESS;

	xtmpnam(tf);

	testbreak();

	/* test creat() */
	passed = expect_gez(SUCCESS, fd = creat_test(tf, S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(SUCCESS, access_test(tf, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(tf, F_OK));
	EXPECTED_RESP;

	testbreak();

	/* test open() */
	passed = expect_gez(SUCCESS, fd = open_test(tf, O_CREAT|O_TRUNC, S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(SUCCESS, access_test(tf, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(tf, F_OK));
	EXPECTED_RESP;

	testbreak();
	return block;
}

/* can I do simple writes, reads, and seeks? */
int BasicFileIO(void)
{
	char tf[NAMEBUF] = {0};
	char readbuf[sizeof(passage)] = {0};
	int fd;
	FILE *fp;
	int passed;
	int block = SUCCESS;

	xtmpnam(tf);

	/* test fd versions */
	testbreak();

	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, write_test(fd, passage, strlen(passage)));
	EXPECTED_RESP;
	passed = expect_vao(0, lseek_test(fd, 0, SEEK_SET));
	EXPECTED_RESP;
	passed = expect_gez(SUCCESS, read_test(fd, readbuf, strlen(passage)));
	EXPECTED_RESP;
	if (strncmp(passage, readbuf, strlen(passage)) != 0) {
		printf("\tFailed Phase 2: read() returned garbage in buffer\n");
		fflush(NULL);
		block = FAILURE;
	}
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(tf, F_OK));
	EXPECTED_RESP;

	testbreak();

	/* test FILE* versions */
	passed = expect_ptr(SUCCESS, fp = fopen_test(tf,"w+"));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, fwrite_test(passage, strlen(passage), 1, fp));
	EXPECTED_RESP;
	passed = expect_off(SUCCESS, fseek_test(fp, 40, SEEK_SET));
	EXPECTED_RESP;
	passed = expect_val(40, ftell_test(fp));
	EXPECTED_RESP;
	passed = expect_gez(SUCCESS, fread_test(readbuf, 20, 1, fp));
	EXPECTED_RESP;
	if (strncmp(&passage[40], readbuf, 20) != 0) {
		printf("\tFailed Phase 2: fread() returned garbage in buffer\n");
		fflush(NULL);
		block = FAILURE;
	}
	passed = expect_zng(SUCCESS, fclose_test(fp));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(tf, F_OK));
	EXPECTED_RESP;

	testbreak();

	/* test FILE* versions specifically rewind() */
	passed = expect_ptr(SUCCESS, fp = fopen_test(tf,"w+"));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, fwrite_test(passage, strlen(passage), 1, fp));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, rewind_test(fp));
	EXPECTED_RESP;
	passed = expect_val(0, ftell_test(fp));
	EXPECTED_RESP;
	passed = expect_gez(SUCCESS, fread_test(readbuf, strlen(passage), 1, fp));
	EXPECTED_RESP;
	if (strncmp(passage, readbuf, strlen(passage)) != 0) {
		printf("\tFailed Phase 2: fread() returned garbage in buffer\n");
		fflush(NULL);
		block = FAILURE;
	}
	passed = expect_zng(SUCCESS, fclose_test(fp));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(tf, F_OK));
	EXPECTED_RESP;

	testbreak();
	return block;
}

int BasicFreopen(void)
{
	int block = SUCCESS;
	FILE *fp;
	FILE *fp2;
	int passed;
	char tf[NAMEBUF] = {0};

	xtmpnam(tf);

	testbreak();

	passed = expect_ptr(SUCCESS, fp = fopen_test(tf, "w+"));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_vap(fp, fp2 = freopen_test(tf, "ad", fp));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(SUCCESS, fclose_test(fp2));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();
	return block;
}

int BasicStat(void)
{
	int fd;
	FILE *fp;
	int passed;
	int block = SUCCESS;
	char tf[NAMEBUF] = {0};
	struct stat buf;

	xtmpnam(tf);

	testbreak();

	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, write_test(fd, passage, strlen(passage)));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fstat_test(fd, &buf));
	EXPECTED_RESP;
	passed = expect_bol(SUCCESS, (buf.st_mode & S_IRWXU));
	EXPECTED_RESP;
	passed = expect_val(strlen(passage), buf.st_size);
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();

	passed = expect_ptr(SUCCESS, fp = fopen_test(tf,"w+"));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, fwrite_test(passage, strlen(passage), 1, fp));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fclose_test(fp));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, stat_test(tf, &buf));
	EXPECTED_RESP;
	passed = expect_val(strlen(passage), buf.st_size);
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();
	return block;
}

int BasicFilePerm(void)
{
	int block = SUCCESS;
	int fd;
	int passed;
	char tf[NAMEBUF] = {0};
	struct stat buf;

	xtmpnam(tf);

	testbreak();

	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(SUCCESS, fstat_test(fd, &buf));
	EXPECTED_RESP;
	passed = expect_bol(SUCCESS, (buf.st_mode & S_IRWXU));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fchmod_test(fd, S_IRWXO));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fstat_test(fd, &buf));
	EXPECTED_RESP;
	passed = expect_bol(SUCCESS, (buf.st_mode & S_IRWXO));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fchmod_test(fd, S_IRWXU));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fstat_test(fd, &buf));
	EXPECTED_RESP;
	passed = expect_bol(SUCCESS, (buf.st_mode & S_IRWXU));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();

	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(SUCCESS, stat_test(tf, &buf));
	EXPECTED_RESP;
	passed = expect_bol(SUCCESS, (buf.st_mode & S_IRWXU));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, chmod_test(tf, S_IRWXO));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, stat_test(tf, &buf));
	EXPECTED_RESP;
	passed = expect_bol(SUCCESS, (buf.st_mode & S_IRWXO));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd)); /* close in the middle */
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, chmod_test(tf, S_IRWXU));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, stat_test(tf, &buf));
	EXPECTED_RESP;
	passed = expect_bol(SUCCESS, (buf.st_mode & S_IRWXU));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();
	return block;
}

int BasicUid(void)
{
	char tf[NAMEBUF] = {0};
	int fd;
	FILE *fp;
	int passed;
	int block = SUCCESS;
	uid_t uid;
	gid_t gid;
	struct stat buf;

	xtmpnam(tf);
	testbreak();

	/* validate what stat, and getuid, and setuid do */
	passed = expect_uid(SUCCESS, uid = getuid_test());
	EXPECTED_RESP;
	passed = expect_gid(SUCCESS, gid = getgid_test());
	EXPECTED_RESP;

	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(SUCCESS, stat_test(tf, &buf));
	EXPECTED_RESP;
	passed = expect_val(uid, buf.st_uid);
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fchown_test(fd, uid, gid));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fstat_test(fd, &buf));
	EXPECTED_RESP;
	passed = expect_val(uid, buf.st_uid);
	EXPECTED_RESP;
	passed = expect_val(gid, buf.st_gid);
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, chown_test(tf, uid, gid));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, stat_test(tf, &buf));
	EXPECTED_RESP;
	passed = expect_val(uid, buf.st_uid);
	EXPECTED_RESP;
	passed = expect_val(gid, buf.st_gid);
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();
	return block;
}

int BasicDup(void)
{
	char tf[NAMEBUF] = {0};
	int fd;
	int fd2;
	int passed;
	int block = SUCCESS;
	char readbuf[sizeof(passage)] = {0};

	xtmpnam(tf);

	testbreak();
	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, write_test(fd, passage, strlen(passage)));
	EXPECTED_RESP;

	passed = expect_gez(SUCCESS, fd2 = dup_test(fd));
	EXPECTED_RESP; 
	IF_FAILED { 
		passed = expect_zng(SUCCESS, close_test(fd));
		EXPECTED_RESP;
		ABORT_TEST;
	}

	/* Check the first fd for correctness */
	passed = expect_val(40, lseek_test(fd, 40, SEEK_SET));
	EXPECTED_RESP;
	passed = expect_gez(SUCCESS, read_test(fd, readbuf, 20));
	EXPECTED_RESP;
	if (strncmp(&passage[40], readbuf, 20) != 0) {
		printf("\tFailed Phase 2: fread() returned garbage for fd\n");
		fflush(NULL);
		block = FAILURE;
	}

	/* check the second fd for correctness */
	memset(readbuf, 0, sizeof(passage));

	passed = expect_vao(0, lseek_test(fd2, 0, SEEK_SET));
	EXPECTED_RESP;
	passed = expect_gez(SUCCESS, read_test(fd2, readbuf, strlen(passage)));
	EXPECTED_RESP;
	if (strncmp(passage, readbuf, strlen(passage)) != 0) {
		printf("\tFailed Phase 2: fread() returned garbage for fd2\n");
		fflush(NULL);
		block = FAILURE;
	}

	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd2));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();
	return block;
}

int BasicFcntlDup(void)
{
	char tf[NAMEBUF] = {0};
	int fd;
	int fd2;
	int passed;
	int block = SUCCESS;
	char readbuf[sizeof(passage)] = {0};

	xtmpnam(tf);

	testbreak();
	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, write_test(fd, passage, strlen(passage)));
	EXPECTED_RESP;

	passed = expect_gez(SUCCESS, fd2 = fcntl_test(fd, F_DUPFD, 42));
	EXPECTED_RESP; 
	IF_FAILED {
		passed = expect_zng(SUCCESS, close_test(fd));
		EXPECTED_RESP;
		ABORT_TEST;
	}

	/* Check the first fd for correctness */
	passed = expect_val(40, lseek_test(fd, 40, SEEK_SET));
	EXPECTED_RESP;
	passed = expect_gez(SUCCESS, read_test(fd, readbuf, 20));
	EXPECTED_RESP;
	if (strncmp(&passage[40], readbuf, 20) != 0) {
		printf("\tFailed Phase 2: fread() returned garbage for fd\n");
		fflush(NULL);
		block = FAILURE;
	}

	/* check the second fd for correctness */
	memset(readbuf, 0, sizeof(passage));

	passed = expect_vao(0, lseek_test(fd2, 0, SEEK_SET));
	EXPECTED_RESP;
	passed = expect_gez(SUCCESS, read_test(fd2, readbuf, strlen(passage)));
	EXPECTED_RESP;
	if (strncmp(passage, readbuf, strlen(passage)) != 0) {
		printf("\tFailed Phase 2: fread() returned garbage for fd2\n");
		fflush(NULL);
		block = FAILURE;
	}

	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd2));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();
	return block;
}

int BasicDir(void)
{
	char tf[NAMEBUF] = {0};
	int passed;
	int block = SUCCESS;
	struct stat buf;

	xtmpnam(tf);

	testbreak();

	passed = expect_zng(SUCCESS, mkdir_test(tf, S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(SUCCESS, stat_test(tf, &buf));
	EXPECTED_RESP
	if (!(S_ISDIR(buf.st_mode))) {
		printf("\tFailed Phase 2: mkdir() something that wasn't a directory\n");
		fflush(NULL);
		block = FAILURE;
	}
	passed = expect_zng(SUCCESS, rmdir_test(tf));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(tf, F_OK));
	EXPECTED_RESP;

	testbreak();
	return block;
}

int BasicChdir(void)
{
	char tf[NAMEBUF] = {0};
	int fd, fd2;
	int passed;
	int block = SUCCESS;
	char *cwdfile = "_.-'^`-._";
	char *cwd = NULL;
	char *cwd_chdir = NULL;
	struct stat buf;

	xtmpnam(tf);

	testbreak();
	passed = expect_ptr(SUCCESS, cwd = getcwd_test(NULL, NAMEBUF));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, 
				fd = open_test(cwdfile,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP;
	IF_FAILED {
		free(cwd);
		ABORT_TEST;
	}
	passed = expect_zng(SUCCESS, chdir_test("/tmp"));
	EXPECTED_RESP;
	passed = expect_ptr(SUCCESS, cwd_chdir = getcwd_test(NULL, NAMEBUF));
	EXPECTED_RESP;
	IF_FAILED {
		free(cwd);
		ABORT_TEST;
	}
	if ((strcmp(cwd, cwd_chdir) == 0)) {
		printf("\tFailed Phase 2: chdir() did not change directories\n");
		fflush(NULL);
		block = FAILURE;
	}
	if (cwd_chdir != NULL) { free(cwd_chdir); }
	passed = expect_zng(FAILURE, access_test(cwdfile, F_OK));
	EXPECTED_RESP;
	passed = expect_gez(SUCCESS, 
				fd2 = open_test(cwdfile,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; 
	IF_FAILED {
		free(cwd);
		ABORT_TEST;
	}
	passed = expect_zng(SUCCESS, access_test(cwdfile, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd2));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(cwdfile));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, chdir_test(cwd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, access_test(cwdfile, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(cwdfile));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(cwdfile, F_OK));
	EXPECTED_RESP;
	if (cwd) free(cwd);

	testbreak();

	return block;
}

int BasicFchdir(void)
{
	char tf[NAMEBUF] = {0};
	int fd, fd2;
	int passed;
	int block = SUCCESS;
	char *cwdfile = "_.-'^`-._";
	char *cwd = NULL;
	char *cwd_chdir = NULL;
	int tmpfd;
	struct stat buf;

	xtmpnam(tf);

	testbreak();
	passed = expect_ptr(SUCCESS, cwd = getcwd_test(NULL, NAMEBUF));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, 
				fd = open_test(cwdfile,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP;
	passed = expect_gez(SUCCESS, tmpfd = open_test("/tmp",O_RDONLY));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fchdir_test(tmpfd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(tmpfd));
	EXPECTED_RESP;
	passed = expect_ptr(SUCCESS, cwd_chdir = getcwd_test(NULL, NAMEBUF));
	EXPECTED_RESP;
	IF_FAILED {
		free(cwd);
		ABORT_TEST;
	}
	if (strcmp(cwd, cwd_chdir) == 0) {
		printf("\tFailed Phase 2: fchdir() did not change directories\n");
		fflush(NULL);
		block = FAILURE;
	}
	if (cwd_chdir != NULL) { free(cwd_chdir); }
	passed = expect_zng(FAILURE, access_test(cwdfile, F_OK));
	EXPECTED_RESP;
	passed = expect_gez(SUCCESS, 
				fd2 = open_test(cwdfile,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, access_test(cwdfile, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd2));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(cwdfile));
	EXPECTED_RESP;
	passed = expect_gez(SUCCESS, tmpfd = open_test(cwd,O_RDONLY));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fchdir_test(tmpfd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(tmpfd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, access_test(cwdfile, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(cwdfile));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(cwdfile, F_OK));
	EXPECTED_RESP;
	free(cwd);

	testbreak();

	return block;
}

/* This test may not work under condor yet */
int BasicMknod(void)
{
	char tf[NAMEBUF] = {0};
	int fd;
	int passed;
	int block = SUCCESS;

	xtmpnam(tf);

	testbreak();

	passed = expect_zng(SUCCESS, mknod_test(tf, S_IFIFO|S_IRWXU, 0));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(SUCCESS, access_test(tf, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();
	passed = expect_zng(FAILURE, mknod_test(tf, S_IFCHR|S_IRWXU, 0));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(FAILURE, access_test(tf, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();
	passed = expect_zng(FAILURE, mknod_test(tf, S_IFDIR|S_IRWXU, 0));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(FAILURE, access_test(tf, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();
	passed = expect_zng(FAILURE, mknod_test(tf, S_IFBLK|S_IRWXU, 0));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(FAILURE, access_test(tf, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, unlink_test(tf));
	EXPECTED_RESP;

	/* It should be that only root can perform this test and have it succeed.
		However glibc 2.2.2 will let a normal user use this function and
		have it succeed. So I'm turning it off until they fix it. -psilord */
#if !defined(LINUX) && !(defined(GLIBC22) || defined(GLIBC23))
	testbreak();
	passed = expect_zng(FAILURE, mknod_test(tf, S_IFREG|S_IRWXU, 0));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(FAILURE, access_test(tf, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, unlink_test(tf));
	EXPECTED_RESP;
#endif

	testbreak();
	return block;
}

int BasicLink(void)
{
	char tf[NAMEBUF] = {0};
	char slink[NAMEBUF] = {0};
	char hlink[NAMEBUF] = {0};
	char readlink[NAMEBUF] = {0};
	int fd;
	int passed;
	int block = SUCCESS;
	struct stat buf;
	struct stat buf2;
	uid_t uid;
	gid_t gid;
	int ls; /* symlink size in bytes */

	xtmpnam(tf);
	xtmpnam(slink);
	xtmpnam(hlink);

	testbreak();

	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, symlink_test(tf, slink));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, ls = readlink_test(slink, readlink, NAMEBUF));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, lstat_test(slink, &buf));
	EXPECTED_RESP;
	passed = expect_val(ls, buf.st_size);
	EXPECTED_RESP;

	/* Some versions of Linux do not have lchown(), or good impl. of it  */
#ifndef LINUX
	passed = expect_uid(SUCCESS, uid = getuid_test());
	EXPECTED_RESP;
	passed = expect_gid(SUCCESS, gid = getgid_test());
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, lchown_test(slink, uid, gid));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, lstat_test(slink, &buf2));
	EXPECTED_RESP;
	passed = expect_val(uid, buf2.st_uid);
	EXPECTED_RESP;
	passed = expect_val(gid, buf2.st_gid);
	EXPECTED_RESP;
	passed = expect_val(buf.st_size, buf2.st_size);
	EXPECTED_RESP;
	passed = expect_val(buf.st_mode, buf2.st_mode);
	EXPECTED_RESP;
#endif

	/* do redundant testing of access/lstat/and unlink with a symlink */
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(tf, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(slink, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, lstat_test(slink, &buf));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(slink));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(slink, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, lstat_test(slink, &buf));
	EXPECTED_RESP;
	testbreak();

	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, link_test(tf, hlink));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(SUCCESS, stat_test(tf, &buf));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, lstat_test(hlink, &buf2));
	EXPECTED_RESP;
	passed = expect_val(buf.st_size, buf2.st_size);
	EXPECTED_RESP;
	passed = expect_val(buf.st_mode, buf2.st_mode);
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(tf, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, access_test(hlink, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, stat_test(hlink, &buf2));
	EXPECTED_RESP;
	passed = expect_val(buf2.st_mode, buf.st_mode);
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(hlink));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(hlink, F_OK));
	EXPECTED_RESP;
	
	testbreak();
	return block;
}

int BasicRename(void)
{
	char tf[NAMEBUF] = {0};
	char ntf[NAMEBUF] = {0};
	int fd;
	int passed;
	int block = SUCCESS;

	xtmpnam(tf);
	xtmpnam(ntf);

	testbreak();

	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, rename_test(tf, ntf));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, access_test(tf, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(FAILURE, unlink_test(tf));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, access_test(ntf, F_OK));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(ntf));
	EXPECTED_RESP;

	testbreak();
	return block;
}

int BasicTruncation(void)
{
	char tf[NAMEBUF] = {0};
	char ntf[NAMEBUF] = {0};
	int fd;
	int passed;
	int block = SUCCESS;
	struct stat buf;
	struct stat buf2;

	xtmpnam(tf);

	/* test ftruncate */
	testbreak();

	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, write_test(fd, passage, sizeof(passage)));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, ftruncate_test(fd, 42));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fstat_test(fd, &buf));
	EXPECTED_RESP;
	passed = expect_val(buf.st_size, 42);
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();
	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, write_test(fd, passage, sizeof(passage)));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, truncate_test(tf, 42));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, stat_test(tf, &buf));
	EXPECTED_RESP;
	passed = expect_val(buf.st_size, 42);
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();
	return block;
}

#if defined(Solaris) || defined(IRIX65)
int BasicFcntlTruncation(void)
{
	char tf[NAMEBUF] = {0};
	char ntf[NAMEBUF] = {0};
	int fd;
	int passed;
	int block = SUCCESS;
	struct stat buf;
	struct stat buf2;
	struct flock fl;

	xtmpnam(tf);

	testbreak();
	/* fcntl truncate to zero length file */
	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, write_test(fd, passage, sizeof(passage)));
	EXPECTED_RESP;
	fl.l_type = 0; /* doesn't matter for this operation */
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_sysid = 0;
	fl.l_pid = 0;
	passed = expect_zng(SUCCESS, fcntl_test(fd, F_FREESP, &fl));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fstat_test(fd, &buf));
	EXPECTED_RESP;
	passed = expect_val(fl.l_start, buf.st_size);
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();

	/* fcntl truncate acting like an ftruncate to non zero length file */
	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, write_test(fd, passage, sizeof(passage)));
	EXPECTED_RESP;
	fl.l_type = 0; /* doesn't matter for this operation */
	fl.l_whence = SEEK_SET;
	fl.l_start = 42;
	fl.l_len = 0;
	fl.l_sysid = 0;
	fl.l_pid = 0;
	passed = expect_zng(SUCCESS, fcntl_test(fd, F_FREESP, &fl));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fstat_test(fd, &buf));
	EXPECTED_RESP;
	passed = expect_val(fl.l_start, buf.st_size);
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();
	return block;
}
#endif

int BasicIOV(void)
{
	char tf[NAMEBUF] = {0};
	int fd;
	int passed;
	int block = SUCCESS;
	struct iovec iov[3];

	/* initialize the vectors so I can do meaningful comparison */
	char snd1[sizeof(passage)] = "GARBAGE snd1"; /* < sizeof(passage) */
	char snd2[sizeof(passage)] = "GARBAGE snd2";
	char snd3[sizeof(passage)] = "GARBAGE snd3";
	char rcv1[sizeof(passage)] = "GARBAGE rcv1";
	char rcv2[sizeof(passage)] = "GARBAGE rcv2";
	char rcv3[sizeof(passage)] = "GARBAGE rcv3";

	strcpy(snd1, passage);
	strcpy(snd2, passage);
	strcpy(snd3, passage);

	xtmpnam(tf);

	testbreak();
	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	iov[0].iov_base = snd1;
	iov[0].iov_len = sizeof(passage);
	iov[1].iov_base = snd2;
	iov[1].iov_len = sizeof(passage);
	iov[2].iov_base = snd3;
	iov[2].iov_len = sizeof(passage);
	passed = expect_gez(SUCCESS, writev_test(fd, iov, 3));
	EXPECTED_RESP;
	passed = expect_vao(0, lseek(fd, 0, SEEK_SET));
	EXPECTED_RESP;
	iov[0].iov_base = rcv1;
	iov[0].iov_len = sizeof(passage);
	iov[1].iov_base = rcv2;
	iov[1].iov_len = sizeof(passage);
	iov[2].iov_base = rcv3;
	iov[2].iov_len = sizeof(passage);
	passed = expect_gez(SUCCESS, readv_test(fd, iov, 3));
	EXPECTED_RESP;

	if (strncmp(snd1, rcv1, strlen(passage)) != 0) {
		printf("\tFailed Phase 2: 1st iov_base read buffer is garbage\n");
		printf("\t\tExpected: [%.31s...]\n", STR(snd1));
		printf("\t\tGot:\t[%.31s...]\n", STR(rcv1));
		fflush(NULL);
		block = FAILURE;
	}
	if (strncmp(snd2, rcv2, strlen(passage)) != 0) {
		printf("\tFailed Phase 2: 2nd iov_base read buffer is garbage\n");
		printf("\t\tExpected: [%.31s...]\n", STR(snd2));
		printf("\t\tGot:\t[%.31s...]\n", STR(rcv2));
		fflush(NULL);
		block = FAILURE;
	}
	if (strncmp(snd3, rcv3, strlen(passage)) != 0) {
		printf("\tFailed Phase 2: 3rd iov_base read buffer is garbage\n");
		printf("\t\tExpected: [%.31s...]\n", STR(snd3));
		printf("\t\tGot:\t[%.31s...]\n", STR(rcv3));
		fflush(NULL);
		block = FAILURE;
	}

	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;
	

	testbreak();
	return block;
}

/* This is a good test to see if umask works */
int BasicUmask(void)
{
	FILE *fp;
	int passed;
	int block = SUCCESS;
	struct stat buf;
	mode_t oldmask;

	testbreak();

	/* test that we can create a file and it is writable by the owner */
	passed = expect_msk(SUCCESS, oldmask = umask_test(S_IRWXG | S_IRWXO));
	EXPECTED_RESP;
	passed = expect_ptr(SUCCESS, fp = tmpfile_test());
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_zng(SUCCESS, fstat_test(fileno(fp), &buf));
	EXPECTED_RESP;
	passed = expect_bol(SUCCESS, buf.st_mode & S_IRWXU);
	EXPECTED_RESP;
	passed = expect_msk(SUCCESS, umask_test(oldmask));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fclose_test(fp));
	EXPECTED_RESP;

	testbreak();

	/* test that we can create a file and it is NOT writable by the owner */
	passed = expect_msk(SUCCESS, oldmask = umask_test(S_IRWXU));
	EXPECTED_RESP;
	passed = expect_ptr(SUCCESS, fp = tmpfile_test());
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fstat_test(fileno(fp), &buf));
	EXPECTED_RESP;
	passed = expect_bol(FAILURE, buf.st_mode & S_IRWXU);
	EXPECTED_RESP;
	passed = expect_msk(SUCCESS, umask_test(oldmask));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, fclose_test(fp));
	EXPECTED_RESP;

	testbreak();
	return block;
}

int BasicGetSetlimit(void)
{
	int passed;
	int block = SUCCESS;
	struct rlimit orlim;
	struct rlimit nrlim;
	struct rlimit crlim;

	testbreak();

	passed = expect_zng(SUCCESS, getrlimit_test(&orlim));
	EXPECTED_RESP;
	nrlim.rlim_cur = 0; 
	nrlim.rlim_max = 0; /* This should always work even when hard limit is 0 */
	passed = expect_zng(SUCCESS, setrlimit_test(&nrlim));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, getrlimit_test(&crlim));
	EXPECTED_RESP;
	passed = expect_val(nrlim.rlim_cur, crlim.rlim_cur);
	EXPECTED_RESP;
	passed = expect_val(nrlim.rlim_max, crlim.rlim_max);
	EXPECTED_RESP;

	/* This can't be done by a non-super user */
/*	passed = expect_zng(SUCCESS, setrlimit_test(&orlim));*/
/*	EXPECTED_RESP;*/
/*	passed = expect_zng(SUCCESS, getrlimit_test(&crlim));*/
/*	EXPECTED_RESP;*/
/*	passed = expect_val(orlim.rlim_cur, crlim.rlim_cur);*/
/*	EXPECTED_RESP;*/
/*	passed = expect_val(orlim.rlim_max, crlim.rlim_max);*/
/*	EXPECTED_RESP;*/

	testbreak();
	return block;
}

int BasicGroups(void)
{
	int size;
	gid_t *groups = NULL;
	gid_t gid;
	int i;
	gid_t found = 0;
	int block = SUCCESS;
	int passed;

	testbreak();
	passed = expect_gid(SUCCESS, gid = getgid_test());
	EXPECTED_RESP;

	/* get the number of groups I care about */
	passed = expect_gez(SUCCESS, size = getgroups_test(0, groups));
	EXPECTED_RESP;

	/* score some memory for them */
	if (size != 0) {
		groups = (gid_t*)xmalloc(sizeof(gid_t) * size);
		/* fill the array */
		passed = expect_gez(SUCCESS, getgroups_test(size, groups));
		EXPECTED_RESP;

		/* find my gid in it, if not, then error */
		for (i = 0; i < size; i++) {
			if (gid == groups[i]) {
				found = gid;
				break;
			}	
		}

		free(groups);

		passed = expect_val(gid, found);
		EXPECTED_RESP;
	}

	testbreak();
	return block;
}

int BasicSync(void)
{
	int passed;
	int block = SUCCESS;

	testbreak();

	passed = expect_zng(SUCCESS, sync_test());
	EXPECTED_RESP;

	testbreak();
	return block;
}

int BasicName(void)
{
	int passed;
	int block = SUCCESS;
	struct utsname ut;
	char name[NAMEBUF] = {0};

	testbreak();

	passed = expect_gez(SUCCESS, uname_test(&ut));
	EXPECTED_RESP;
	testbreak();

#if !defined(LINUX) /* XXX a know problem with this call */
	passed = expect_zng(SUCCESS, gethostname_test(name, NAMEBUF));
	EXPECTED_RESP;
	testbreak();
#endif

#if !defined(Solaris)
	passed = expect_zng(SUCCESS, getdomainname_test(name, NAMEBUF));
	EXPECTED_RESP;
#endif

	testbreak();
	return block;
}

/* This is not checked over checkpoints */
int BasicTime(void)
{
	char tf[NAMEBUF] = {0};
	int block = SUCCESS;
	int passed;
	struct timeval tv;
	struct timezone tz;
	struct utimbuf ubuf;
	struct timeval ftv[2];
	struct stat buf;
	int fd;

	xtmpnam(tf);

	testbreak();
	passed = expect_zng(SUCCESS, gettimeofday_test(&tv, &tz));
	EXPECTED_RESP;

	/* make some files and check thier timestamps */
	passed = expect_gez(SUCCESS, 
				fd = open_test(tf,O_RDWR|O_TRUNC|O_CREAT,S_IRWXU));
	EXPECTED_RESP; IF_FAILED ABORT_TEST;
	passed = expect_gez(SUCCESS, write_test(fd, passage, strlen(passage)));
	EXPECTED_RESP;
	passed = expect_zng(SUCCESS, close_test(fd));
	EXPECTED_RESP;
	ftv[0].tv_sec = tv.tv_sec;
	ftv[0].tv_usec = tv.tv_usec;
	ftv[1].tv_sec = tv.tv_sec;
	ftv[1].tv_usec = tv.tv_usec;

	passed = expect_zng(SUCCESS, utimes_test(tf, ftv));
	EXPECTED_RESP;

	passed = expect_zng(SUCCESS, stat_test(tf, &buf));
	EXPECTED_RESP;
	/* be aware that gettimeofday might not be correct */
	if (buf.st_mtime < ftv[1].tv_sec)
	{
		printf("\tFailed Phase 2: utimes() gave strange timestamp on file\n");
		fflush(NULL);
		block = FAILURE;
	}

	ubuf.actime = tv.tv_sec;
	ubuf.modtime = tv.tv_sec;
	passed = expect_zng(SUCCESS, utime_test(tf, &ubuf));
	EXPECTED_RESP;

	passed = expect_zng(SUCCESS, stat_test(tf, &buf));
	EXPECTED_RESP;
	/* be aware that gettimeof day might not be correct */
	if (buf.st_mtime < tv.tv_sec)
	{
		printf("\tFailed Phase 2: utime() gave strange timestamp on file\n");
		fflush(NULL);
		block = FAILURE;
	}

	passed = expect_zng(SUCCESS, unlink_test(tf));
	EXPECTED_RESP;

	testbreak();
	return block;
}

int main(int argc, char **argv)
{
	int ret;
	int i;
	int whole_test = SUCCESS; /* did everything succeed? */

	/* place your new test in here in the order you'd like it run. */
	struct TestADT {
		int (*func)(void);
		char *desc;
	} tests[] = {
		{BasicFile, "BasicFile: simple open/close/access/unlink tests."},
		{BasicFileIO, "BasicFileIO: simple write/read/seek tests."},
/*		{BasicIOV, "BasicIOV: Basic vector reads and writes"},*/
		{BasicFreopen, "BasicFreopen: Does freopen return something sensible?"},
		{BasicStat, "BasicStat: Does [fs]tat return correct simple info?"},
		{BasicFilePerm, "BasicFilePerm: stat/chmod/fchmod"},
		{BasicUid, "BasicUid: validate uid/gid operations"},
		{BasicDup, "BasicDup: Does dup() work?"},
		{BasicFcntlDup, "BasicFcntlDup: Does fcntl() with F_DUPFD work?"},
		{BasicDir, "BasicDir: Can I make and remove a directory?"},
		{BasicChdir, "BasicChdir: Can I validly change directories?"},
		{BasicFchdir, "BasicFchdir: Can I validly change directories?"},
		{BasicMknod, "BasicMknod: Can I make pipes and not other stuff?"},
		{BasicLink, "BasicLink: (Sym|Hard)link testing with lchown/lstat()"},
		{BasicRename, "BasicRename: Does rename() work?"},
		{BasicTruncation, "BasicTruncation: Does f?truncate() work?"},

#if defined(Solaris) || defined(IRIX65)
		{BasicFcntlTruncation, "BasicFcntlTruncation: Does F_FREESP work?"},
#endif
		{BasicUmask, "BasicUmask: Does umask() work?"},
		{BasicGroups, "BasicGroups: Does getgroups() work?"},
		{BasicSync, "BasicSync: Can I sync() the disk?"},
		{BasicName, "BasicName: Do I know my own name?"},
/*		{BasicTime, "BasicTime: Do I know what time it is?"},*/
		{BasicGetSetlimit, "BasicGetSetLimit: Can I change proc limits?"},
	};

	printf("Condor System Call Tester $Revision: 1.2.2.2 $\n\n");

	printf("The length of the string:\n'%s'\nIs: %d\n\n", 
		STR(passage), strlen(passage));

	/* perform all of the tests in the order given */
	for (i = 0; i < sizeof tests / sizeof tests[0]; i++)
	{
		printf("Beginning Test: [%s]\n", STR(tests[i].desc));
		fflush(NULL);

		/* run the test block */
		ret = tests[i].func();

		/* see if the block failed or not */
		if (ret == SUCCESS) { 
			printf("Succeeded Phase 3\n"); 
			fflush(NULL); 
		} else {
			printf("Failed Phase 3\n");
			fflush(NULL);
			whole_test = FAILURE;
		}

		printf("Ending Test: [%s]\n", STR(tests[i].desc)); 
		printf("\n");
		fflush(NULL);
	}

	printf("%s Phase 4\n", whole_test==SUCCESS?"Succeeded":"Failed");

	fflush(NULL);
	return whole_test;
}

	/* Test the stuff you don't need a file descriptor for */
/*	printf("BEGIN Test %d\n", test);*/
/*	handle(	setregid_test()			);*/
/*	handle(	setreuid_test()			);*/
/*	printf("END Test %d\n\n", test);*/
/*	test++;*/

