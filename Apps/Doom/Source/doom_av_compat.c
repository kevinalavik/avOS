#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

#include <System/Console.h>
#include <System/Fileio.h>
#include <System/Memory.h>
#include <System/Process.h>
#include <System/Time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/time.h>

int errno;

struct AvFile {
	Handle Handle;
	Offset Size;
	Offset Position;
	int Used;
	int Console;
	int Eof;
	int Error;
	unsigned char *Data;
	int Memory;
};

struct AvDir {
	Handle Handle;
	struct dirent Entry;
	int Used;
};

#define AV_FILE_MAX 32
#define AV_DIR_MAX 16

static FILE AvFiles[AV_FILE_MAX];
static DIR AvDirs[AV_DIR_MAX];
static FILE AvStdout = { 0, 0, 0, 1, 1, 0, 0 };
static FILE AvStderr = { 0, 0, 0, 1, 1, 0, 0 };
static FILE AvStdin = { 0, 0, 0, 1, 1, 1, 0 };

FILE *stdin = &AvStdin;
FILE *stdout = &AvStdout;
FILE *stderr = &AvStderr;

static int IsSpace(int C)
{
	return C == ' ' || C == '\t' || C == '\n' || C == '\r' || C == '\f' ||
		   C == '\v';
}

static int IsDigit(int C)
{
	return C >= '0' && C <= '9';
}

static int ToLowerLocal(int C)
{
	return (C >= 'A' && C <= 'Z') ? C + ('a' - 'A') : C;
}

void *memset(void *Dst, int Ch, size_t Count)
{
	unsigned char *D = (unsigned char *)Dst;
	while (Count--)
		*D++ = (unsigned char)Ch;
	return Dst;
}

void *memcpy(void *Dst, const void *Src, size_t Count)
{
	unsigned char *D = (unsigned char *)Dst;
	const unsigned char *S = (const unsigned char *)Src;
	while (Count--)
		*D++ = *S++;
	return Dst;
}

void *memmove(void *Dst, const void *Src, size_t Count)
{
	unsigned char *D = (unsigned char *)Dst;
	const unsigned char *S = (const unsigned char *)Src;
	if (D < S) {
		while (Count--)
			*D++ = *S++;
	} else if (D > S) {
		D += Count;
		S += Count;
		while (Count--)
			*--D = *--S;
	}
	return Dst;
}

int memcmp(const void *A, const void *B, size_t Count)
{
	const unsigned char *P = (const unsigned char *)A;
	const unsigned char *Q = (const unsigned char *)B;
	while (Count--) {
		if (*P != *Q)
			return (int)*P - (int)*Q;
		P++;
		Q++;
	}
	return 0;
}

size_t strlen(const char *S)
{
	size_t N = 0;
	if (!S)
		return 0;
	while (S[N])
		N++;
	return N;
}

char *strcpy(char *Dst, const char *Src)
{
	char *Out = Dst;
	while ((*Dst++ = *Src++) != '\0')
		;
	return Out;
}

char *strncpy(char *Dst, const char *Src, size_t Count)
{
	size_t I = 0;
	for (; I < Count && Src[I]; ++I)
		Dst[I] = Src[I];
	for (; I < Count; ++I)
		Dst[I] = '\0';
	return Dst;
}

char *strcat(char *Dst, const char *Src)
{
	strcpy(Dst + strlen(Dst), Src);
	return Dst;
}

char *strncat(char *Dst, const char *Src, size_t Count)
{
	char *D = Dst + strlen(Dst);
	while (Count-- && *Src)
		*D++ = *Src++;
	*D = '\0';
	return Dst;
}

int strcmp(const char *A, const char *B)
{
	while (*A && *A == *B) {
		A++;
		B++;
	}
	return (unsigned char)*A - (unsigned char)*B;
}

int strncmp(const char *A, const char *B, size_t Count)
{
	while (Count && *A && *A == *B) {
		A++;
		B++;
		Count--;
	}
	return Count ? (unsigned char)*A - (unsigned char)*B : 0;
}

int strcasecmp(const char *A, const char *B)
{
	while (*A && ToLowerLocal(*A) == ToLowerLocal(*B)) {
		A++;
		B++;
	}
	return ToLowerLocal(*A) - ToLowerLocal(*B);
}

