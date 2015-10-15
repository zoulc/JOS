#ifndef JOS_KERN_MONITOR_H
#define JOS_KERN_MONITOR_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

struct Trapframe;

// Activate the kernel monitor,
// optionally providing a trap frame indicating the current state
// (NULL if none).
void monitor(struct Trapframe *tf);

// Helper function for reading address from string
uint32_t read_addr(char *str, bool *success);

// Functions implementing monitor commands.
int mon_help(int argc, char **argv, struct Trapframe *tf);
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);
int mon_backtrace(int argc, char **argv, struct Trapframe *tf);
int mon_showmappings(int argc, char **argv, struct Trapframe *tf);
int mon_setperm(int argc, char **argv, struct Trapframe *tf);
int mon_checkvm(int argc, char **argv, struct Trapframe *tf);
#endif	// !JOS_KERN_MONITOR_H
