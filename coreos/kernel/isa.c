/* $Id$ */

#include <kernel/kernel.h>
#include <kernel/driver.h>

#include <wchar.h>
#include <errno.h>
#include <string.h>

static void IsaDeleteDevice(device_t *dev)
{
	assert(false && "IsaDeleteDevice called");
}


static status_t IsaRequest(device_t *dev, request_t *req)
{
	return EINVALID;
}


static status_t IsaClaimResources(device_t *dev,
								  device_t *child,
								  dev_resource_t *resources, 
								  unsigned num_resources,
								  bool is_claiming)
{
	dev_config_t *config;

	config = child->cfg;

	if (is_claiming)
	{
		config->resources = realloc(config->resources,
			sizeof(dev_resource_t) * config->num_resources + num_resources);
		if (config->resources == NULL)
			return errno;

		memcpy(config->resources + config->num_resources,
			resources,
			sizeof(*resources) * num_resources);
		config->num_resources += num_resources;
	}

	return 0;
}


static status_t IsaRemoveChild(device_t *dev, device_t *child)
{
	dev_config_t *config;
	config = child->cfg;
	free(config->resources);
	free(config->profile_key);
	free(config);
	return 0;
}


static const device_vtbl_t isa_bus_vtbl =
{
	IsaDeleteDevice,
	IsaRequest,
	NULL,
	NULL,
	NULL,
	IsaClaimResources,
	IsaRemoveChild,
};


static device_t isa_bus =
{
	&isa_bus_vtbl,
	NULL,
	NULL,
	NULL,
	NULL, NULL,
	NULL,
	0,
};

bool DevInstallIsaDevice(const wchar_t *driver, const wchar_t *device)
{
	dev_config_t *cfg;
	wchar_t key[50];

	swprintf(key, L"ISA/%s", driver);
    cfg = malloc(sizeof(*cfg));
    cfg->bus = &isa_bus;
	cfg->bus_type = DEV_BUS_UNKNOWN;
    cfg->num_resources = 0;
    cfg->resources = NULL;
	cfg->profile_key = _wcsdup(key);
    cfg->device_class = 0;
    cfg->businfo = NULL;
	cfg->location.ptr = NULL;
    return DevInstallDevice(driver, device, cfg);
}
