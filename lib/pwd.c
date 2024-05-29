#include <pwd.h>
#include <errno.h>
#include <string.h>

static struct passwd passwd = {
    .pw_name = "root",
    .pw_uid = 0,
    .pw_gid = 0,
    .pw_shell = "/bin/sh"
};

struct passwd* getpwnam(const char* name)
{
    VALIDATE_INPUT(name, NULL);
    return &passwd;
}

struct passwd* getpwuid(uid_t)
{
    return &passwd;
}

int getpwnam_r(
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

int getpwuid_r(
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

void endpwent(void)
{
}

struct passwd* getpwent(void)
{
    return &passwd;
}

void setpwent(void)
{
}
