/* $Id$ */
#include <stdio.h>
#include <wchar.h>
#include <stddef.h>
#include <stdlib.h>

#include "common.h"


static int (*out)(const wchar_t *, ...) = _wdprintf;

typedef struct lmutex_test_t lmutex_test_t;
struct lmutex_test_t
{
	int number1;
	int number2;
	//lmutex_t mutex;
	handle_t mutex;
};


static int ShTestLmutexThread(void *param)
{
	lmutex_test_t *test;
	unsigned i;

	test = param;
	ThrSleep(1000);
	for (i = 0; i < 1000000; i++)
	{
		__asm__("lock incl %0" : : "g" (test->number1));
		//LmuxAcquire(&test->mutex);
		SemDown(test->mutex);
		test->number2++;
		//LmuxRelease(&test->mutex);
		SemUp(test->mutex);
	}

	return 0;
}


static void ShTestLmutex(void)
{
	lmutex_test_t test;
	handle_t threads[5];
	unsigned i;

	//LmuxInit(&test.mutex);
	test.mutex = SemCreate(1);
	test.number1 = test.number2 = 0;

	out(L"ShTestLmutex\n"
		L"\tCreating threads... ");
	for (i = 0; i < _countof(threads); i++)
	{
		threads[i] = ThrCreateThread(ShTestLmutexThread, &test, 16, L"ShTestLmutexThread");
		out(L"%u ", threads[i]);
	}

	out(L"\tRunning...\n");

	for (i = 0; i < _countof(threads); i++)
	{
		ThrWaitHandle(threads[i]);
		HndClose(threads[i]);
	}

	out(L"\tnumber1 = %u number2 = %u\n"
		L"\tCleaning up...", test.number1, test.number2);
	//LmuxDelete(&test.mutex);
	HndClose(test.mutex);
	out(L"done\n");
}


typedef struct semaphore_test_t semaphore_test_t;
struct semaphore_test_t
{
	handle_t mutex;
	handle_t empty;
	handle_t full;
	wchar_t item[9];
};


static int ShTestSemaphoreConsumer(void *param)
{
	semaphore_test_t *test;
	wchar_t item[9];

	test = param;
	do
	{
		SemDown(test->full);
		SemDown(test->mutex);
        wcscpy(item, test->item);
		SemUp(test->mutex);
		SemUp(test->empty);

		out(L"consumer: got item %s\n", item);
	} while (wcscmp(item, L"quit") != 0);

	return 0;
}


static int ShTestSemaphoreProducer(void *param)
{
	semaphore_test_t *test;
	unsigned i, my_id;
	wchar_t item[9];

	test = param;
	my_id = ThrGetThreadInfo()->id;
	ThrSleep(1000);
	for (i = 0; i < 100; i++)
	{
		swprintf(item, L"%8d", i);
		out(L"producer %u: submitting item %s\n", my_id, item);

		SemDown(test->empty);
		SemDown(test->mutex);
		wcscpy(test->item, item);
		SemUp(test->mutex);
		SemUp(test->full);
	}

	SemDown(test->empty);
	SemDown(test->mutex);
	wcscpy(test->item, L"quit");
	SemUp(test->mutex);
	SemUp(test->full);

	return 0;
}


static void ShTestSemaphore(void)
{
	semaphore_test_t test;
	handle_t producers[2], consumer;
	unsigned i;

	out(L"ShTestSemaphore:\n"
		L"\tCreating semaphores... ");
	test.mutex = SemCreate(1);
	out(L"mutex = %u ", test.mutex);
	test.empty = SemCreate(1);
	out(L"empty = %u ", test.empty);
	test.full = SemCreate(0);
	out(L"full = %u\n"
		L"\tCreating threads... ", test.full);

	for (i = 0; i < _countof(producers); i++)
	{
		producers[i] = ThrCreateThread(ShTestSemaphoreProducer, &test, 16, L"ShTestSemaphoreProducer");
		out(L"producer %u = %u ", i, producers[i]);
	}

	consumer = ThrCreateThread(ShTestSemaphoreConsumer, &test, 16, L"ShTestSemaphoreConsumer");
	out(L"consumer = %u\n\tRunning\n", consumer);

	ThrWaitHandle(consumer);

	out(L"\tCleaning up...");
	for (i = 0; i < _countof(producers); i++)
	{
		ThrWaitHandle(producers[i]);
		HndClose(producers[i]);
	}

	HndClose(consumer);
	HndClose(test.mutex);
	HndClose(test.empty);
	HndClose(test.full);
	out(L"done\n");
}


void ShCmdTest(const wchar_t *command, wchar_t *params)
{
	static const struct
	{
		const wchar_t *name;
		void (*func)(void);
	} tests[] =
	{
		{ L"lmutex",	ShTestLmutex },
		{ L"semaphore", ShTestSemaphore },
	};

	wchar_t *token;
	unsigned i;

	if (*params == '\0')
	{
		printf("Possible tests are:\n");
		for (i = 0; i < _countof(tests); i++)
			printf("%10S", tests[i].name);

		params = ShPrompt(L"\n Tests? ", params);
		if (*params == '\0')
			return;
	}

	token = _wcstok(params, L" ");
	while (token != NULL)
	{
		for (i = 0; i < _countof(tests); i++)
			if (_wcsicmp(token, tests[i].name) == 0)
			{
				tests[i].func();
				break;
			}

		token = _wcstok(NULL, L" ");
	}
}
