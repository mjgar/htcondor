#ifndef FIX_TYPES_H
#define FIX_TYPES_H

	 /*
	 OSF/1 has this as an "unsigned long", but this is incorrect.  It
	 is used by lseek(), and must be able to hold negative values.
	 */
#if defined(OSF1) && !defined(__GNUC__)
#define off_t _hide_off_t
#endif

#if defined(OSF1)
/* We need to define _OSF_SOURCE so that type quad, and
   u_int and friends get defined.  We should leave it on since we'll
   need it later as well.*/
#define _OSF_SOURCE
#endif

#if	defined(ULTRIX43)
#define key_t       long
typedef int		bool_t;
#endif

/* define __EXTENSIONS__ on suns so u_int & friends get defined */
#if defined(Solaris) || defined(SUNOS41)
#define __EXTENSIONS__
#endif /* Solaris */

/* for IRIX62, we want _BSD_TYPES defined when we include sys/types.h, but
 * then we want to set it back to how it was. -Todd, 1/31/97
 */
#if defined(IRIX62)
#	ifdef _BSD_TYPES
#		include <sys/types.h>
#	else
#		define _BSD_TYPES
#		include <sys/types.h>
#		undef _BSD_TYPES
#	endif
#endif

#include <sys/types.h>


#if defined(OSF1) && !defined(__GNUC__)
#undef off_t
typedef long off_t;
#endif

/*
The system include file defines this in terms of bzero(), but ANSI says
we should use memset().
*/
#if defined(OSF1)
#undef FD_ZERO
#define FD_ZERO(p)     memset((char *)(p), 0, sizeof(*(p)))
#endif

/*
Various non-POSIX conforming files which depend on sys/types.h will
need these extra definitions...
*/

#if defined(HPUX9) || defined(LINUX) || defined(Solaris) || defined(IRIX53) || defined(SUNOS41) || defined(OSF1)
#	define HAS_U_TYPES
#endif

#if !defined(HAS_U_TYPES)
	typedef unsigned int u_int;
	typedef unsigned char   u_char;
	typedef unsigned short  u_short;
	typedef unsigned long   u_long;
#endif

#if defined(AIX32)
typedef unsigned short ushort;
#endif

#if defined(ULTRIX42) ||  defined(ULTRIX43)
typedef char * caddr_t;
#endif

#if defined(AIX32)
typedef unsigned long rlim_t;
#endif

#if defined(LINUX)
typedef long rlim_t;
#endif

/* On IRIX, we need to compile senders.c with the stupid native CC compiler
 * because gcc/g++ is rather broken on this platform.  BUT, CC does not have
 * bool defined, so do it here. */
#if !defined(__GNUC__) && defined(IRIX53)
enum bool {false, true};
#endif

#endif





