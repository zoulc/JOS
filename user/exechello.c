#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int r;
	if (fork() == 0) {
		if ((r = execl("hello", "", 0)) < 0)
			panic("execl(hello) failed: %e", r);
	}
	cprintf("i am parent environment %08x\n", thisenv->env_id);
}
