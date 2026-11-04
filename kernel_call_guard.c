/* x86 call boundary for migrated native readers. The exception registration
 * belongs to this calling thread and exists only during the guarded call.
 * Unlike a process-wide VEH, it cannot consume another thread's exception.
 * No allocation, log, input, network or process-lifetime handler is introduced.
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
    (void)dispatcher;
    if(record->ExceptionFlags || (record->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION &&
                                 record->ExceptionCode!=EXCEPTION_IN_PAGE_ERROR))
        return ExceptionContinueSearch;
    context->Eip=(DWORD)(uintptr_t)KRC_CallFailed;
    context->Esp=(DWORD)(uintptr_t)frame;
    context->Ebp=((KrcExceptionFrame *)frame)->saved_ebp;
    return ExceptionContinueExecution;
}

/* convention: 0=thiscall byte result; 1=stdcall BOOL; 2=stdcall void;
 * 3=cdecl pointer result; 4=engine AddText (EAX=text, ECX=local-client0).
 * Saved EBX/ESI/EDI/EBP and the original caller stack survive both paths.
 * The handler restores ESP to the registration record before this epilogue. */
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
