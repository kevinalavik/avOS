#include <Core/Panic.h>
#include <Library/Printf.h>

#include <stdarg.h>
#include <stdint.h>

#define REG3(NameA, ValueA, NameB, ValueB, NameC, ValueC)                     \
	Printf("  %-3s: 0x%016llx  %-3s: 0x%016llx  %-3s: 0x%016llx\n", NameA,    \
		   (unsigned long long)(ValueA), NameB, (unsigned long long)(ValueB), \
		   NameC, (unsigned long long)(ValueC))

#define REG2(NameA, ValueA, NameB, ValueB)                \
	Printf("  %-3s: 0x%016llx  %-3s: 0x%016llx\n", NameA, \
		   (unsigned long long)(ValueA), NameB, (unsigned long long)(ValueB))

void Panic(Frame *frame, const char *Reason, ...)
{
	__asm__ volatile("cli");

	PrintLine("========== KERNEL PANIC ==========");

	Printf("  reason: ");

	if (Reason) {
		va_list Arguments;
		va_start(Arguments, Reason);
		VPrintf(Reason, Arguments);
		va_end(Arguments);
	} else {
		Printf("no reason provided");
	}

	PrintLine("");

	if (!frame) {
		PrintLine("  no interrupt frame was provided");
		PrintLine("========= system halted =========");

		for (;;) {
			__asm__ volatile("hlt");
		}
	}

	Printf("  vector : %llu  err: 0x%016llx", (unsigned long long)frame->vector,
		   (unsigned long long)frame->err);

	if (frame->vector == 14) {
		Printf("  cr2: 0x%016llx", (unsigned long long)frame->cr2);
	}

	PrintLine("");

	REG3("rip", frame->rip, "rsp", frame->rsp, "rbp", frame->rbp);
	REG3("rax", frame->rax, "rbx", frame->rbx, "rcx", frame->rcx);
	REG3("rdx", frame->rdx, "rsi", frame->rsi, "rdi", frame->rdi);
	REG3("r8", frame->r8, "r9", frame->r9, "r10", frame->r10);
	REG3("r11", frame->r11, "r12", frame->r12, "r13", frame->r13);
	REG2("r14", frame->r14, "r15", frame->r15);
	REG3("cs", frame->cs, "ss", frame->ss, "rflags", frame->rflags);
	REG3("cr0", frame->cr0, "cr2", frame->cr2, "cr3", frame->cr3);
	REG2("cr4", frame->cr4, "", 0);

	PrintLine("========= system halted =========");

	for (;;) {
		__asm__ volatile("hlt");
	}
}
