#include <stdlib.h>
#include <string.h>
#include <os/os.h>
#include <os/filesys.h>
#include <ctype.h>
#include <stdio.h>
#include "config.h"

config_t cfg_builtin =
{
	NULL, NULL,
	L"System Default",
	config_t::dispText,
	5000
};

config_t *cfg_current = &cfg_builtin, *cfg_first = &cfg_builtin, *cfg_last;

void ProcessKernelRcLine(const wchar_t* line, const wchar_t* setting, config_t* cfg)
{
	wprintf(L"[%s] %s = %s\n", cfg->name, line, setting);

	if (wcscmp(line, L"display") == 0)
	{
		if (wcsicmp(setting, L"gui") == 0)
			cfg->display = config_t::dispGui;
		else
			cfg->display = config_t::dispText;
	}
	else if (wcscmp(line, L"timeout") == 0)
		cfg->timeout = wcstol(setting, NULL, 10);
}

bool ProcessKernelRcSection(IStream* strm, config_t* cfg)
{
	char ch;
	wchar_t line[256], *buf, *setting;
	bool comment = false;
	
	buf = line;
	while (strm->Read(&ch, sizeof(ch)))
	{
		switch (ch)
		{
		case '\r':
			break;

		case '\n':
			*buf = '\0';
			
			buf = wcschr(line, '#');
			if (buf)
				*buf = '\0';

			if (*line)
			{
				setting = wcschr(line, '=');
				if (setting)
				{
					*setting = '\0';
					setting++;
				}
				else
					setting = L"1";

				for (buf = line; *buf; buf++)
					if (iswupper(*buf))
						*buf = towlower(*buf);

				ProcessKernelRcLine(line, setting, cfg);
			}

			buf = line;
			comment = false;
			break;

		case '#':
			comment = true;
		default:
			if (buf < line + countof(line))
			{
				*buf = ch;
				buf++;
			}
			break;

		case '[':
			if (!comment)
				return true;
		}
	}

	return false;
}

void ProcessKernelRc()
{
	IStream* strm;
	wchar_t line[256], *buf;
	char ch;
	config_t *cfg, common;
	bool finished = false, has_common = false;
	
	strm = (IStream*) fsOpen(L"/boot/kernelrc", IID_IStream);
	if (!strm)
		return;
	
	strm->SetIoMode(ioAnsi);

	//finished = !ProcessKernelRcSection(strm, &config);

	buf = line;
	while (!finished && strm->Read(&ch, sizeof(ch)))
	{
		switch (ch)
		{
		case '\r':
		case '[':
		case ']':
			break;

		case '\n':
			*buf = '\0';
			
			buf = wcschr(line, '#');
			if (buf)
				*buf = '\0';

			buf = line;
			
			if (line[0])
			{
				if (!has_common)
				{
					//memset(&common, 0, sizeof(common));
					common = cfg_builtin;
					wcscpy(common.name, line);
					
					if (!ProcessKernelRcSection(strm, &common))
						finished = true;

					has_common = true;
				}
				else
				{
					cfg = (config_t*) malloc(sizeof(config_t));
					*cfg = common;
					wcsncpy(cfg->name, line, countof(cfg->name));
					cfg->next = NULL;
					cfg->prev = cfg_last;
					
					if (cfg_last)
						cfg_last->next = cfg;

					if (cfg_first == NULL)
						cfg_current = cfg_first = cfg;
						
					cfg_last = cfg;

					if (!ProcessKernelRcSection(strm, cfg))
						finished = true;
				}
			}

			break;

		default:
			if (buf < line + countof(line))
			{
				*buf = ch;
				buf++;
			}
			break;
		}
	}

	strm->Release();
}