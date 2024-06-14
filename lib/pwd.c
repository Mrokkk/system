#include <pwd.h>
#include <string.h>

static struct passwd passwd = {
    .pw_name = "root",
    .pw_uid = 0,
    .pw_gid = 0,
    .pw_dir = "/root",
    .pw_shell = "/bin/sh"
};

struct passwd* LIBC(getpwnam)(const char* name)
{
    VALIDATE_INPUT(name, NULL);
    return &passwd;
}

struct passwd* LIBC(getpwuid)(uid_t)
{
    return &passwd;
}

int LIBC(getpwnam_r)(
    const char* restrict name,
    struct passwd* restrict pwd,
    char* buf,
    size_t buflen,
    struct passwd** restrict result)
{
    VALIDATE_INPUT(name && pwd && buf && buflen, 0);
    memcpy(buf, &passwd, buflen);
    *result = (void*)buf;
    return 0;
}

int LIBC(getpwuid_r)(
    uid_t,
    struct passwd* restrict pwd,
    char* buf,
    size_t buflen,
    struct passwd** restrict result)
{
    VALIDATE_INPUT(pwd && buf && buflen, 0);
    memcpy(buf, &passwd, buflen);
    *result = (void*)buf;
    return 0;
}

void LIBC(endpwent)(void)
{
    NOT_IMPLEMENTED_NO_RET_NO_ARGS();
}

struct passwd* LIBC(getpwent)(void)
{
    return &passwd;
}

void LIBC(setpwent)(void)
{
    NOT_IMPLEMENTED_NO_RET_NO_ARGS();
}

LIBC_ALIAS(getpwnam);
LIBC_ALIAS(getpwuid);
LIBC_ALIAS(getpwnam_r);
LIBC_ALIAS(getpwuid_r);
LIBC_ALIAS(endpwent);
LIBC_ALIAS(getpwent);
LIBC_ALIAS(setpwent);
