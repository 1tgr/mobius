void* ps2Init();
void ps2Delete(void*);

int __stdcall WinMain(unsigned hinst, unsigned hprev, const char* cmdline, unsigned cmdshow)
{
	ps2Delete(ps2Init());
}