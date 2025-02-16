#include "kernel.h"
#include "common.h"

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef uint32_t size_t;

// Addresses of symbols defined in the linker script 
extern char __bss[], __bss_end[], __stack_top[];
extern char __free_ram[], __free_ram_end[];
extern char __kernel_base[];

// Symbols from the embedded raw binary of the user shell
extern char _binary_shell_bin_start[], _binary_shell_bin_size[];

struct process process_list[PROCS_MAX];

void man_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags);

// Call OpenSBI via the ecall instruction
struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                       long arg5, long fid, long eid) {
    // Ask the compiler to place values in the specified registers. 
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;

    __asm__ __volatile__("ecall"
                         : "=r"(a0), "=r"(a1)
                         : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                           "r"(a6), "r"(a7)
                         : "memory");
    return (struct sbiret){.error = a0, .value = a1};
}

__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    __asm__ __volatile__(
        // Retrieve the kernel stack of the running process from sscratch.
        "csrrw sp, sscratch, sp\n"
        
        "addi sp, sp, -4 * 31\n"
        "sw ra,  4 * 0(sp)\n"
        "sw gp,  4 * 1(sp)\n"
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"

        // Retrieve and save the sp at the time of exception.
        "csrr a0, sscratch\n"
        "sw a0, 4 * 30(sp)\n"

        // Reset the kernel stack.
        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n"

        "mv a0, sp\n"
        "call handle_trap\n"

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n"
    );
}

__attribute__((naked))
void switch_context(uint32_t *prev_sp, uint32_t *next_sp) {
    __asm__ __volatile__(
        // Save callee-saved registers onto the current process's stack.
        "addi sp, sp, -13 * 4\n" // Allocate stack space for 13 4-byte registers
        "sw ra,  0  * 4(sp)\n"   // Save callee-saved registers only
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        // Switch the stack pointer.
        "sw sp, (a0)\n"         // *prev_sp = sp;
        "lw sp, (a1)\n"         // Switch stack pointer (sp) here

        // Restore callee-saved registers from the next process's stack.
        "lw ra,  0  * 4(sp)\n"  // Restore callee-saved registers only
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n"  // We've popped 13 4-byte registers from the stack
        "ret\n"
    );
}

// Implement Bump allocator or Linear allocator, assuming that deallocation is not necessary.
// For deallocation, use a bitmap-based algorithm or an algorithm called the buddy system.
paddr_t alloc_pages(uint32_t n) {
    static paddr_t next_paddr = (paddr_t) __free_ram;
    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE;

    if (next_paddr > (paddr_t) __free_ram_end) {
        PANIC("Out of memory");
    }

    memset((void *) paddr, 0, n * PAGE_SIZE);
    return paddr;
}

__attribute__((naked)) void user_entry(void) {
    __asm__ __volatile__(
        "csrw sepc, %[sepc]         \n"
        "csrw sstatus, %[sstatus]    \n"
        "sret                       \n"
        :
        : [sepc] "r" (USER_BASE),
          [sstatus] "r" (SSTATUS_SPIE)
    );
}

/**
 * @param image the pointer to the execution image
 * @param image_size the image size
 */
struct process *create_process(const void *image, size_t image_size) {
    struct process *proc = NULL;
    int i = 0;

    for (; i < PROCS_MAX; ++i) {
        if (process_list[i].state == PROC_UNUSED) {
            proc = &process_list[i];
            break;
        }
    }

    if (!proc) {
        PANIC("No free process slots");
    }

    // Stack callee-saved registers. These register values will be restored in
    // the first context switch in switch_context.
    uint32_t *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                      // s11
    *--sp = 0;                      // s10
    *--sp = 0;                      // s9
    *--sp = 0;                      // s8
    *--sp = 0;                      // s7
    *--sp = 0;                      // s6
    *--sp = 0;                      // s5
    *--sp = 0;                      // s4
    *--sp = 0;                      // s3
    *--sp = 0;                      // s2
    *--sp = 0;                      // s1
    *--sp = 0;                      // s0
    // Jump destination for the first context switch to user_entry
    *--sp = (uint32_t) user_entry;  // ra

    // Map kernel pages.
    uint32_t *page_table = (uint32_t *) alloc_pages(1);
    for (paddr_t paddr = (paddr_t) __kernel_base;
         paddr < (paddr_t) __free_ram_end;
         paddr += PAGE_SIZE) {
            man_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);
    }

    // Map user pages
    for (uint32_t off = 0; off < image_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);
        // Handle the case where the data to be copied is smaller than page size.
        size_t remaining = image_size - off;
        size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;

        // Fill and map the page.
        memcpy((void*) page, image + off, copy_size);
        man_page(page_table, USER_BASE + off, page, PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }

    // Initialize fields.
    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->page_table = page_table;
    proc->sp = (uint32_t) sp;

    return proc;
}

