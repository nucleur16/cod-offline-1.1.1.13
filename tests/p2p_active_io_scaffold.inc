/* Deterministic overlapped OS double: cancellation must drain before close. */
typedef struct { HANDLE hEvent; } OVERLAPPED;
#define ERROR_IO_PENDING 997u
#define WAIT_OBJECT_0 0u
#define ZeroMemory(p,n) memset(p,0,n)
static int io_mode,io_pending,io_cancels,io_drains,io_completes,io_waits;
static OVERLAPPED *io_operation;
static DWORD io_size;
static HANDLE CreateEventA(void *p,BOOL a,BOOL b,const char *name){(void)p;(void)a;(void)b;(void)name;assert(!io_pending);return(HANDLE)77;}
static DWORD GetLastError(void){return ERROR_IO_PENDING;}
static BOOL ReadFile(HANDLE h,void *p,DWORD n,DWORD *got,OVERLAPPED *op)
{(void)h;(void)p;*got=0;assert(!io_pending);io_pending=1;io_size=n;io_operation=op;return FALSE;}
static BOOL WriteFile(HANDLE h,void *p,DWORD n,DWORD *got,OVERLAPPED *op){return ReadFile(h,p,n,got,op);}
static DWORD WaitForMultipleObjects(DWORD n,const HANDLE *handles,BOOL all,DWORD timeout)
{
 (void)n;(void)handles;(void)all;(void)timeout;assert(io_pending);++io_waits;
 if(io_mode==1){clock_ms+=6000;return WAIT_TIMEOUT;}
 if(io_mode==2){parent_alive=0;return 1;}
 return WAIT_OBJECT_0;
}
static BOOL CancelIoEx(HANDLE h,OVERLAPPED *op){(void)h;assert(io_pending&&op==io_operation);++io_cancels;return TRUE;}
static BOOL GetOverlappedResult(HANDLE h,OVERLAPPED *op,DWORD *got,BOOL wait)
{
 (void)h;assert(io_pending&&op==io_operation);io_pending=0;
 if(wait){assert(io_cancels&&principal_faulted());++io_drains;*got=0;return FALSE;}
 ++io_completes;*got=io_size>3?3:io_size;return TRUE;
}