int strncasecmp(const char *A, const char *B, size_t Count)
{
	while (Count && *A && ToLowerLocal(*A) == ToLowerLocal(*B)) {
		A++;
		B++;
		Count--;
	}
	return Count ? ToLowerLocal(*A) - ToLowerLocal(*B) : 0;
}

char *strchr(const char *S, int Ch)
{
	while (*S) {
		if (*S == (char)Ch)
			return (char *)S;
		S++;
	}
	return Ch == 0 ? (char *)S : NULL;
}

char *strrchr(const char *S, int Ch)
{
	const char *Last = NULL;
	do {
		if (*S == (char)Ch)
			Last = S;
	} while (*S++);
	return (char *)Last;
}

char *strstr(const char *Haystack, const char *Needle)
{
	size_t NeedleLen = strlen(Needle);
	if (NeedleLen == 0)
		return (char *)Haystack;
	for (; *Haystack; ++Haystack) {
		if (strncmp(Haystack, Needle, NeedleLen) == 0)
			return (char *)Haystack;
	}
	return NULL;
}

char *strdup(const char *S)
{
	size_t Len = strlen(S) + 1;
	char *Out = (char *)malloc(Len);
	if (Out)
		memcpy(Out, S, Len);
	return Out;
}

int atoi(const char *S)
{
	return (int)strtol(S, NULL, 10);
}

long atol(const char *S)
{
	return strtol(S, NULL, 10);
}

long strtol(const char *Nptr, char **Endptr, int Base)
{
	const char *S = Nptr;
	long Sign = 1;
	unsigned long Value = 0;

	while (IsSpace(*S))
		S++;
	if (*S == '-') {
		Sign = -1;
		S++;
	} else if (*S == '+') {
		S++;
	}
	if ((Base == 0 || Base == 16) && S[0] == '0' &&
		(S[1] == 'x' || S[1] == 'X')) {
		Base = 16;
		S += 2;
	} else if (Base == 0) {
		Base = S[0] == '0' ? 8 : 10;
	}

	for (;;) {
		int Digit;
		if (*S >= '0' && *S <= '9')
			Digit = *S - '0';
		else if (*S >= 'a' && *S <= 'z')
			Digit = *S - 'a' + 10;
		else if (*S >= 'A' && *S <= 'Z')
			Digit = *S - 'A' + 10;
		else
			break;
		if (Digit >= Base)
			break;
		Value = Value * (unsigned long)Base + (unsigned long)Digit;
		S++;
	}

	if (Endptr)
		*Endptr = (char *)S;
	return (long)(Value * (unsigned long)Sign);
}

unsigned long strtoul(const char *Nptr, char **Endptr, int Base)
{
	return (unsigned long)strtol(Nptr, Endptr, Base);
}

double strtod(const char *Nptr, char **Endptr)
{
	const char *S = Nptr;
	double Sign = 1.0;
	double Value = 0.0;
	double Scale = 1.0;
	int Any = 0;

	while (IsSpace(*S))
		S++;

	if (*S == '-') {
		Sign = -1.0;
		S++;
	} else if (*S == '+') {
		S++;
	}

	while (IsDigit(*S)) {
		Value = Value * 10.0 + (double)(*S - '0');
		S++;
		Any = 1;
	}

	if (*S == '.') {
		S++;
		while (IsDigit(*S)) {
			Scale *= 0.1;
			Value += (double)(*S - '0') * Scale;
			S++;
			Any = 1;
		}
	}

	if (Any && (*S == 'e' || *S == 'E')) {
		const char *ExpStart = S;
		int ExpSign = 1;
		int Exp = 0;
		int ExpAny = 0;

		S++;
		if (*S == '-') {
			ExpSign = -1;
			S++;
		} else if (*S == '+') {
			S++;
		}

		while (IsDigit(*S)) {
			Exp = Exp * 10 + (*S - '0');
			S++;
			ExpAny = 1;
		}

		if (ExpAny) {
			double Pow10 = 1.0;
			while (Exp-- > 0)
				Pow10 *= 10.0;

			if (ExpSign < 0)
				Value /= Pow10;
			else
				Value *= Pow10;
		} else {
			S = ExpStart;
		}
	}

	if (Endptr)
		*Endptr = (char *)(Any ? S : Nptr);

	return Any ? Value * Sign : 0.0;
}