void delay(void) {
    for (int i = 0; i < 300000000; i++) {
        __asm__ __volatile__("nop"); // do nothing
    }
}

struct process *current_proc; // Currently running process
struct process *idle_proc; // Idle process

// Scheduler
void yield(void) {
    // Search for a runnable process
    struct process *next = idle_proc;
    for (int i = 0; i < PROCS_MAX; ++i) {
        struct process *proc = &process_list[(current_proc->pid + i) % PROCS_MAX];
        if (proc->state == PROC_RUNNABLE && proc->pid > 0) {
            next = proc;
            break;
        }
    }

    // No runnable process other than the current one, return and continue processing
    if (next == current_proc) {
        return;
    }

    __asm__ __volatile__ (
        "sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"
        "csrw sscratch, %[sscratch]\n"
        :
        : [satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)), 
          [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
    );

    // Context switch
    struct process *prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}

struct process *proc_a;
struct process *proc_b;

void proc_a_entry(void) {
    printf("Starting process A\n");
    while (true) {
        putchar('A');
        delay();
        yield();
    }
}

void proc_b_entry(void) {
    printf("Starting process B\n");
    while (true) {
        putchar('B');
        delay();
        yield();
    }
}

void handle_syscall(struct trap_frame *f) {
    switch (f->a3)
    {
    case SYS_PUTCHAR:
        putchar(f->a0);
        break;
    case SYS_GETCHAR:
        while (1) {
            long ch = getchar();
            if (ch >= 0) {
                f->a0 = ch;
                break;
            }

            yield();
        }
        break;
    case SYS_EXIT:
        printf("process %d exited\n", current_proc->pid);
        current_proc->state = PROC_EXITED;
        yield();
        PANIC("unreachable");
        break;
    default:
        PANIC("unexpected syscall a3=%x\n", f->a3);
    }
}

void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);

    if (scause == SCAUSE_ECALL) {
        handle_syscall(f);
        // We add the size of ecall instaction (4) to the value of sepc (program counter),
        // in order to move over the exception cause and avoid infinite loop
        // upon return from the exception handler
        user_pc += 4;
    } else {
        PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
    }

    WRITE_CSR(sepc, user_pc);
}

void putchar(char ch) {
    sbi_call(ch, 0, 0, 0, 0, 0, 0, 1/* Console Putchar */);
}

long getchar(void) {
    struct sbiret ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2);
    return ret.error;
}

void kernel_main(void) {
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss);

    // Tell the CPU where the exception handler is located
    WRITE_CSR(stvec, (uint32_t) kernel_entry);

    printf("\n\nHello %s\n", "World!");

    idle_proc = create_process(NULL, 0);
    idle_proc->pid = -1; // idle
    current_proc = idle_proc;

    create_process(_binary_shell_bin_start, (size_t) _binary_shell_bin_size);

    yield();
    PANIC("switched to idle process");

    for (;;) {
        __asm__ __volatile__("wfi");
    }
}

// The __attribute__((section(".text.boot"))) attribute
// controls the placement of the function in the linker script.
// Since OpenSBI simply jumps to 0x80200000 without knowing the entry point,
// the boot function needs to be placed at 0x80200000.

// The __attribute__((naked)) attribute instructs the compiler not to
// generate unnecessary code before and after the function body,
// such as a return instruction. This ensures that the inline assembly code
// is the exact function body.
__attribute__((section(".text.boot")))
__attribute__((naked))
void boot(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n" // Set the stack pointer
        "j kernel_main\n"       // Jump to the kernel main function
        :
        : [stack_top] "r" (__stack_top) // Pass the stack top address as %[stack_top]
    );
}

// Prepare the second-level page table and fill the page table entry in the second level
// Virtual address is split into vpn[1] (1st level), vpn[0] (2nd level), offset
// @param table1: The first-level page table
// @param vaddr: The virtual address
// @param paddr: The physical address
// @param flags: The page table entry flags
void man_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags) {
    if (!is_aligned(vaddr, PAGE_SIZE)) {
        PANIC("unaligned virtual address %x", vaddr);
    }

    if (!is_aligned(paddr, PAGE_SIZE)) {
        PANIC("unaligned physical address %x", paddr);
    }

    uint32_t vpn1 = (vaddr >> 22) & 0x3ff;

    if ((table1[vpn1] & PAGE_V) == 0) {
        // Create the non-existent 2nd level page table.
        uint32_t pt_paddr = alloc_pages(1);
        // Divide paddr by PAGE_SIZE because the entry should contain the physical page number, not the physical address itself
        table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V;
    }

    // Set 2nd level page table entry to map the physical page.
    uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
    uint32_t *table0 = (uint32_t *) ((table1[vpn1] >> 10) * PAGE_SIZE);
    table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V;
}
