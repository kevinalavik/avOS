#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <Library/Printf.h>
#include <Library/Stdout.h>

void (*StdoutPutc)(char Character) = NULL;

typedef struct PrintState {
	int Written;
} PrintState;

static void EmitChar(PrintState *State, char Character)
{
	if (StdoutPutc != NULL) {
		StdoutPutc(Character);
		if (Character == '\n')
			StdoutPutc('\r');
	}
	++State->Written;
}

static void EmitPadding(PrintState *State, char Fill, int Count)
{
	while (Count-- > 0) {
		EmitChar(State, Fill);
	}
}

static void EmitUnsigned(PrintState *State, uintptr_t Value, uint32_t Base,
						 bool Uppercase, int Width, char Fill, bool LeftAlign)
{
	static const char LowerDigits[] = "0123456789abcdef";
	static const char UpperDigits[] = "0123456789ABCDEF";
	const char *Digits = Uppercase ? UpperDigits : LowerDigits;
	char Buffer[sizeof(uintptr_t) * 8u];
	int Length = 0;

	do {
		Buffer[Length++] = Digits[Value % Base];
		Value /= Base;
	} while (Value != 0);

	int Pad = Width - Length;

	if (LeftAlign) {
		while (Length-- > 0) {
			EmitChar(State, Buffer[Length]);
		}

		EmitPadding(State, Fill, Pad);
	} else {
		EmitPadding(State, Fill, Pad);

		while (Length-- > 0) {
			EmitChar(State, Buffer[Length]);
		}
	}
}

static void EmitSigned(PrintState *State, intptr_t Value, int Width, char Fill,
					   bool LeftAlign)
{
	uintptr_t Magnitude;

	if (Value < 0) {
		EmitChar(State, '-');
		Magnitude = 0u - (uintptr_t)Value;

		if (Width > 0) {
			--Width;
		}
	} else {
		Magnitude = (uintptr_t)Value;
	}

	EmitUnsigned(State, Magnitude, 10, false, Width, Fill, LeftAlign);
}

static void EmitString(PrintState *State, const char *String, int Width,
					   bool LeftAlign)
{
	if (String == NULL) {
		String = "(null)";
	}

	int Length = 0;

	while (String[Length] != '\0') {
		++Length;
	}

	if (LeftAlign) {
		int i = 0;

		while (i < Length) {
			EmitChar(State, String[i++]);
		}

		EmitPadding(State, ' ', Width - Length);
	} else {
		EmitPadding(State, ' ', Width - Length);

		int i = 0;

		while (i < Length) {
			EmitChar(State, String[i++]);
		}
	}
}

int PrintChar(int Character)
{
	if (StdoutPutc != NULL) {
		StdoutPutc((char)Character);
	}
	return Character;
}

int PrintLine(const char *String)
{
	if (StdoutPutc != NULL) {
		while (*String != '\0') {
			StdoutPutc(*String++);
		}
		StdoutPutc('\n');
		StdoutPutc('\r');
	}
	return 0;
}

int VPrintf(const char *Format, va_list Arguments)
{
	PrintState State = { 0 };

	while (*Format != '\0') {
		if (*Format != '%') {
			EmitChar(&State, *Format++);
			continue;
		}

		++Format;

		bool LeftAlign = false;
		char Fill = ' ';

		for (;;) {
			if (*Format == '-') {
				LeftAlign = true;
				++Format;
			} else if (*Format == '0') {
				Fill = '0';
				++Format;
			} else {
				break;
			}
		}

		int Width = 0;
		while (*Format >= '0' && *Format <= '9') {
			Width = (Width * 10) + (*Format - '0');
			++Format;
		}

		int Length = 0;
		if (*Format == 'l') {
			++Format;
			++Length;

			if (*Format == 'l') {
				++Format;
				++Length;
			}
		}

		switch (*Format) {
		case '%':
			EmitChar(&State, '%');
			break;
		case 'c':
			EmitChar(&State, (char)va_arg(Arguments, int));
			break;
		case 's':
			EmitString(&State, va_arg(Arguments, const char *), Width,
					   LeftAlign);
			break;
		case 'd':
		case 'i':
			switch (Length) {
			case 2:
				EmitSigned(&State, (intptr_t)va_arg(Arguments, long long),
						   Width, Fill, LeftAlign);
				break;
			case 1:
				EmitSigned(&State, (intptr_t)va_arg(Arguments, long), Width,
						   Fill, LeftAlign);
				break;
			default:
				EmitSigned(&State, va_arg(Arguments, int), Width, Fill,
						   LeftAlign);
				break;
			}
			break;
		case 'u':
			switch (Length) {
			case 2:
				EmitUnsigned(&State,
							 (uintptr_t)va_arg(Arguments, unsigned long long),
							 10, false, Width, Fill, LeftAlign);
				break;
			case 1:
				EmitUnsigned(&State,
							 (uintptr_t)va_arg(Arguments, unsigned long), 10,
							 false, Width, Fill, LeftAlign);
				break;
			default:
				EmitUnsigned(&State, va_arg(Arguments, unsigned int), 10, false,
							 Width, Fill, LeftAlign);
				break;
			}
			break;
		case 'x':
			switch (Length) {
			case 2:
				EmitUnsigned(&State,
							 (uintptr_t)va_arg(Arguments, unsigned long long),
							 16, false, Width, Fill, LeftAlign);
				break;
			case 1:
				EmitUnsigned(&State,
							 (uintptr_t)va_arg(Arguments, unsigned long), 16,
							 false, Width, Fill, LeftAlign);
				break;
			default:
				EmitUnsigned(&State, va_arg(Arguments, unsigned int), 16, false,
							 Width, Fill, LeftAlign);
				break;
			}
			break;
		case 'X':
			switch (Length) {
			case 2:
				EmitUnsigned(&State,
							 (uintptr_t)va_arg(Arguments, unsigned long long),
							 16, true, Width, Fill, LeftAlign);
				break;
			case 1:
				EmitUnsigned(&State,
							 (uintptr_t)va_arg(Arguments, unsigned long), 16,
							 true, Width, Fill, LeftAlign);
				break;
			default:
				EmitUnsigned(&State, va_arg(Arguments, unsigned int), 16, true,
							 Width, Fill, LeftAlign);
				break;
			}
			break;
		case 'p':
			EmitString(&State, "0x", 0, false);
			EmitUnsigned(&State, (uintptr_t)va_arg(Arguments, void *), 16,
						 false, (int)(sizeof(uintptr_t) * 2u), '0', false);
			break;
		case '\0':
			return State.Written;
		default:
			EmitChar(&State, '%');
			EmitChar(&State, *Format);
			break;
		}

		++Format;
	}

	return State.Written;
}

int Printf(const char *Format, ...)
{
	va_list Arguments;
	va_start(Arguments, Format);
	int Written = VPrintf(Format, Arguments);
	va_end(Arguments);

	return Written;
}