float strtof(const char *Nptr, char **Endptr)
{
	return (float)strtod(Nptr, Endptr);
}

long double strtold(const char *Nptr, char **Endptr)
{
	return (long double)strtod(Nptr, Endptr);
}

double atof(const char *S)
{
	return strtod(S, NULL);
}

int abs(int X)
{
	return X < 0 ? -X : X;
}

long labs(long X)
{
	return X < 0 ? -X : X;
}

static unsigned long AvRandState = 1;

void srand(unsigned int Seed)
{
	AvRandState = Seed ? Seed : 1;
}

int rand(void)
{
	AvRandState = AvRandState * 1103515245u + 12345u;
	return (int)((AvRandState / 65536u) % 32768u);
}

int atexit(void (*Function)(void))
{
	(void)Function;
	return 0;
}

void abort(void)
{
	Exit(1);
}

void exit(int Status)
{
	Exit((U64)Status);
}

char *getenv(const char *Name)
{
	(void)Name;
	return NULL;
}

int system(const char *Command)
{
	(void)Command;
	return -1;
}

static void BufferPut(char **Out, size_t *Remaining, int *Written, char Ch)
{
	if (*Remaining > 1) {
		**Out = Ch;
		(*Out)++;
		(*Remaining)--;
	}
	(*Written)++;
}

static void BufferPad(char **Out, size_t *Remaining, int *Written, char Ch,
					  int Count)
{
	while (Count-- > 0)
		BufferPut(Out, Remaining, Written, Ch);
}

static void FormatUnsigned(char **Out, size_t *Remaining, int *Written,
						   unsigned long long Value, unsigned Base, int Upper,
						   int Width, int PadZero, int Left, int Precision,
						   char Sign, const char *Prefix)
{
	const char *Digits = Upper ? "0123456789ABCDEF" : "0123456789abcdef";
	char Tmp[32];
	int Len = 0;
	int PrefixLen = Prefix ? (int)strlen(Prefix) : 0;

	if (Value == 0 && Precision == 0) {
		Len = 0;
	} else {
		do {
			Tmp[Len++] = Digits[Value % Base];
			Value /= Base;
		} while (Value != 0 && Len < (int)sizeof(Tmp));
	}

	int ZeroCount = 0;
	if (Precision > Len)
		ZeroCount = Precision - Len;

	int Total = Len + ZeroCount + PrefixLen + (Sign ? 1 : 0);

	if (!Left && (!PadZero || Precision >= 0))
		BufferPad(Out, Remaining, Written, ' ', Width - Total);

	if (Sign)
		BufferPut(Out, Remaining, Written, Sign);

	if (Prefix) {
		while (*Prefix)
			BufferPut(Out, Remaining, Written, *Prefix++);
	}

	if (!Left && PadZero && Precision < 0)
		BufferPad(Out, Remaining, Written, '0', Width - Total);

	BufferPad(Out, Remaining, Written, '0', ZeroCount);

	while (Len-- > 0)
		BufferPut(Out, Remaining, Written, Tmp[Len]);

	if (Left)
		BufferPad(Out, Remaining, Written, ' ', Width - Total);
}

