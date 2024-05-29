#pragma once

#include <sys/types.h>

// https://pubs.opengroup.org/onlinepubs/009695399/basedefs/pwd.h.html

struct passwd
{
    char*   pw_name;  // User's login name
    uid_t   pw_uid;   // Numerical user ID
    gid_t   pw_gid;   // Numerical group ID
    char*   pw_dir;   // Initial working directory
    char*   pw_shell; // Program to use as shell
};

struct passwd* getpwnam(const char* name);
struct passwd* getpwuid(uid_t uid);

int getpwnam_r(
    const char* restrict name,
    struct passwd* restrict pwd,
    char* buf,
    size_t buflen,
    struct passwd** restrict result);

int getpwuid_r(
    uid_t uid,
    struct passwd* restrict pwd,
    char* buf,
    size_t buflen,
    struct passwd** restrict result);

void endpwent(void);
struct passwd* getpwent(void);
void setpwent(void);
