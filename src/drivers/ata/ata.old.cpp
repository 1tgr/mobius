#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <printf.h>
#include <stdio.h>
#include "ata.h"
#include <kernel/driver.h>

#define CLRSCR		L"\x1b[2J"
#define DEBUG(s)	

volatile word _interrupt_occurred;

void __cdecl AtaIrq(dword context, int irq)
{
	//wprintf(L"ATA(PI) interrupt %d\n", irq);
	_interrupt_occurred |= 1 << irq;
}

CAtaDrive::CAtaDrive()
{
	m_refs = 0;
}

// IUnknown methods
HRESULT CAtaDrive::QueryInterface(REFIID iid, void ** ppvObject)
{
	if (InlineIsEqualGUID(iid, IID_IUnknown) ||
		InlineIsEqualGUID(iid, IID_IDevice))
	{
		*ppvObject = (IDevice*) this;
		AddRef();
		return S_OK;
	}
	else if (InlineIsEqualGUID(iid, IID_IBlockDevice))
	{
		*ppvObject = (IBlockDevice*) this;
		AddRef();
		return S_OK;
	}

	return E_FAIL;
}

// IDevice method
HRESULT CAtaDrive::GetInfo(device_t* buf)
{
	if (buf->size < sizeof(device_t))
		return E_FAIL;

	wcscpy(buf->name, m_name);
	return S_OK;
}

HRESULT CAtaDrive::DeviceOpen()
{
	return S_OK;
}

// IBlockDevice methods
HRESULT CAtaDrive::GetSize(blocksize_t *size)
{
	size_t old_size = size->size;
	if (old_size < sizeof(blocksize_t))
		return E_FAIL;

	*size = m_size;
	size->size = old_size;
	return S_OK;
}

bool CAtaDrive::WaitStatus(word mask, word bits)
{
	dword end;
	word stat;

	end = sysUpTime() + 2000;
	while (((stat = in(m_port + ATA_REG_STATUS)) & mask) != bits)
	{
		if (sysUpTime() >= end)
		{
			wprintf(L"ATA: wait failed; stat = %x\n", stat);
			return false;
		}
	}

	return true;
}

void CAtaDrive::Select()
{
	out(m_port + ATA_REG_DRVHD, m_unit);
}

void CAtaDrive::BlockToChs(int block, int *cyl, int *head, int *sect)
{
	*sect = block % m_sectors + 1;
	block /= m_sectors;
	*head = block % m_heads;
	block /= m_heads;
	*cyl = block;

	/**cyl = block / (m_heads * m_sectors);
	block %= m_heads * m_sectors;
	*head = block / m_sectors;
	block %= m_sectors;
	*sect = block;*/
}

size_t CAtaDrive::BlockRead(addr_t start, size_t blocks, void* buffer)
{
	unsigned short *buf;
	int cyl, head, sect;
	size_t i, read, per = min(blocks, m_mult_max);

	if (!m_heads || !m_sectors)
		return 0;

	buf = (unsigned short *) buffer;
	start += m_start_sector;
	read = 0;

	while (read < blocks)
	{
		BlockToChs(start, &cyl, &head, &sect);
		//wprintf(L"%d = %d:%d:%d\t", start, cyl, head, sect);
		//IDE_WAIT_0 (STATUS, 7);
		if (!WaitStatus(ATA_BUSY, 0))
			return read;

		//IDE_SEL_DH (bus, device, head);
		//IDE_WAIT_0 (STATUS, 7);
		//IDE_WAIT_1 (STATUS, 6);
		Select();
		/*if (!WaitStatus(ATA_BUSY, 0) ||
			!WaitStatus(ATA_READY, ATA_READY))
			return read;*/
		if (!WaitStatus(ATA_BUSY | ATA_READY, ATA_READY))
			return read;
		
		out(m_port + ATA_REG_LOCYL, cyl & 0xff);
		out(m_port + ATA_REG_HICYL, (cyl >> 8) & 0xff);
		out(m_port + ATA_REG_SECTOR, sect /*+ 1*/);
		out(m_port + ATA_REG_DRVHD, head);
		out(m_port + ATA_REG_COUNT, per);
		out(m_port + ATA_REG_CMD, ATA_CMD_READ);
		//IDE_WAIT_0 (STATUS, 7);
		//IDE_WAIT_1 (STATUS, 3);
		/*if (!WaitStatus(ATA_BUSY, 0) ||
			!WaitStatus(ATA_DRQ, ATA_DRQ))
			return read;*/
		if (!WaitStatus(ATA_BUSY | ATA_DRQ, ATA_DRQ))
			return read;
		
		for (i = 0; i < 256 * per; i++)
			buf[i] = in16(m_port + ATA_REG_DATA);
		
		read += per;
		start += per;
		buf += per * 256;
	}

	return read;
}