static int AvVsnprintf(char *Buffer, size_t Size, const char *Format,
					   va_list Args)
{
	char *Out = Buffer;
	size_t Remaining = Size ? Size : 0;
	int Written = 0;

	while (*Format) {
		if (*Format != '%') {
			BufferPut(&Out, &Remaining, &Written, *Format++);
			continue;
		}

		Format++;

		int PadZero = 0;
		int Left = 0;
		int Width = 0;
		int Precision = -1;
		int LongCount = 0;

		for (;;) {
			if (*Format == '-') {
				Left = 1;
				Format++;
			} else if (*Format == '0') {
				PadZero = 1;
				Format++;
			} else {
				break;
			}
		}

		if (*Format == '*') {
			Width = va_arg(Args, int);
			if (Width < 0) {
				Left = 1;
				Width = -Width;
			}
			Format++;
		} else {
			while (IsDigit(*Format)) {
				Width = Width * 10 + (*Format - '0');
				Format++;
			}
		}

		if (*Format == '.') {
			Format++;
			Precision = 0;

			if (*Format == '*') {
				Precision = va_arg(Args, int);
				if (Precision < 0)
					Precision = -1;
				Format++;
			} else {
				while (IsDigit(*Format)) {
					Precision = Precision * 10 + (*Format - '0');
					Format++;
				}
			}
		}

		while (*Format == 'l' || *Format == 'z' || *Format == 'h') {
			if (*Format == 'l')
				LongCount++;
			Format++;
		}

		switch (*Format) {
		case '%':
			BufferPut(&Out, &Remaining, &Written, '%');
			break;

		case 'c': {
			char Ch = (char)va_arg(Args, int);
			if (!Left)
				BufferPad(&Out, &Remaining, &Written, ' ', Width - 1);
			BufferPut(&Out, &Remaining, &Written, Ch);
			if (Left)
				BufferPad(&Out, &Remaining, &Written, ' ', Width - 1);
			break;
		}

		case 's': {
			const char *S = va_arg(Args, const char *);
			if (!S)
				S = "(null)";

			int Len = (int)strlen(S);
			if (Precision >= 0 && Len > Precision)
				Len = Precision;

			if (!Left)
				BufferPad(&Out, &Remaining, &Written, ' ', Width - Len);

			for (int I = 0; I < Len; ++I)
				BufferPut(&Out, &Remaining, &Written, S[I]);

			if (Left)
				BufferPad(&Out, &Remaining, &Written, ' ', Width - Len);
			break;
		}

		case 'd':
		case 'i': {
			long long V;

			if (LongCount >= 2)
				V = va_arg(Args, long long);
			else if (LongCount == 1)
				V = va_arg(Args, long);
			else
				V = va_arg(Args, int);

			char Sign = 0;
			unsigned long long U;

			if (V < 0) {
				Sign = '-';
				U = (unsigned long long)(-V);
			} else {
				U = (unsigned long long)V;
			}

			FormatUnsigned(&Out, &Remaining, &Written, U, 10, 0, Width, PadZero,
						   Left, Precision, Sign, NULL);
			break;
		}

		case 'u': {
			unsigned long long V;

			if (LongCount >= 2)
				V = va_arg(Args, unsigned long long);
			else if (LongCount == 1)
				V = va_arg(Args, unsigned long);
			else
				V = va_arg(Args, unsigned int);

			FormatUnsigned(&Out, &Remaining, &Written, V, 10, 0, Width, PadZero,
						   Left, Precision, 0, NULL);
			break;
		}

		case 'x':
		case 'X': {
			unsigned long long V;

			if (LongCount >= 2)
				V = va_arg(Args, unsigned long long);
			else if (LongCount == 1)
				V = va_arg(Args, unsigned long);
			else
				V = va_arg(Args, unsigned int);

			FormatUnsigned(&Out, &Remaining, &Written, V, 16, *Format == 'X',
						   Width, PadZero, Left, Precision, 0, NULL);
			break;
		}

		case 'p':
			FormatUnsigned(&Out, &Remaining, &Written,
						   (uintptr_t)va_arg(Args, void *), 16, 0, Width,
						   PadZero, Left, Precision, 0, "0x");
			break;

		default:
			BufferPut(&Out, &Remaining, &Written, '%');
			BufferPut(&Out, &Remaining, &Written, *Format);
			break;
		}

		if (*Format)
			Format++;
	}

	if (Size) {
		if (Remaining > 0)
			*Out = '\0';
		else
			Buffer[Size - 1] = '\0';
	}

	return Written;
}

int vsnprintf(char *Buffer, size_t Size, const char *Format, va_list Args)
{
	return AvVsnprintf(Buffer, Size, Format, Args);
}

int snprintf(char *Buffer, size_t Size, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Written = AvVsnprintf(Buffer, Size, Format, Args);
	va_end(Args);
	return Written;
}

int sprintf(char *Buffer, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Written = AvVsnprintf(Buffer, (size_t)-1, Format, Args);
	va_end(Args);
	return Written;
}

int vfprintf(FILE *Stream, const char *Format, va_list Args)
{
	char Buf[512];
	int Written = AvVsnprintf(Buf, sizeof(Buf), Format, Args);
	fwrite(Buf, 1, strlen(Buf), Stream);
	return Written;
}

