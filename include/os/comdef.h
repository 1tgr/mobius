/* $Id: comdef.h,v 1.1 2002/09/13 23:13:03 pavlovskii Exp $ */

#ifndef __OS_COMDEF_H
#define __OS_COMDEF_H

#include <sys/types.h>

#ifndef __cominterface
#ifdef _MSC_VER
#define __cominterface
#else
#define __cominterface __attribute__((com_interface))
#endif
#endif

#ifndef __comcall
#ifdef _MSC_VER
#define __comcall       __cdecl
#else
#define __comcall
#endif
#endif

#define __interface_name        void

#ifdef __cplusplus

#define METHOD(ret, name)       virtual ret __comcall name
#define PURE                    = 0
#define THIS                    void
#define THIS_
#define METHODIMP(ret, name)    ret __comcall name
#define INTERFACE(name)         struct __cominterface name
#define INTERFACE_EXTENDS(name, base)   struct __cominterface name : base
#define INTERFACE_FORWARD(name) struct __cominterface name;

#else

#define METHOD(ret, name)       ret __comcall (*name)
#define PURE
#define THIS_                   __interface_name *this, 
#define THIS                    __interface_name *this
#define METHODIMP(ret, name)    ret __comcall name
#define INTERFACE(name)         typedef struct name name; \
                                struct name \
                                { \
                                    const struct vtbl_##name *vtbl; \
                                }; \
                                typedef struct vtbl_##name vtbl_##name; \
                                struct vtbl_##name
#define INTERFACE_EXTENDS(name, base)   INTERFACE(name)
#define INTERFACE_FORWARD(name) struct name;

#endif

#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct GUID GUID;
struct GUID
{
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
};
#endif

typedef GUID IID;
typedef GUID CLSID;

typedef struct comstr_t comstr_t;
struct comstr_t
{
    size_t allocated;
    unsigned refs;
    wchar_t *str;
};

#endif