size_t CAtaDrive::BlockWrite(addr_t start, size_t blocks, const void* buffer)
{
	return 0;
}

wchar_t *ConvertName(const word* in_data, int off_start, int off_end)
{
	static wchar_t ret_val[255];
	int loop, loop1, last_space = 0;

	for (loop = off_start, loop1 = 0; loop <= off_end; loop++)
	{
		ret_val [loop1++] = (char) (in_data [loop] / 256);  /* Get High byte */
		ret_val [loop1++] = (char) (in_data [loop] % 256);  /* Get Low byte */
	}

	for (loop1--; loop1 >= 0 && ret_val[loop1] == ' '; loop1--)
		;

	ret_val[loop1 + 1] = '\0';  /* Make sure it ends in a NULL character */
	return ret_val;
}

#pragma pack(push, 1)

struct partition_t
{
	byte bBoot;
	byte bStartHead;
	byte bStartSector;
	byte bStartCylinder;
	byte bSystem;
	byte bEndHead;
	byte bEndSector;
	byte bEndCylinder;
	dword dwStartSector;
	dword dwSectorCount;
};
#pragma pack(pop)

const wchar_t* part_type(int type)
{
	switch (type)
	{
	case 0x00:
		return L"FDISK_TYPE_EMPTY";
	case 0x01:
		return L"FDISK_TYPE_FAT12";
	case 0x04:
		return L"FDISK_TYPE_FAT16_SMALL";
	case 0x05:
		return L"FDISK_TYPE_EXTENDED";
	case 0x06:
		return L"FDISK_TYPE_FAT16_BIG";
	//case 0x07:
	case 0x0C:
		return L"FDISK_TYPE_FAT32";
	case 0x0F:
		return L"FDISK_TYPE_NTFS";
	case 0x82:
		return L"FDISK_TYPE_LINUX_SWAP";
	case 0x83:
		return L"FDISK_TYPE_EXT2";
	case 0xa5:
		return L"FDISK_TYPE_FREEBSD";
	case 0xa6:
		return L"FDISK_TYPE_OPENBSD";
	case 0xeb:
		return L"FDISK_TYPE_BFS";
	default:
		return L"unknown";
	}
}

