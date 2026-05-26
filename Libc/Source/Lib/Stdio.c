#include <Lib/Stdio.h>
#include <System/Console.h>
#include <Lib/String.h>
#include <System/Types.h>

static void PrintPad(char Ch, Size Count)
{
	for (Size i = 0; i < Count; i++)
		ConsoleWrite(&Ch, 1);
}

Size Printf(const char *Format, ...)
{
	__builtin_va_list Args;
	__builtin_va_start(Args, Format);

	Size Total = 0;

	while (*Format != '\0') {
		if (*Format != '%') {
			ConsoleWrite(Format, 1);
			Total++;
			Format++;
			continue;
		}

		Format++;

		Size Width = 0;
		char Pad = ' ';
		if (*Format == '0') {
			Pad = '0';
			Format++;
		}
		while (*Format >= '0' && *Format <= '9') {
			Width = Width * 10 + (Size)(*Format - '0');
			Format++;
		}

		while (*Format == 'l' || *Format == 'h' || *Format == 'z' ||
			   *Format == 't')
			Format++;

		switch (*Format) {
		case 's': {
			const char *Str = __builtin_va_arg(Args, const char *);
			if (Str == 0)
				Str = "(null)";
			Size Len = StringLength(Str);
			if (Width > Len)
				PrintPad(Pad, Width - Len);
			Total += ConsoleWrite(Str, Len);
			if (Width > Len)
				Total += Width - Len;
			else
				Total += Len;
			break;
		}
		case 'd':
		case 'i': {
			S64 Val = (S64) __builtin_va_arg(Args, S64);
			char Sign = 0;
			if (Val < 0) {
				Sign = '-';
				Val = -Val;
			}
			char Buf[24];
			Size Len = 0;
			if (Val == 0) {
				Buf[Len++] = '0';
			} else {
				while (Val != 0) {
					Buf[Len++] = (char)('0' + (Val % 10));
					Val /= 10;
				}
			}
			Size Actual = Len + (Sign ? 1 : 0);
			if (Width > Actual)
				PrintPad(Pad, Width - Actual);
			if (Sign) {
				ConsoleWrite(&Sign, 1);
				Total++;
			}
			for (Size i = 0; i < Len; i++)
				ConsoleWrite(&Buf[Len - 1 - i], 1);
			Total += Len;
			if (Width > Actual)
				Total += Width - Actual;
			break;
		}
		case 'u': {
			U64 Val = __builtin_va_arg(Args, U64);
			char Buf[24];
			Size Len = 0;
			if (Val == 0) {
				Buf[Len++] = '0';
			} else {
				while (Val != 0) {
					Buf[Len++] = (char)('0' + (Val % 10));
					Val /= 10;
				}
			}
			if (Width > Len)
				PrintPad(Pad, Width - Len);
			for (Size i = 0; i < Len; i++)
				ConsoleWrite(&Buf[Len - 1 - i], 1);
			Total += Len;
			if (Width > Len)
				Total += Width - Len;
			break;
		}
		case 'x':
		case 'X': {
			U64 Val = __builtin_va_arg(Args, U64);
			Size Len = 0;
			char Buf[18];
			Buf[Len++] = '0';
			Buf[Len++] = 'x';
			if (Val == 0) {
				Buf[Len++] = '0';
			} else {
				char Tmp[17];
				Size TmpLen = 0;
				while (Val != 0) {
					U64 D = Val % 16;
					Tmp[TmpLen++] =
						(char)(D < 10 ? '0' + D :
										(*Format == 'X' ? 'A' : 'a') + D - 10);
					Val /= 16;
				}
				for (Size i = 0; i < TmpLen; i++)
					Buf[Len++] = Tmp[TmpLen - 1 - i];
			}
			if (Width > Len)
				PrintPad(Pad, Width - Len);
			Total += ConsoleWrite(Buf, Len);
			if (Width > Len)
				Total += Width - Len;
			break;
		}
		case 'p': {
			void *Ptr = __builtin_va_arg(Args, void *);
			U64 Val = (U64)Ptr;
			char Buf[20];
			Size Len = 0;
			Buf[Len++] = '0';
			Buf[Len++] = 'x';
			if (Val == 0) {
				Buf[Len++] = '0';
			} else {
				char Tmp[17];
				Size TmpLen = 0;
				while (Val != 0) {
					U64 D = Val % 16;
					Tmp[TmpLen++] = (char)(D < 10 ? '0' + D : 'a' + D - 10);
					Val /= 16;
				}
				for (Size i = 0; i < TmpLen; i++)
					Buf[Len++] = Tmp[TmpLen - 1 - i];
			}
			if (Width > Len)
				PrintPad(Pad, Width - Len);
			Total += ConsoleWrite(Buf, Len);
			if (Width > Len)
				Total += Width - Len;
			break;
		}
		case 'c': {
			char Ch = (char)__builtin_va_arg(Args, int);
			if (Width > 1)
				PrintPad(Pad, Width - 1);
			ConsoleWrite(&Ch, 1);
			Total++;
			if (Width > 1)
				Total += Width - 1;
			break;
		}
		case '%':
			ConsoleWrite(Format, 1);
			Total++;
			break;
		default:
			ConsoleWrite(Format, 1);
			Total++;
			break;
		}
		Format++;
	}

	__builtin_va_end(Args);
	return Total;
}
