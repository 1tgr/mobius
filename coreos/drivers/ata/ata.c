/* $Id: ata.c,v 1.1.1.1 2002/12/21 09:48:40 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>

#include "interface.h"

bool DrvInit(driver_t *drv)
{
    drv->add_device = AtaAddController;
    return true;
}
