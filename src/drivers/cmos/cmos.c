/* $Id: cmos.c,v 1.4 2002/04/20 12:47:27 pavlovskii Exp $ */

#include <kernel/kernel.h>
#include <kernel/driver.h>
#include <kernel/arch.h>
#include <errno.h>

/* #define DEBUG */
#include <kernel/debug.h>

/*!
 *  \ingroup	drivers
 *  \defgroup	cmos	CMOS/RTC
 *  @{
 */

#define PCIDEV_RTC	0x1f00

#define CMOS_CTRL	0x70
#define CMOS_DATA	0x71

#define RTC_SECOND	0
#define RTC_MINUTE	2
#define RTC_HOUR	4
#define RTC_DAYMONTH	7
#define RTC_MONTH	8
#define RTC_YEAR	9
#define RTC_STATUS_B	11

uint8_t rtcRead(uint16_t port, uint8_t reg)
{
	uint8_t high_digit, low_digit;
	out(port, reg);
	high_digit = low_digit = in(port + 1);
	/* convert from BCD to binary */
	high_digit >>= 4;
	high_digit &= 0x0F;
	low_digit &= 0x0F;
	return 10 * high_digit + low_digit;
}

/*****************************************************************************
Finds the number of days between two dates in the Gregorian calendar.
- it's a leap year if the year is divisible by 4,
- UNLESS the year is also divisible by 100,
- UNLESS the year is also divisible by 400

This code divides the time between start_day/start_year and end_day/end_year
into "slices": fourcent (400-year) slices in the middle, bracketed on
either end by century slices, fouryear (4-year) slices, and year slices.

It's a highly generalized algorithm. If you call it with
	greg_to_jdn(-4713, 327, curr_day_in_year, curr_year);
you get the true Julian Day Number
(JDN; days since Nov 24, 4714 BC/BCE in [proleptic] Gregorian calendar)
Call with
	greg_to_jdn(1970, 0, curr_day_in_year, curr_year);
you get the number of days in the Unix epoch (since Jan 1, 1970)

This algorithm produces identical results to the one shown here:
	http://serendipity.magnet.ch/hermetic/cal_stud/jdn.htm
I think it's easier to see where my code came from, however.

And don't worry about all that heavy-duty math. GCC is pretty
good at folding constants, and doing strength-reduction
(shifts instead of divides, AND instead of MOD, etc.)
*****************************************************************************/
static long days_between_dates(short start_year, unsigned short start_day,
		short end_year, unsigned short end_day)
{
	short fourcents, centuries, fouryears, years;
	long days;

	fourcents = end_year / 400 - start_year / 400;
	centuries = end_year / 100 - start_year / 100 -
/* subtract from 'centuries' the centuries already accounted for by
'fourcents' */
		fourcents * 4;
	fouryears = end_year / 4 - start_year / 4 -
/* subtract from 'fouryears' the fouryears already accounted for by
'fourcents' and 'centuries' */
		fourcents * 100 - centuries * 25;
	years = end_year - start_year -
/* subtract from 'years' the years already accounted for by
'fourcents', 'centuries', and 'fouryears' */
		400 * fourcents - 100 * centuries - 4 * fouryears;
/* add it up: 97 leap days every fourcent */
	days = (365L * 400 + 97) * fourcents;
/* 24 leap days every residual century */
	days += (365L * 100 + 24) * centuries;
/* 1 leap day every residual fouryear */
	days += (365L * 4 + 1) * fouryears;
/* 0 leap days for residual years */
	days += (365L * 1) * years;
/* residual days (need the cast!) */
	days += ((long)end_day - start_day);
/* account for terminal leap year */
	if(end_year % 4 == 0 && end_day >= 60)
	{
		days++;
		if(end_year % 100 == 0)
			days--;
		if(end_year % 400 == 0)
			days++;
	}
/* xxx - what have I wrought? I don't know what's going on here,
but the code won't work properly without it */
	if(end_year >= 0)
	{
		days++;
		if(end_year % 4 == 0)
			days--;
		if(end_year % 100 == 0)
			days++;
		if(end_year % 400 == 0)
			days--;
	}
	if(start_year > 0)
		days--;
	return days;
}

/*****************************************************************************
NOTE: this function works only with local time, stored in the CMOS clock.
It knows nothing of GMT or timezones.
*****************************************************************************/
#define	EPOCH_YEAR	1970
#define	EPOCH_DAY	0 /* Jan 1 */

