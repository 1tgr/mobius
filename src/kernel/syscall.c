#include <kernel/kernel.h>
#include <kernel/thread.h>
#include <kernel/sys.h>
#include <kernel/vmm.h>
#include <kernel/obj.h>
#include <kernel/fs.h>
#include <kernel/port.h>
#include <errno.h>

volatile int syscall_level;
extern tss_t tss;

thread_t* sysCreateThread(addr_t entry, dword param, unsigned priority)
{
	thread_t* thr;
	context_t* ctx;
	dword* dw;

	thr = thrCreate(current->process->level, current->process, 
		(const void*) entry, priority);
	if (thr == NULL)
		return NULL;

	ctx = thrContext(thr);
	dw = (dword*) ctx->esp;
	dw[1] = 0;
	dw[2] = param;
	ctx->esp += sizeof(dword);
	thrSuspend(thr, false);
	thrSchedule();
	return thr;
}

void syscall(context_t* ctx)
{
	//dword code = ctx->regs.eax;
	
	switch (ctx->regs.eax)
	{
	case 0:
		ctx->regs.eax = (dword)
			thrCreate86(current->process, (const byte*) ctx->regs.ebx, ctx->regs.ecx,
				sysV86Fault, ctx->regs.edx);
		break;

	case 1:
		ctx->regs.eax = keShutdown(ctx->regs.ebx);
		break;

	case 2:
		thrSchedule();
		break;

	case 4:
		wprintf(L"[enter] ctx = %x esp = %x tss.esp0 = %x syscall_level = %d\n", 
			ctx, current->kernel_esp, tss.esp0, syscall_level);
		if (syscall_level < 3)
		{
			syscall_level++;
			asm("mov $4, %eax ; int $0x30");
			syscall_level--;
		}
		wprintf(L" [exit] ctx = %x esp = %x tss.esp0 = %x syscall_level = %d\n", 
			ctx, current->kernel_esp, tss.esp0, syscall_level);
		break;

	case 3:
		procTerminate(current->process);
		break;

	case 8:
		ctx->regs.eax = (dword) sysOpen((const wchar_t*) ctx->regs.ebx);
		break;

	case 9:
		sysMount((const wchar_t*) ctx->regs.ebx, (void*) ctx->regs.ecx);
		break;

	case 10:
		ctx->regs.eax = 
			sysInvoke((void*) ctx->regs.ebx, ctx->regs.ecx, (dword*) ctx->regs.edx,
				ctx->regs.esi);
		break;

	case 0x0100:
		thrSleep(current, ctx->regs.ebx);
		ctx->regs.eax = 1;
		break;

	case 0x0101:
		thrWaitHandle(current, (void**) ctx->regs.ebx, ctx->regs.ecx, ctx->regs.edx);
		ctx->regs.eax = 1;
		break;

	case 0x102:
		ctx->regs.eax = uptime;
		break;

	case 0x103:
		thrCall(current, (void*) ctx->regs.ebx, (void*) ctx->regs.ecx, ctx->regs.edx);
		ctx->regs.eax = 0;
		break;

	/*case 0x104:
		current->tls = ctx->regs.ebx;
		ctx->regs.eax = 1;
		break;

	case 0x105:
		ctx->regs.eax = current->tls;
		break;*/

	case 0x106:
		thrDelete(current);
		thrSchedule();
		break;

	case 0x107:
		ctx->regs.eax = (dword) sysCreateThread(ctx->regs.ebx, ctx->regs.ecx,
			ctx->regs.edx);
		break;
	
	case 0x200:
		ctx->regs.eax = (dword) procLoad(3, 
			(const wchar_t*) ctx->regs.ebx,
			(const wchar_t*) ctx->regs.ecx,
			ctx->regs.edx,
			(file_t*) ctx->regs.esi,
			(file_t*) ctx->regs.edi);
		thrSchedule();
		break;

	case 0x201:
		ctx->regs.eax = (dword) current->process;
		break;

	case 0x300:
		ctx->regs.eax = (dword) vmmAlloc(current->process, ctx->regs.ebx, ctx->regs.ecx,
			ctx->regs.edx);
		break;

	case 0x400:
		ctx->regs.eax = (dword) objMarshal(current->process, (void*) ctx->regs.ebx);
		break;

	case 0x401:
		ctx->regs.eax = (dword) objUnmarshal(current->process, (marshal_t) ctx->regs.ebx);
		break;

	case 0x402:
		objNotifyDelete(current->process, (marshal_t) ctx->regs.ebx);
		break;

	case 0x500:
		ctx->regs.eax = (dword) devOpen((const wchar_t*) ctx->regs.ebx, 
			(const wchar_t*) ctx->regs.ecx);
		break;

	case 0x501:
		ctx->regs.eax = devClose((device_t*) ctx->regs.ebx);
		break;

	case 0x502:
		errno = devUserRequest((device_t*) ctx->regs.ebx, 
			(request_t*) ctx->regs.ecx, (size_t) ctx->regs.edx);
		ctx->regs.eax = errno == 0;
		break;

	case 0x503:
		//devSwapUserBuffers((request_t*) ctx->regs.ebx);
		//ctx->regs.eax = 1;
		errno = devUserFinishRequest((request_t*) ctx->regs.ebx, 
			(bool) ctx->regs.ecx);
		ctx->regs.eax = errno == 0;
		break;

	case 0x600:
		ctx->regs.eax = (dword) hndAlloc((size_t) ctx->regs.ebx, current->process);
		break;

	case 0x601:
		hndFree((void*) ctx->regs.ebx);
		ctx->regs.eax = 1;
		break;

	case 0x602:
		if (ctx->regs.ebx == NULL)
			ctx->regs.eax = NULL;
		else
			ctx->regs.eax = (dword) hndHandle(ctx->regs.ebx)->process;
		break;

	case 0x700:
		ctx->regs.eax = (dword) fsOpen((const wchar_t*) ctx->regs.ebx);
		break;

	case 0x701:
		ctx->regs.eax = fsClose((file_t*) ctx->regs.ebx);
		break;

	case 0x702:
		if (ctx->regs.ebx == NULL)
		{
			request_t *req;
			req = (request_t*) ctx->regs.ecx;
			/* need to zero this in case devUserFinishRequest() is called */
			req->kernel_request = NULL;
			errno = EINVALID;
			ctx->regs.eax = 0;
		}
		else
		{
			errno = devUserRequest(((file_t*) ctx->regs.ebx)->fsd, 
				(request_t*) ctx->regs.ecx, (size_t) ctx->regs.edx);
			ctx->regs.eax = errno == 0;
		}
		break;

	case 0x703:
		fsSeek((file_t*) ctx->regs.ebx, 
			(qword) ctx->regs.ecx | (qword) ctx->regs.edx << 32);
		break;

	case 0x704:
		if (ctx->regs.ebx == NULL)
		{
			errno = EINVALID;
			ctx->regs.eax = 0;
		}
		else
		{
			qword pos = ((file_t*) ctx->regs.ebx)->pos;
			ctx->regs.eax = pos & 0xffffffff;
			ctx->regs.edx = (pos >> 32) & 0xffffffff;
		}
		break;

	case 0x705:
		{
			qword length = fsGetLength((file_t*) ctx->regs.ebx);
			ctx->regs.eax = length & 0xffffffff;
			ctx->regs.edx = (length >> 32) & 0xffffffff;
		}
		break;

	case 0x800:
		ctx->regs.eax = (dword) portCreate(current->process, 
			(const wchar_t*) ctx->regs.ebx);
		break;

	case 0x801:
		ctx->regs.eax = portListen((port_t*) ctx->regs.ebx);
		break;

	case 0x802:
		ctx->regs.eax = portConnect((port_t*) ctx->regs.ebx, 
			(const wchar_t*) ctx->regs.ecx);
		thrSchedule();
		break;

	case 0x803:
		ctx->regs.eax = (dword) portAccept((port_t*) ctx->regs.ebx);
		thrSchedule();
		break;

	case 0x804:
		portDelete((port_t*) ctx->regs.ebx);
		ctx->regs.eax = true;
		break;

	default:
		wprintf(L"%d: invalid syscall\n", ctx->regs.eax);
	}
}
