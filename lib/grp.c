#include <grp.h>

static struct group root_group = {
    .gr_name = "root",
    .gr_passwd = "",
    .gr_gid = 0,
    .gr_mem = NULL,
};

struct group* LIBC(getgrent)(void)
{
    NOT_IMPLEMENTED_NO_ARGS(NULL);
}

void LIBC(setgrent)(void)
{
    NOT_IMPLEMENTED_NO_RET_NO_ARGS();
}

void LIBC(endgrent)(void)
{
    NOT_IMPLEMENTED_NO_RET_NO_ARGS();
}

struct group* LIBC(getgrnam)(char const* name)
{
    NOT_IMPLEMENTED(&root_group, "\"%s\"", name);
}

struct group* LIBC(getgrgid)(gid_t gid)
{
    NOT_IMPLEMENTED(&root_group, "%u", gid);
}

LIBC_ALIAS(getgrent);
LIBC_ALIAS(setgrent);
LIBC_ALIAS(endgrent);
LIBC_ALIAS(getgrnam);
LIBC_ALIAS(getgrgid);
