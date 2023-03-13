#pragma once

#define TRACE_WAIT_QUEUE    0
#define TRACE_PAGE          0
#define TRACE_KMALLOC       1
#define TRACE_FMALLOC       0
#define TRACE_PAGE_DETAILED 1
#define TRACE_VM            0

#define crash() { int* i = (int*)0; *i = 2137; }
