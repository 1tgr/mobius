/* $Id$ */

#ifndef IPC_H_
#define IPC_H_

#ifdef __cplusplus
extern "C"
{
#endif

bool IpcCallExtra(handle_t ipc, uint32_t code, const wnd_params_t *params, 
				  const void *extra, size_t extra_length);
bool IpcCall(handle_t ipc, uint32_t code, const wnd_params_t *params);
uint32_t IpcReceiveExtra(handle_t ipc, wnd_params_t *params, void **extra, 
						 size_t *extra_length);
uint32_t IpcReceive(handle_t ipc, wnd_params_t *params);
handle_t IpcGetDefault(void);

#ifdef __cplusplus
}
#endif

#endif