int fprintf(FILE *Stream, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Written = vfprintf(Stream, Format, Args);
	va_end(Args);
	return Written;
}

int printf(const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Written = vfprintf(stdout, Format, Args);
	va_end(Args);
	return Written;
}

int puts(const char *S)
{
	fwrite(S, 1, strlen(S), stdout);
	fwrite("\n", 1, 1, stdout);
	return 0;
}

int putchar(int C)
{
	char Ch = (char)C;
	fwrite(&Ch, 1, 1, stdout);
	return C;
}

static FILE *AllocFile(void)
{
	for (int I = 0; I < AV_FILE_MAX; ++I) {
		if (!AvFiles[I].Used) {
			memset(&AvFiles[I], 0, sizeof(AvFiles[I]));
			AvFiles[I].Used = 1;
			AvFiles[I].Handle = FileInvalid;
			return &AvFiles[I];
		}
	}
	return NULL;
}

static const char *NormalizePath(const char *Path)
{
	while (Path && Path[0] == '.' && Path[1] == '/')
		Path += 2;
	return Path;
}

static int ShouldCacheFile(const char *Path, Offset FileSize)
{
	Size Len = strlen(Path);

	if (FileSize == 0 || FileSize > 16ul * 1024ul * 1024ul)
		return 0;

	if (Len >= 4 && strcasecmp(Path + Len - 4, ".wad") == 0)
		return 1;

	return 0;
}
FILE *fopen(const char *Path, const char *Mode)
{
	if (!Path || !Mode || Mode[0] != 'r') {
		errno = EROFS;
		return NULL;
	}

	Path = NormalizePath(Path);

	Handle H = FileOpen(Path);
	if (H == FileInvalid) {
		errno = ENOENT;
		return NULL;
	}

	FILE *F = AllocFile();
	if (!F) {
		FileClose(H);
		errno = EMFILE;
		return NULL;
	}

	F->Handle = H;
	F->Size = FileSize(H);
	F->Position = 0;

	if (ShouldCacheFile(Path, F->Size)) {
		unsigned char *Data = (unsigned char *)malloc((size_t)F->Size);
		if (Data) {
			Offset Done = 0;
			int Failed = 0;

			while (Done < F->Size) {
				SSize Got = FileRead(H, Data + Done, (Size)(F->Size - Done));
				if (Got <= 0) {
					Failed = 1;
					break;
				}
				Done += (Offset)Got;
			}

			if (!Failed && Done == F->Size) {
				FileClose(H);
				F->Handle = FileInvalid;
				F->Data = Data;
				F->Memory = 1;
				return F;
			}

			free(Data);
			FileSeek(H, 0, SEEK_SET);
		}
	}

	return F;
}

size_t fread(void *Ptr, size_t Size, size_t Count, FILE *Stream)
{
	if (!Stream || Stream->Console || Size == 0 || Count == 0)
		return 0;

	size_t Wanted = Size * Count;

	if (Stream->Memory) {
		Offset Left = Stream->Position < Stream->Size ?
						  Stream->Size - Stream->Position :
						  0;
		size_t Bytes = Wanted < Left ? Wanted : (size_t)Left;

		if (Bytes)
			memcpy(Ptr, Stream->Data + Stream->Position, Bytes);

		Stream->Position += (Offset)Bytes;
		if (Bytes < Wanted)
			Stream->Eof = 1;
		return Bytes / Size;
	}

	SSize Bytes = FileRead(Stream->Handle, Ptr, Wanted);
	if (Bytes < 0) {
		Stream->Error = 1;
		return 0;
	}
	Stream->Position += (Offset)Bytes;
	if ((size_t)Bytes < Wanted)
		Stream->Eof = 1;
	return (size_t)Bytes / Size;
}

size_t fwrite(const void *Ptr, size_t Size, size_t Count, FILE *Stream)
{
	size_t Bytes = Size * Count;
	if (!Stream || Bytes == 0)
		return 0;
	if (Stream->Console) {
		ConsoleWrite((const char *)Ptr, Bytes);
		return Count;
	}
	Stream->Error = 1;
	errno = EROFS;
	return 0;
}

