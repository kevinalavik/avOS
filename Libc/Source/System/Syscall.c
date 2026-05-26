#include <System/Syscall.h>

U64 Syscall0(U64 Number)
{
	U64 Result;

	__asm__ volatile("int $0x80" : "=a"(Result) : "a"(Number) : "memory");
	return Result;
}

U64 Syscall1(U64 Number, U64 Arg0)
{
	U64 Result;

	__asm__ volatile("int $0x80"
					 : "=a"(Result)
					 : "a"(Number), "D"(Arg0)
					 : "memory");
	return Result;
}

U64 Syscall2(U64 Number, U64 Arg0, U64 Arg1)
{
	U64 Result;

	__asm__ volatile("int $0x80"
					 : "=a"(Result)
					 : "a"(Number), "D"(Arg0), "S"(Arg1)
					 : "memory");
	return Result;
}

U64 Syscall3(U64 Number, U64 Arg0, U64 Arg1, U64 Arg2)
{
	U64 Result;

	__asm__ volatile("int $0x80"
					 : "=a"(Result)
					 : "a"(Number), "D"(Arg0), "S"(Arg1), "d"(Arg2)
					 : "memory");
	return Result;
}

U64 Syscall4(U64 Number, U64 Arg0, U64 Arg1, U64 Arg2, U64 Arg3)
{
	U64 Result;

	__asm__ volatile("int $0x80"
					 : "=a"(Result)
					 : "a"(Number), "D"(Arg0), "S"(Arg1), "d"(Arg2), "c"(Arg3)
					 : "memory");
	return Result;
}
