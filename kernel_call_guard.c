/* x86 call boundary for migrated native calls. The exception registration
 * belongs to this calling thread and exists only during the guarded call.
 * Unlike a process-wide VEH, it cannot consume another thread's exception.
 * Native calls can hold locks or partially mutate globals when they fault.
 * Recognized faults terminate this client; they NEVER become ordinary false
 * returns or attempt to release unknown locks. No logger/allocator is entered.
 * Include in Kernel33's translation unit; compile/execute offline fixtures
 * before enabling any migration binder. */
typedef struct {
    EXCEPTION_REGISTRATION_RECORD registration;
    DWORD saved_ebp;
} KrcExceptionFrame;
void KRC_CallFailed(void);
int __cdecl KRC_CallGuarded(uint32_t argument,uint32_t function,unsigned convention);

static __attribute__((used)) EXCEPTION_DISPOSITION __cdecl KrcCallException(
    EXCEPTION_RECORD *record,void *frame,CONTEXT *context,void *dispatcher)
{
    (void)dispatcher;(void)frame;(void)context;
    if(record->ExceptionFlags || (record->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION &&
                                 record->ExceptionCode!=EXCEPTION_IN_PAGE_ERROR))
        return ExceptionContinueSearch;
    TerminateProcess(GetCurrentProcess(),0x4b524301u);
    /* TerminateProcess(self) does not return on success. A fail-fast trap is
     * the fallback if the API unexpectedly fails; never resume the game. */
    __asm__ volatile("int $0x29" : : "c"(7) : "memory");
    __builtin_unreachable();
}

/* convention: 0=thiscall byte result; 1=stdcall BOOL; 2=stdcall void;
 * 3=cdecl pointer result; 4=engine AddText (EAX=text, ECX=local-client0).
 * Saved EBX/ESI/EDI/EBP and caller stack survive ordinary native returns.
 * Invalid convention returns zero without calling native. Faults terminate. */
__asm__(
    ".text\n"
    ".globl _KRC_CallGuarded\n"
    "_KRC_CallGuarded:\n"
    " pushl %ebp\n"
    " movl %esp,%ebp\n"
    " pushl %ebx\n"
    " pushl %esi\n"
    " pushl %edi\n"
    " subl $12,%esp\n"
    " movl %fs:0,%eax\n"
    " movl %eax,(%esp)\n"
    " movl $_KrcCallException,4(%esp)\n"
    " movl %ebp,8(%esp)\n"
    " movl %esp,%fs:0\n"
    " movl 16(%ebp),%edx\n"
    " cmpl $4,%edx\n"
    " ja _KRC_CallFailed\n"
    " cmpl $4,%edx\n"
    " je 4f\n"
    " cmpl $3,%edx\n"
    " je 3f\n"
    " testl %edx,%edx\n"
    " je 0f\n"
    " pushl 8(%ebp)\n"
    " call *12(%ebp)\n"
    " cmpl $2,16(%ebp)\n"
    " jne 5f\n"
    " movl $1,%eax\n"
    " jmp 5f\n"
    "0: movl 8(%ebp),%ecx\n"
    " call *12(%ebp)\n"
    " movzbl %al,%eax\n"
    " jmp 5f\n"
    "3: pushl 8(%ebp)\n"
    " call *12(%ebp)\n"
    " addl $4,%esp\n"
    " jmp 5f\n"
    "4: movl 8(%ebp),%eax\n"
    " xorl %ecx,%ecx\n"
    " call *12(%ebp)\n"
    " movl $1,%eax\n"
    " jmp 5f\n"
    ".globl _KRC_CallFailed\n"
    "_KRC_CallFailed:\n"
    " xorl %eax,%eax\n"
    "5: movl (%esp),%edx\n"
    " movl %edx,%fs:0\n"
    " addl $12,%esp\n"
    " popl %edi\n"
    " popl %esi\n"
    " popl %ebx\n"
    " leave\n"
    " ret\n");
