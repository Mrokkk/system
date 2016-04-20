#ifndef __STDDEF_H_
#define __STDDEF_H_

/* Non-POSIX... */

#ifndef NULL
#define NULL ((void *)0)
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

#endif /* __STDDEF_H_ */
