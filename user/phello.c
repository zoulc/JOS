// hello, world
#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int i, j, pid;
	cprintf("hello, world!\n");
	for (i = 1; i <= 5; ++i) {
		pid = pfork(ENV_PRIORTY_NORMAL + i);
		if (pid == 0) {
			cprintf("hello, world! from child %d\n", i);
			for (j = 0; j < 5; ++j) {
				cprintf("child %d yields!\n", i);
				sys_yield();
			}
			return;
		}
	}
}
