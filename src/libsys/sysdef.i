/*!
 *	\ingroup	sys
 *	@{
 */

int    DbgWrite (const wchar_t*, size_t ); 
int    Hello (int, int ); 
unsigned    SysUpTime (void ); 

void    ThrExitThread (int ); 
bool    ThrWaitHandle (handle_t ); 
void    ThrSleep (unsigned ); 

void    ProcExitProcess (int ); 

handle_t    FsCreate (const wchar_t*, uint32_t ); 
handle_t    FsOpen (const wchar_t*, uint32_t ); 
bool    FsClose (handle_t ); 
bool    FsRead (handle_t, void*, size_t, struct fileop_t* ); 
bool    FsWrite (handle_t, const void*, size_t, struct fileop_t* ); 
addr_t    FsSeek (handle_t, addr_t ); 

void *    VmmAlloc (size_t, addr_t, uint32_t ); 

handle_t    EvtAlloc (void ); 
bool    EvtFree (handle_t ); 
void    EvtSignal (handle_t ); 
bool    EvtIsSignalled (handle_t ); 

/*! @} */
