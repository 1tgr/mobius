#ifndef __KERNEL_HANDLE_H
#define __KERNEL_HANDLE_H

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct handle_t handle_t;
struct handle_t
{
	handle_t *next;
	bool signal;
	byte refs;
	struct process_t* process;
	const char* file;
	int line;
};

void* _hndAlloc(size_t size, struct process_t* proc, const char* file, int line);
#define hndAlloc(size, proc)	_hndAlloc(size, proc, __FILE__, __LINE__);

int hndAddRef(void* buf);
int hndFree(void* buf);
void hndSignal(void* buf, bool signal);
bool hndIsSignalled(void* buf);
void hndEnum(struct process_t* proc);

#define hndHandle(buf)	((handle_t*) (buf) - 1)

#ifdef __cplusplus
}
#endif

#endif