unsigned long sys_time(void)
{
	static const unsigned short days_to_date[12] =
	{
/*              jan  feb  mar  apr  may  jun  jul  aug  sep  oct  nov  dec */
		0,
		31,
		31 + 28,
		31 + 28 + 31,
		31 + 28 + 31 + 30,
		31 + 28 + 31 + 30 + 31,
		31 + 28 + 31 + 30 + 31 + 30,
		31 + 28 + 31 + 30 + 31 + 30 + 31,
		31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
		31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
		31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
		31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30
	};
	unsigned short day, month, hour, minute, second;
	unsigned long time;
	short year;

/* b2=0 BCD mode, vs. binary (binary mode seems to be buggy)
b1=1	24-hour mode, vs. 12-hour mode

### - This could be done once when the OS initializes,
instead of being done on every syscall. */
	out(CMOS_CTRL, RTC_STATUS_B);
	out(CMOS_DATA, (in(CMOS_DATA) & ~6) | 2);
/* wait for stored time value to stop changing */
	out(CMOS_CTRL, 10);
	while(in(CMOS_DATA) & 128)
		/* nothing */;
/* get year (0-99)	xxx - OH NO, Y2K!
	year = read_cmos(9) + 1900; */
	year = rtcRead(CMOS_CTRL, RTC_YEAR) + 2000;
/* get month (1-12) */
	month = rtcRead(CMOS_CTRL, RTC_MONTH);
/* get date (1-31) */
	day = rtcRead(CMOS_CTRL, RTC_DAYMONTH);
	TRACE3("Current date: MM/DD/YYYY=%u/%u/%u\n",
		month, day, year);
/* convert date to (0-30) */
	day--;
/* convert month to (0-11), convert to days-to-date, add */
	day += days_to_date[month - 1];
	TRACE2("Epoch day/year=%u/%u\n", EPOCH_DAY, EPOCH_YEAR);
	TRACE2("Current day/year=%u/%u\n", day, year);
/* convert to MJDN (days since Jan 1, 1970) */
	time = days_between_dates(EPOCH_YEAR, EPOCH_DAY, year, day);
/* read current time */
	hour = rtcRead(CMOS_CTRL, RTC_HOUR); /* 0-23 */
	minute = rtcRead(CMOS_CTRL, RTC_MINUTE); /* 0-59 */
	second = rtcRead(CMOS_CTRL, RTC_SECOND); /* 0-59 */
	TRACE3("Current time is %u:%u:%u\n", hour, minute, second);
/* convert time to hours, add current hour */
	time *= 24;
	time += hour;
/* convert to minutes, add current minute */
	time *= 60;
	time += minute;
/* convert to seconds, add current second */
	time *= 60;
	time += second;

	return time;
}

bool rtcRequest(device_t* dev, request_t* req)
{
	uint64_t* qw;
	request_dev_t *req_dev;

	req_dev = (request_dev_t*) req;
	switch (req->code)
	{
	case DEV_REMOVE:
		free(dev);
		return true;

	case DEV_READ:
		if (req_dev->params.dev_read.length < sizeof(uint64_t))
		{
			req->result = EBUFFER;
			return false;
		}

		qw = MemMapPageArray(req_dev->params.dev_read.pages, 
			PRIV_WR | PRIV_PRES | PRIV_KERN);
		*qw = sys_time();
		MemUnmapTemp();
		return true;
	}

	req->result = ENOTIMPL;
	return false;
}

static const device_vtbl_t cmos_vtbl =
{
	rtcRequest,
	NULL,
	NULL,
};

device_t* cmosAddDevice(driver_t* drv, const wchar_t* name, device_config_t* cfg)
{
	device_t* dev;

	/*TRACE2("cmos: %x/%x\n", cfg->vendor_id, cfg->device_id);
	if (cfg->vendor_id != 0xffff)
		return NULL;

	switch (cfg->device_id)
	{
	case PCIDEV_RTC:*/
		dev = malloc(sizeof(device_t));
		dev->driver = drv;
		dev->vtbl = &cmos_vtbl;

		TRACE2("CMOS %s installed; time = %u\n", name, sys_time());
		return dev;
	/*}

	return NULL;*/
}

bool DrvInit(driver_t* drv)
{
	drv->add_device = cmosAddDevice;
	return true;
}

/*@}*/