#define SYSCALL(rtn, name, argbyes, args...) rtn name(##args);

#include <os/sysdef.h>
