#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/ramdisk.h>
#include <kernel/config.h>
#include <stdlib.h>
#include <string.h>

device_t *isa;
wchar_t devname[16];
int num_resources;
device_resource_t *resources;

void isaAddItem(hashelem_t* elem)
{
	device_resource_t res;
	wchar_t *data, *ch;

	data = (wchar_t*) elem->data;
	if (wcsicmp(elem->str, L"name") == 0)
		wcscpy(devname, data);
	else if (wcsicmp(elem->str, L"io") == 0)
	{
		ch = wcschr(data, ',');
		if (ch)
		{
			*ch = 0;
			ch++;
		}
		else
			ch = L"1";

		res.cls = dresIo;
		res.u.io.base = wcstol(data, NULL, 16);
		res.u.io.length = wcstol(ch, NULL, 0);
		goto add_resource;
	}
	else if (wcsicmp(elem->str, L"memory") == 0)
	{
		ch = wcschr(data, ',');
		if (ch)
		{
			*ch = 0;
			ch++;
		}
		else
			ch = L"1";

		res.cls = dresMemory;
		res.u.memory.base = wcstol(data, NULL, 16);
		res.u.memory.length = wcstol(ch, NULL, 0);
		goto add_resource;
	}
	else if (wcsicmp(elem->str, L"irq") == 0)
	{
		res.cls = dresIrq;
		res.u.irq = wcstol(data, NULL, 0);
		goto add_resource;
	}
	else
		wprintf(L"%s: invalid keyword\n", elem->str);

	return;

add_resource:
	num_resources++;
	resources = realloc(resources, sizeof(device_resource_t) * num_resources);
	resources[num_resources - 1] = res;
}

bool STDCALL INIT_CODE drvInit(driver_t* drv)
{
	void* file;
	//char line[256], *p, *dest;
	char* p;
	size_t length;
	hashtable_t *table;
	device_config_t *cfg;

	file = ramOpen(L"isa.cfg");
	if (!file)
		return false;

	isa = hndAlloc(sizeof(device_t), NULL);
	isa->driver = drv;
	isa->request = NULL;
	isa->req_first = isa->req_last = NULL;
	isa->config = NULL;
	devRegister(L"isa", isa, NULL);

	length = ramFileLength(L"isa.cfg");

	p = (char*) file;

	while (p < (char*) file + length)
	{
		memset(devname, 0, sizeof(devname));
		num_resources = 0;
		resources = NULL;

		table = cfgParseStrLine((const char**) &p);
		hashList(table, isaAddItem);
		cfgDeleteTable(table);

		if (devname[0])
		{
			cfg = hndAlloc(sizeof(device_config_t), NULL);
			cfg->parent = isa;
			cfg->vendor_id = cfg->device_id = 0xffff;
			cfg->subsystem = 0xffffffff;
			cfg->num_resources = num_resources;
			cfg->resources = resources;
			devRegister(devname, NULL, cfg);
		}
	}

	return true;
}