void ataDetect()
{
	word dd[256];
	int dd_off, i, j;
	word ports[4] = {  0x1F0,  0x1F0,  0x170,  0x170 };
	byte units[4] = {   0xA0,   0xB0,   0xA0,   0xB0 };
	word irqs[4] =  { 0x4000, 0x4000, 0x8000, 0x8000 };
	dword end;
	bool fail = false;
	byte stat;
	CAtaDrive *drive, *partdrive;
	wchar_t str[10];
	partition_t *parts;
	
	sysRegisterIrq(14, AtaIrq, NULL);
	sysRegisterIrq(15, AtaIrq, NULL);

	parts = (partition_t*) (dd + 0xdf);
	for (i = 0; i < 4; i++)  /* Loop through drives */
    {
		wprintf(L"Detecting drive %d...\n", i);

		if (units[i] == 0xA0)
		{
			out(ports[i] + ATA_REG_DEVCTRL, 0x06);
			nsleep(400);
			/* release soft reset AND enable interrupts from drive */
			out(ports[i] + ATA_REG_DEVCTRL, 0x00);
			nsleep(400);
			/* wait up to 2 seconds for status =
			BUSY=0  READY=1  DF=?  DSC=?		DRQ=?  CORR=?  IDX=?  ERR=0 */

			end = sysUpTime() + 5000;
			fail = false;
			while (((stat = in(ports[i] + 7)) & 0xC1) != 0x40)
			{
				if (sysUpTime() >= end)
				{
					fail = true;
					break;
				}
			}

			if (fail)
			{
				wprintf(L"stat = %x, no master detected\n", stat);
				i++;	// Skip slave as well
				continue;
			}
		}

		fail = false;

		// Get IDE Drive info
		
		//_interrupt_occurred = 0;
		//end = sysUpTime() + 2000;
		//while (/*(_interrupt_occurred & irqs[i]) == 0 &&*/
			//((stat = in(ports[i] + 7)) & ATA_READY) == 0)
		/*{
			//putwchar('.');
			//wprintf(L"%d %d\r", sysUpTime(), end);
			if (sysUpTime() >= end)
			{
				fail = true;
				break;
			}
		}

		if (fail)
		{
			wprintf(L"%d: Drive isn't ready (stat = %x), trying anyway\n", i, stat);
			//continue;
		}*/

		end = sysUpTime() + 2000;
		while ((stat = in(ports[i] + ATA_REG_STATUS) & ATA_BUSY))
		{
			if (sysUpTime() >= end)
			{
				fail = true;
				break;
			}
		}
		
		if (fail)
		{
			wprintf(L"stat=%x: fail on wait BUSY = 0\n", stat);
			continue;
		}

		//_interrupt_occurred = 0;
		
		out(ports[i] + 6, units[i]);	// Get first/second drive
		
		end = sysUpTime() + 2000;
		while (((stat = in(ports[i] + ATA_REG_STATUS) & ATA_READY) == 0))
		{
			if (sysUpTime() >= end)
			{
				fail = true;
				break;
			}
		}
		
		if (fail)
		{
			wprintf(L"stat=%x: fail on wait READY = 1\n", stat);
			continue;
		}

		out(ports[i] + 7, ATA_CMD_ID);	// Get drive info data
		
		end = sysUpTime() + 2000;
		while (((stat = in(ports[i] + ATA_REG_STATUS) & ATA_DRQ) == 0))
		{
			if ((stat & ATA_ERR) ||
				sysUpTime() >= end)
			{
				fail = true;
				break;
			}
		}

		/*end = sysUpTime() + 2000;
		fail = false;

		while (!_interrupt_occurred)
		{
			if (sysUpTime() >= end)
			{
				fail = true;
				break;
			}
		}

		if (fail)
		{
			_cputws(L"not present\n");
			continue;
		}*/

		/* Wait for data ready */
		/* was ATA_READY | ATA_DSC | ATA_DRQ  58 */
		//while ((stat = in(ports[i] + 7)) != (ATA_READY | ATA_DSC))
		//while (//!_interrupt_occurred &&
			//(stat = in(ports[i] + 7)) != (ATA_READY | ATA_DSC | ATA_DRQ))
		//while (/*(_interrupt_occurred & irqs[i]) == 0 &&*/
		/*	((stat = in(ports[i] + 7)) & ATA_DRQ) == 0)
		{
			if (sysUpTime() >= end)
			{
				fail = true;
				break;
			}
		}*/

		//if (((stat = in(ports[i] + 7)) & ATA_DRQ) == 0)
		if (fail)
		{
			wprintf(L"stat = %x, ATAPI? ", stat);

			_interrupt_occurred = 0;
			out(ports[i] + 7, ATA_CMD_PID);          /* Get ATAPI drive info data */
			
			end = sysUpTime() + 2000;
			fail = false;
			 /* Wait for data ready */
			//while ((stat = in(ports[i] + 7)) != (ATA_READY | ATA_DSC))
			while ((_interrupt_occurred & irqs[i]) == 0 &&
				((stat = in(ports[i] + 7)) & ATA_DRQ) == 0)
			{
				if (sysUpTime() >= end)
				{
					fail = true;
					break;
				}
			}

			if (fail)
			{
				wprintf(L"stat = %x, not present\n", stat);
				continue;
			}
		}
		
		for (dd_off = 0; dd_off != 256; dd_off++) /* Read "sector" */
			dd[dd_off] = in16(ports[i]);
		
		/*wprintf(L"Model Number______________________: %s\n", ConvertName(dd, 27, 46));
		wprintf(L"Serial Number_____________________: %s\n", ConvertName(dd, 10, 19));
		wprintf(L"Controller Revision Number________: %s\n\n", ConvertName(dd, 23, 26));
		wprintf(L"Able to do Double Word Transfer___: %6s\n", (dd [48] == 0 ? L"No" : L"Yes"));
		wprintf(L"Controller type___________________:   %04X\n", dd [20]);
		wprintf(L"Controller buffer size (bytes)____: %6u\n", dd [21] * 512);
		wprintf(L"Number of ECC bytes transferred___: %6u\n", dd [22]);
		wprintf(L"Number of sectors per interrupt___: %6u\n\n", dd [47]);
		wprintf(L"Number of Cylinders (Fixed)_______: %6u\n", dd [1]);
		wprintf(L"Number of Heads___________________: %6u\n", dd [3]);
		wprintf(L"Number of Sectors per Track_______: %6u\n\n", dd [6]);*/

		drive = new CAtaDrive;
		drive->m_port = ports[i];
		drive->m_irq = irqs[i];
		drive->m_unit = units[i];
		drive->m_cylinders = dd[1];
		drive->m_heads = dd[3];
		drive->m_sectors = dd[6];
		drive->m_start_sector = 0;
		drive->m_total_sectors = drive->m_sectors * drive->m_heads * drive->m_cylinders;

		if (((dd[119] & 1) != 0) && (dd[118] != 0))
			drive->m_mult_max = dd[94];
		else
			drive->m_mult_max = 1;

		wcscpy(drive->m_name, ConvertName(dd, 27, 46));
		wcscat(drive->m_name, L" ");
		wcscat(drive->m_name, ConvertName(dd, 10, 19));
		swprintf(str, L"ide%d", i);
		sysExtRegisterDevice(str, drive);
		wprintf(L"Registered %s => %s [ ", str, drive->m_name);

		if (drive->BlockRead(0, 1, dd))
		{
			for (j = 0; j < 4; j++)
			{
				word c, h, s;

				c = parts[j].bStartCylinder | 
					((word) parts[j].bStartSector & 0xc0) << 2;
				h = parts[j].bStartHead;
				s = parts[j].bStartSector & 0x3f;

				if (parts[j].bSystem)
				{
					/*wprintf(L"CHS\t%d:%d:%d\t"
					L"System\t%x (%s)\t"
					L"Start\t%d\t",
					c, h, s, 
					parts[j].bSystem,
					part_type(parts[j].bSystem),
					parts[j].dwStartSector);

					wprintf(L"Size\t");
					kb = parts[j].dwSectorCount / 2;
					if (kb > 1048576)
						wprintf(L"%d Mb\n", kb / 1024);
					else
						wprintf(L"%d Kb\n", kb);*/

					partdrive = new CAtaDrive;
					*partdrive = *drive;

					partdrive->m_start_sector = parts[j].dwStartSector;
					/*partdrive->m_start_sector = s + 
						h * partdrive->m_sectors + 
						c * partdrive->m_heads;*/
					partdrive->m_total_sectors = parts[j].dwSectorCount;
					swprintf(str, L"ide%d%c", i, j + 'a');
					sysExtRegisterDevice(str, partdrive);
					//wprintf(L"%c ", j + 'a');
					wprintf(L"%s ", part_type(parts[j].bSystem));

					/*if (partdrive->BlockRead(0, 2, buf) == 2)
						DumpBootSector(buf);*/
				}
			}
		}

		_cputws(L"]\n");

		//_wgetch();
	}
      
	wprintf(L"Finished detection!\n");
	//for (;;) ;
}
