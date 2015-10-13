// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "backtrace", "Display stack backtrace", mon_backtrace },
	{ "checkvm", "Dump memory contents within certain virtual address range", mon_checkvm },
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "showmappings", "Display physical page mappings within certain range of virtual addresses", mon_showmappings },
	{ "setperm", "Set permission bit in page table entry for given virtual address", mon_setperm }
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	int *ebp = (int *)read_ebp();
	struct Eipdebuginfo info;
	uintptr_t eip;

	while (ebp) {
		eip = (uintptr_t)(*(ebp + 1));
		cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n",
			ebp, eip, *(ebp + 2), *(ebp + 3), *(ebp + 4), *(ebp + 5));
		if (!debuginfo_eip(eip, &info))
			cprintf("\t%s:%u: %.*s+%u\n",
				info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, eip - info.eip_fn_addr);
		ebp = (int *)(*ebp);
	}
	return 0;
}

uint32_t
read_addr(char *str, bool *success) {
	uint32_t ret = 0;
	uint32_t base = 10;
	char msg[] = "Support only addresses represented in Lowercase Hex or Decimal\n";
	*success = 0;
	if (str[0] == '0' && str[1] == 'x') {
		str += 2;
		base = 16;
	}
	while (*str) {
		ret *= base;
		if (*str >= 'a' && *str <= 'z') {
			if (base == 16)
				ret += *str - 'a' + 10;
			else {
				cprintf("%s", msg);
				return 0;
			}
		}
		else if (*str >= '0' && *str <= '9')
			ret += *str - '0';
		else {
			cprintf("%s", msg);
			return 0;
		}
		str++;
	}
	*success = 1;
	return ret;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	bool read_success = 0;
	uint32_t start_addr, end_addr;
	pte_t *pte;
	if (argc != 3) {
		cprintf("Usage: showmappings start_addr end_addr\n");
		return 0;
	}
	start_addr = read_addr(argv[1], &read_success);
	if (!read_success)
		return 0;
	end_addr = read_addr(argv[2], &read_success);
	if (!read_success || start_addr > end_addr)
		return 0;
	for (start_addr &= ~0xfff; start_addr <= end_addr; start_addr += PGSIZE) {
		pte = pgdir_walk(kern_pgdir, (const void *)start_addr, 0);
		if (pte == NULL)
			cprintf("page mapping %x -> NULL\n", start_addr);
		else {
			cprintf("page mapping %x -> %x: PTE_P %x, PTE_W %x, PTE_U %x\n",
			start_addr, PTE_ADDR(*pte), *pte & PTE_P, *pte & PTE_W, *pte & PTE_U);
		}
	}
	return 0;
}

int
mon_setperm(int argc, char **argv, struct Trapframe *tf)
{
	bool read_success = 0;
	uint32_t addr;
	pte_t *pte;
	uint32_t perm = 0;
	char msg[] = "Usage: setperm vaddr [0|1] [P|W|U]\n";
	if (argc != 4) {
		cprintf("%s", msg);
		return 0;
	}
	addr = read_addr(argv[1], &read_success);
	if (!read_success)
		return 0;
	pte = pgdir_walk(kern_pgdir, (const void *)addr, 1);
	switch (*argv[3]) {
	case 'P':
		perm = PTE_P;
		break;
	case 'W':
		perm = PTE_W;
		break;
	case 'U':
		perm = PTE_U;
		break;
	default:
		cprintf("Permission %s not supported\n", argv[3]);
	}
	cprintf("Permissions for %s:\nPTE_P %x, PTE_W %x, PTE_U %x ->\n",
		argv[1], *pte & PTE_P, *pte & PTE_W, *pte & PTE_U);
	if (*argv[2] == '1')
		*pte |= perm;
	else if (*argv[2] == '0')
		*pte &= ~perm;
	else
		cprintf("%s", msg);
	cprintf("PTE_P %x, PTE_W %x, PTE_U %x\n",
		*pte & PTE_P, *pte & PTE_W, *pte & PTE_U);
	return 0;
}

int
mon_checkvm(int argc, char **argv, struct Trapframe *tf)
{
	bool read_success = 0;
        void **start_addr, **end_addr;
        if (argc != 3) {
                cprintf("Usage: checkvm start_addr end_addr\n");
                return 0;
        }
        start_addr = (void **)read_addr(argv[1], &read_success);
        if (!read_success)
                return 0;
        end_addr = (void **)read_addr(argv[2], &read_success);
        if (!read_success)
                return 0;
	for (; start_addr <= end_addr; ++start_addr)
		cprintf("vaddr: %x, value: %x\n",
			start_addr, *start_addr);
        return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
