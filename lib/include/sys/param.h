#pragma once

#include <limits.h>

#define NBBY		CHAR_BIT

#if !defined NGROUPS && defined NGROUPS_MAX
# define NGROUPS	NGROUPS_MAX
#endif
#if !defined MAXSYMLINKS && defined SYMLOOP_MAX
# define MAXSYMLINKS	SYMLOOP_MAX
#endif
#if !defined CANBSIZ && defined MAX_CANON
# define CANBSIZ	MAX_CANON
#endif
#if !defined MAXPATHLEN && defined PATH_MAX
# define MAXPATHLEN	PATH_MAX
#endif
#if !defined NOFILE && defined OPEN_MAX
# define NOFILE		OPEN_MAX
#endif
#if !defined MAXHOSTNAMELEN && defined HOST_NAME_MAX
# define MAXHOSTNAMELEN	HOST_NAME_MAX
#endif
#ifndef NCARGS
# ifdef ARG_MAX
#  define NCARGS	ARG_MAX
# else
/* ARG_MAX is unlimited, but we define NCARGS for BSD programs that want to
   compare against some fixed limit.  */
# define NCARGS		INT_MAX
# endif
#endif

/* Magical constants.  */
#ifndef NOGROUP
# define NOGROUP	65535     /* Marker for empty group set member.  */
#endif
#ifndef NODEV
# define NODEV		((dev_t) -1)    /* Non-existent device.  */
#endif

/* Unit of `st_blocks'.  */
#ifndef DEV_BSIZE
# define DEV_BSIZE	512
#endif

/* Bit map related macros.  */
#define setbit(a,i)     ((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define clrbit(a,i)     ((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define isset(a,i)      ((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define isclr(a,i)      (((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

/* Macros for counting and rounding.  */
#ifndef howmany
# define howmany(x, y)  (((x) + ((y) - 1)) / (y))
#endif
#ifdef __GNUC__
# define roundup(x, y)  (__builtin_constant_p (y) && powerof2 (y)             \
                         ? (((x) + (y) - 1) & ~((y) - 1))                     \
                         : ((((x) + ((y) - 1)) / (y)) * (y)))
#else
# define roundup(x, y)  ((((x) + ((y) - 1)) / (y)) * (y))
#endif
#define powerof2(x)     ((((x) - 1) & (x)) == 0)

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