int fseek(FILE *Stream, long OffsetValue, int Whence)
{
	if (!Stream || Stream->Console)
		return -1;

	SSize Target;
	if (Whence == SEEK_SET)
		Target = OffsetValue;
	else if (Whence == SEEK_CUR)
		Target = (SSize)Stream->Position + OffsetValue;
	else if (Whence == SEEK_END)
		Target = (SSize)Stream->Size + OffsetValue;
	else
		return -1;

	if (Target < 0 || (Offset)Target > Stream->Size) {
		Stream->Error = 1;
		return -1;
	}

	if (!Stream->Memory && FileSeek(Stream->Handle, Target, SEEK_SET) < 0) {
		Stream->Error = 1;
		return -1;
	}

	Stream->Position = (Offset)Target;
	Stream->Eof = 0;
	return 0;
}

long ftell(FILE *Stream)
{
	if (!Stream || Stream->Console)
		return -1;
	return (long)Stream->Position;
}

int fclose(FILE *Stream)
{
	if (!Stream || Stream->Console)
		return 0;
	if (Stream->Memory && Stream->Data)
		free(Stream->Data);
	if (!Stream->Memory && Stream->Handle != FileInvalid)
		FileClose(Stream->Handle);
	memset(Stream, 0, sizeof(*Stream));
	return 0;
}

int fflush(FILE *Stream)
{
	(void)Stream;
	return 0;
}

int feof(FILE *Stream)
{
	return Stream ? Stream->Eof : 1;
}

int ferror(FILE *Stream)
{
	return Stream ? Stream->Error : 1;
}

int fgetc(FILE *Stream)
{
	unsigned char Ch;
	return fread(&Ch, 1, 1, Stream) == 1 ? (int)Ch : EOF;
}

char *fgets(char *S, int Size, FILE *Stream)
{
	if (!S || Size <= 0)
		return NULL;
	int I = 0;
	while (I + 1 < Size) {
		int C = fgetc(Stream);
		if (C == EOF)
			break;
		S[I++] = (char)C;
		if (C == '\n')
			break;
	}
	S[I] = '\0';
	return I ? S : NULL;
}

int fputs(const char *S, FILE *Stream)
{
	return fwrite(S, 1, strlen(S), Stream) ? 0 : EOF;
}

void perror(const char *S)
{
	if (S)
		fprintf(stderr, "%s: error %d\n", S, errno);
}

int remove(const char *Path)
{
	(void)Path;
	errno = EROFS;
	return -1;
}

int rename(const char *Old, const char *New)
{
	(void)Old;
	(void)New;
	errno = EROFS;
	return -1;
}

int stat(const char *Path, struct stat *St)
{
	Path = NormalizePath(Path);
	Handle H = FileOpen(Path);
	if (H == FileInvalid) {
		errno = ENOENT;
		return -1;
	}
	if (St) {
		memset(St, 0, sizeof(*St));
		St->st_size = (off_t)FileSize(H);
		St->st_mode = S_IFREG;
	}
	FileClose(H);
	return 0;
}

int mkdir(const char *Path, unsigned int Mode)
{
	(void)Path;
	(void)Mode;
	errno = EROFS;
	return -1;
}

int access(const char *Path, int Mode)
{
	(void)Mode;
	Path = NormalizePath(Path);
	Handle H = FileOpen(Path);
	if (H == FileInvalid) {
		errno = ENOENT;
		return -1;
	}
	FileClose(H);
	return 0;
}

unsigned int sleep(unsigned int Seconds)
{
	U64 Frequency = TimeFrequency();
	U64 Target = TimeTicks() + Frequency * Seconds;
	while (TimeTicks() < Target)
		__asm__ volatile("pause" ::: "memory");
	return 0;
}

int usleep(useconds_t Usec)
{
	U64 Frequency = TimeFrequency();
	U64 Target = TimeTicks() + (Frequency * (U64)Usec) / 1000000u;
	while (TimeTicks() < Target)
		__asm__ volatile("pause" ::: "memory");
	return 0;
}

