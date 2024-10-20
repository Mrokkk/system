#pragma once

#define IA32_MSR_EFER       0xc0000080
#define IA32_MSR_EFER_SCE   (1 << 0)
#define IA32_MSR_EFER_LME   (1 << 8)
#define IA32_MSR_EFER_NXE   (1 << 11)
