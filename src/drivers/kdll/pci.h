/* Copyright 1999, Brian J. Swetland. All rights reserved.
** Distributed under the terms of the OpenBLT License
*/

#ifndef _PCI_H
#define _PCI_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _pci_cfg_t pci_cfg_t;
struct _pci_cfg_t
{
	/* normal header stuff */
	word vendor_id;
	word device_id;
	
	word command;
	word status;
	
	byte revision_id;
	byte interface;
	byte sub_class;
	byte base_class;
	
	byte cache_line_size;
	byte latency_timer;
	byte header_type;
	byte bist;	
	
	/* device info */
	byte bus;
	byte dev;
	byte func;
	byte irq;

	/* base registers */	
	dword base[6];
	dword size[6];
};

dword pci_read(int bus, int dev, int func, int reg, int bytes);
void pci_write(int bus, int dev, int func, int reg, dword v, int bytes);
int pci_probe(int bus, int dev, int func, pci_cfg_t *cfg);
bool pci_find(word vendor_id, word device_id, pci_cfg_t* cfg);

#ifdef __cplusplus
}
#endif

#ifdef PCI_STUB

#include <blt/Connection.h>
#include <blt/Message.h>

namespace BLT {

class PCI 
{
public:
	int read(int bus, int dev, int func, int reg, dword *v, int bytes);
	int write(int bus, int dev, int func, int reg, dword v, int bytes);
	int get_nth_cfg(int n, pci_cfg *cfg);
	
	static PCI *FindService();
	~PCI();
	
private:
	PCI(Connection *pci);	
	Connection *pci;
};

inline int 
PCI::read(int bus, int dev, int func, int reg, dword *v, int bytes)
{
	int32 res;
	Message msg;
	msg.PutInt32('code',2);
	msg.PutInt32('arg0',bus);
	msg.PutInt32('arg1',dev);
	msg.PutInt32('arg2',func);
	msg.PutInt32('arg3',reg);
	msg.PutInt32('arg4',bytes);
	
	if((res = pci->Call(&msg,&msg))) return res;
	msg.GetInt32('ret0',(int32*) v);
	msg.GetInt32('resp',&res);

	return res;	
}

inline int 
PCI::write(int bus, int dev, int func, int reg, dword v, int bytes)
{
	int32 res;
	Message msg;
	msg.PutInt32('code',3);
	msg.PutInt32('arg0',bus);
	msg.PutInt32('arg1',dev);
	msg.PutInt32('arg2',func);
	msg.PutInt32('arg3',reg);
	msg.PutInt32('arg4',(int32)v);
	msg.PutInt32('arg5',bytes);
	
	if((res = pci->Call(&msg,&msg))) return res;
	msg.GetInt32('resp',&res);

	return res;	
}

inline int 
PCI::get_nth_cfg(int n, pci_cfg *cfg)
{
	int32 res;
	Message msg;
	msg.PutInt32('code',1);
	msg.PutInt32('arg0',n);
	
	if((res = pci->Call(&msg,&msg))) return res;
	if((res = msg.GetData('ret0','pcic',cfg,sizeof(pci_cfg)))) return res;
	msg.GetInt32('resp',&res);
	
	return res;	
}

inline PCI *
PCI::FindService()
{
	Connection *cnxn = Connection::FindService("pci");
	if(cnxn) {
		return new PCI(cnxn);
	} else {
		return 0;
	}
}

PCI::PCI(Connection *cnxn){
	pci = cnxn;
}

PCI::~PCI(){
	delete pci;
}

}

#endif

#endif