DIR *opendir(const char *Path)
{
	Handle H = DirOpen(Path);
	if (H == DirInvalid) {
		errno = ENOENT;
		return NULL;
	}
	for (int I = 0; I < AV_DIR_MAX; ++I) {
		if (!AvDirs[I].Used) {
			memset(&AvDirs[I], 0, sizeof(AvDirs[I]));
			AvDirs[I].Used = 1;
			AvDirs[I].Handle = H;
			return &AvDirs[I];
		}
	}
	DirClose(H);
	errno = EMFILE;
	return NULL;
}

struct dirent *readdir(DIR *Dir)
{
	DirEntry Entry;
	if (!Dir || DirRead(Dir->Handle, &Entry) == 0)
		return NULL;
	memset(&Dir->Entry, 0, sizeof(Dir->Entry));
	strncpy(Dir->Entry.d_name, Entry.Name, sizeof(Dir->Entry.d_name) - 1);
	return &Dir->Entry;
}

int closedir(DIR *Dir)
{
	if (!Dir)
		return -1;
	DirClose(Dir->Handle);
	memset(Dir, 0, sizeof(*Dir));
	return 0;
}

int sscanf(const char *Str, const char *Format, ...)
{
	va_list Args;
	va_start(Args, Format);
	int Assigned = 0;

	while (*Format && *Str) {
		if (IsSpace(*Format)) {
			while (IsSpace(*Format))
				Format++;
			while (IsSpace(*Str))
				Str++;
			continue;
		}
		if (*Format != '%') {
			if (*Format++ != *Str++)
				break;
			continue;
		}
		Format++;
		while (IsSpace(*Str))
			Str++;
		if (*Format == 'd' || *Format == 'i') {
			int *Out = va_arg(Args, int *);
			char *End;
			*Out = (int)strtol(Str, &End, *Format == 'i' ? 0 : 10);
			if (End == Str)
				break;
			Str = End;
			Assigned++;
		} else if (*Format == 'u') {
			unsigned int *Out = va_arg(Args, unsigned int *);
			char *End;
			*Out = (unsigned int)strtoul(Str, &End, 10);
			if (End == Str)
				break;
			Str = End;
			Assigned++;
		} else if (*Format == 's') {
			char *Out = va_arg(Args, char *);
			if (!*Str)
				break;
			while (*Str && !IsSpace(*Str))
				*Out++ = *Str++;
			*Out = '\0';
			Assigned++;
		} else {
			break;
		}
		Format++;
	}

	va_end(Args);
	return Assigned;
}

intmax_t imaxabs(intmax_t N)
{
	return N < 0 ? -N : N;
}

imaxdiv_t imaxdiv(intmax_t Numer, intmax_t Denom)
{
	imaxdiv_t Result;
	Result.quot = Numer / Denom;
	Result.rem = Numer % Denom;
	return Result;
}

intmax_t strtoimax(const char *Nptr, char **Endptr, int Base)
{
	return (intmax_t)strtol(Nptr, Endptr, Base);
}

uintmax_t strtoumax(const char *Nptr, char **Endptr, int Base)
{
	return (uintmax_t)strtoul(Nptr, Endptr, Base);
}

char *strerror(int Errnum)
{
	switch (Errnum) {
	case 0:
		return "no error";
	case ENOENT:
		return "not found";
	case EIO:
		return "i/o error";
	case EBADF:
		return "bad file";
	case ENOMEM:
		return "out of memory";
	case EACCES:
		return "access denied";
	case EEXIST:
		return "exists";
	case ENOTDIR:
		return "not a directory";
	case EISDIR:
		return "is a directory";
	case EINVAL:
		return "invalid argument";
	case EMFILE:
		return "too many files";
	case EROFS:
		return "read-only filesystem";
	default:
		return "error";
	}
}

int gettimeofday(struct timeval *Tv, struct timezone *Tz)
{
	U64 Frequency = TimeFrequency();
	U64 Ticks = TimeTicks();

	if (Tz) {
		Tz->tz_minuteswest = 0;
		Tz->tz_dsttime = 0;
	}

	if (!Tv)
		return 0;

	if (Frequency == 0) {
		Tv->tv_sec = 0;
		Tv->tv_usec = 0;
		return 0;
	}

	Tv->tv_sec = (time_t)(Ticks / Frequency);
	Tv->tv_usec = (suseconds_t)(((Ticks % Frequency) * 1000000ull) / Frequency);
	return 0;
}