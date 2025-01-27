#pragma once

#include <stdint.h>
#include <signal.h>

typedef uintptr_t greg_t;

enum
{
  REG_GS = 0,
#define REG_GS     REG_GS
  REG_FS,
#define REG_FS     REG_FS
  REG_ES,
#define REG_ES     REG_ES
  REG_DS,
#define REG_DS     REG_DS
  REG_EDI,
#define REG_EDI    REG_EDI
  REG_ESI,
#define REG_ESI    REG_ESI
  REG_EBP,
#define REG_EBP    REG_EBP
  REG_ESP,
#define REG_ESP    REG_ESP
  REG_EBX,
#define REG_EBX    REG_EBX
  REG_EDX,
#define REG_EDX    REG_EDX
  REG_ECX,
#define REG_ECX    REG_ECX
  REG_EAX,
#define REG_EAX    REG_EAX
  REG_TRAPNO,
#define REG_TRAPNO REG_TRAPNO
  REG_ERR,
#define REG_ERR    REG_ERR
  REG_EIP,
#define REG_EIP    REG_EIP
  REG_CS,
#define REG_CS     REG_CS
  REG_EFL,
#define REG_EFL    REG_EFL
  REG_UESP,
#define REG_UESP   REG_UESP
  REG_SS
#define REG_SS     REG_SS
};

#define __NGREG (REG_SS + 1)
#define NGREG __NGREG

typedef struct mcontext_t
{
    greg_t gregs[NGREG];
} mcontext_t;

typedef struct ucontext_t
{
    unsigned long int  uc_flags;
    struct ucontext_t* uc_link;
    sigset_t           uc_sigmask;
    mcontext_t         uc_mcontext;
} ucontext_t;
