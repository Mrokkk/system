#include <kernel/process.h>
#include <kernel/test.h>
#include <kernel/module.h>
#include <kernel/unistd.h>

TEST_PLAN(processes)

    TEST_SUITE(kernel_process) {
        struct process *proc = 0;
        int child_pid, status;
        ASSERT_EQ(isKernelProcess, init_process.type, KERNEL_PROCESS);
        if ((child_pid = fork()) == 0) {
            while (1);
        }
        ASSERT_GT(isntPidNull, child_pid, 0);
        proc = process_find(child_pid);
        ASSERT_NEQ(ifProcessStructExists, proc, 0);
        process_exit(proc);
        ASSERT_NEQ(isntProcessZombie, proc->stat, 0);
        waitpid(child_pid, &status, 0);
        ASSERT_EQ(isProcessZombie, proc->stat, 0);
    }

TEST_PLAN_END(processes)

void tests_run() {

    test_plan_processes();

}

