# 1 "sysdef.c"


# 1 "../../include/os/sysdef.h" 1
 
# 13 "../../include/os/sysdef.h"










 




























































 
 
 

 
 
 int    DbgWrite (const wchar_t*, size_t ); 
 int    Hello (int, int ); 
 unsigned    SysUpTime (void ); 
 bool    SysGetInfo (struct sysinfo_t * ); 
 bool    SysGetTimes (struct systimes_t * ); 
 bool    SysShutdown (unsigned ); 
 void    KeLeakBegin (void ); 
 void    KeLeakEnd (void ); 
 void    SysYield (void ); 
 

 
 
 void    ThrExitThread (int ); 
 bool    ThrWaitHandle (handle_t ); 
 void    ThrSleep (unsigned ); 
 handle_t    ThrCreateV86Thread (uint32_t, uint32_t, unsigned, void (*)(void) ); 
 bool    ThrGetV86Context (struct context_v86_t* ); 
 bool    ThrSetV86Context (const struct context_v86_t* ); 
 bool    ThrContinueV86 (void ); 
 handle_t    ThrCreateThread (void (*)(void), void*, unsigned ); 
 

 
 
 void    ProcExitProcess (int ); 
 handle_t    ProcSpawnProcess (const wchar_t*, const struct process_info_t * ); 
 

 
 
 handle_t    FsCreate (const wchar_t*, uint32_t ); 
 handle_t    FsOpen (const wchar_t*, uint32_t ); 
 bool    FsClose (handle_t ); 
 bool    FsRead (handle_t, void*, size_t, struct fileop_t* ); 
 bool    FsWrite (handle_t, const void*, size_t, struct fileop_t* ); 
 off_t    FsSeek (handle_t, off_t, unsigned ); 
 handle_t    FsOpenSearch (const wchar_t* ); 
 bool    FsQueryFile (const wchar_t*, uint32_t, void*, size_t ); 
 bool    FsRequestSync (handle_t, uint32_t, void*, size_t, struct fileop_t* ); 
 bool    FsIoCtl (handle_t, uint32_t, void *, size_t, struct fileop_t* ); 
 

 
 
 void *    VmmAlloc (size_t, addr_t, uint32_t ); 
 bool    VmmFree (void* ); 
 

 
 
 handle_t    EvtAlloc (void ); 
 bool    HndClose (handle_t ); 
 void    EvtSignal (handle_t ); 
 bool    EvtIsSignalled (handle_t ); 
 

 
 
 handle_t    WndCreate (handle_t, const struct wndattr_t *, unsigned ); 
 bool    WndClose (handle_t ); 
 bool    WndPostMessage (handle_t, const struct msg_t * ); 
 bool    WndInvalidate (handle_t, const struct MGLrect * ); 
 bool    WndGetMessage (struct msg_t* ); 
 bool    WndGetAttribute (handle_t, uint32_t, uint32_t, void *, size_t* ); 
 bool    WndSetAttribute (handle_t, uint32_t, uint32_t, const void *, size_t ); 
 handle_t    WndOwnRoot (void ); 
 bool    WndQueueInput (const struct wndinput_t* ); 
 bool    WndSetFocus (handle_t ); 
 bool    WndHasFocus (handle_t ); 
 bool    WndSetCapture (handle_t, bool ); 
 bool    WndGetClip (handle_t, struct MGLrect *, size_t* ); 
 

 

# 175 "../../include/os/sysdef.h"

# 3 "sysdef.c" 2

