void exit(int code);
int main(void);

void mainCRTStartup(void)
{
	exit(main());
}