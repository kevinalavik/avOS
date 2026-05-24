#include <Device/PortIO.h>
#include <Device/TextConsole.h>

#include <stddef.h>
#include <stdint.h>

#define VgaTextAddress 0xB8000
#define VgaTextWidth 80
#define VgaTextHeight 25
#define VgaTextCells (VgaTextWidth * VgaTextHeight)
#define VgaDefaultForeground 7
#define VgaDefaultBackground 0
#define VgaCursorIndexPort 0x3D4
#define VgaCursorDataPort 0x3D5
#define VgaCursorLow 0x0F
#define VgaCursorHigh 0x0E

static volatile uint16_t *TextBuffer =
	(volatile uint16_t *)(uintptr_t)VgaTextAddress;
static size_t CursorX;
static size_t CursorY;
static uint8_t CurrentForeground;
static uint8_t CurrentBackground;
static bool Ready;
static bool Escape;
static bool ControlSequence;
static bool Bold;

static uint8_t TextAttribute(void)
{
	return (uint8_t)((CurrentBackground << 4) | (CurrentForeground & 0x0Fu));
}

static uint16_t TextCell(char Character)
{
	return (uint16_t)((uint16_t)TextAttribute() << 8) | (uint8_t)Character;
}

static uint16_t ReadCursor(void)
{
	PortIOWrite8(VgaCursorIndexPort, VgaCursorLow);
	uint16_t Position = PortIORead8(VgaCursorDataPort);
	PortIOWrite8(VgaCursorIndexPort, VgaCursorHigh);
	Position |= (uint16_t)PortIORead8(VgaCursorDataPort) << 8;

	return Position;
}

static void UpdateCursor(void)
{
	uint16_t Position = (uint16_t)((CursorY * VgaTextWidth) + CursorX);

	PortIOWrite8(VgaCursorIndexPort, VgaCursorLow);
	PortIOWrite8(VgaCursorDataPort, (uint8_t)(Position & 0xFFu));
	PortIOWrite8(VgaCursorIndexPort, VgaCursorHigh);
	PortIOWrite8(VgaCursorDataPort, (uint8_t)(Position >> 8));
}

static uint8_t AnsiToVgaColor(unsigned int Color, bool Bright)
{
	static const uint8_t BaseColors[] = {
		0x00u, /* black */
		0x04u, /* red */
		0x02u, /* green */
		0x06u, /* yellow/brown */
		0x01u, /* blue */
		0x05u, /* magenta */
		0x03u, /* cyan */
		0x07u, /* white/light gray */
	};

	uint8_t VgaColor = BaseColors[Color & 7u];
	if (Bright) {
		VgaColor |= 0x08u;
	}

	return VgaColor;
}

static void ResetStyle(void)
{
	CurrentForeground = VgaDefaultForeground;
	CurrentBackground = VgaDefaultBackground;
	Bold = false;
}

static void ApplySgrParameter(unsigned int Parameter)
{
	if (Parameter == 0) {
		ResetStyle();
	} else if (Parameter == 1) {
		Bold = true;
	} else if (Parameter == 22) {
		Bold = false;
	} else if (Parameter == 39) {
		CurrentForeground = VgaDefaultForeground;
	} else if (Parameter == 49) {
		CurrentBackground = VgaDefaultBackground;
	} else if (Parameter >= 30 && Parameter <= 37) {
		CurrentForeground = AnsiToVgaColor(Parameter - 30u, Bold);
	} else if (Parameter >= 40 && Parameter <= 47) {
		CurrentBackground = AnsiToVgaColor(Parameter - 40u, false);
	} else if (Parameter >= 90 && Parameter <= 97) {
		CurrentForeground = AnsiToVgaColor(Parameter - 90u, true);
	} else if (Parameter >= 100 && Parameter <= 107) {
		CurrentBackground = AnsiToVgaColor(Parameter - 100u, true);
	}
}

static void ApplySgr(const char *Sequence, size_t Length)
{
	if (Length == 0) {
		ApplySgrParameter(0);
		return;
	}

	unsigned int Parameter = 0;
	bool HasParameter = false;

	for (size_t Index = 0; Index <= Length; ++Index) {
		char Character = Index < Length ? Sequence[Index] : ';';

		if (Character >= '0' && Character <= '9') {
			Parameter = (Parameter * 10u) + (unsigned int)(Character - '0');
			HasParameter = true;
			continue;
		}

		if (Character == ';') {
			ApplySgrParameter(HasParameter ? Parameter : 0);
			Parameter = 0;
			HasParameter = false;
		}
	}
}

static void Scroll(void)
{
	for (size_t Y = 1; Y < VgaTextHeight; ++Y) {
		for (size_t X = 0; X < VgaTextWidth; ++X) {
			TextBuffer[(Y - 1u) * VgaTextWidth + X] =
				TextBuffer[Y * VgaTextWidth + X];
		}
	}

	for (size_t X = 0; X < VgaTextWidth; ++X) {
		TextBuffer[(VgaTextHeight - 1u) * VgaTextWidth + X] = TextCell(' ');
	}
}

static void NewLine(void)
{
	CursorX = 0;
	++CursorY;

	if (CursorY >= VgaTextHeight) {
		Scroll();
		CursorY = VgaTextHeight - 1u;
	}
}

static bool IsEscapeTerminator(char Character)
{
	return Character >= '@' && Character <= '~';
}

void TextConsoleInit(void)
{
	uint16_t Position = ReadCursor();

	ResetStyle();

	if (Position >= VgaTextCells) {
		Scroll();
		Position = (VgaTextHeight - 1u) * VgaTextWidth;
	}

	CursorX = Position % VgaTextWidth;
	CursorY = Position / VgaTextWidth;
	Escape = false;
	ControlSequence = false;
	Ready = true;

	UpdateCursor();
}

bool TextConsoleReady(void)
{
	return Ready;
}

void TextConsoleSetBufferAddress(uint64_t Address)
{
	if (Address != 0) {
		TextBuffer = (volatile uint16_t *)(uintptr_t)Address;
	}
}

void TextConsolePutc(char Character)
{
	static char EscapeParameters[24];
	static size_t EscapeParameterLength;

	if (!Ready) {
		return;
	}

	if (Escape) {
		if (!ControlSequence && Character == '[') {
			ControlSequence = true;
			EscapeParameterLength = 0;
			return;
		}

		if (ControlSequence && !IsEscapeTerminator(Character)) {
			if (EscapeParameterLength < sizeof(EscapeParameters)) {
				EscapeParameters[EscapeParameterLength++] = Character;
			}
			return;
		}

		if (ControlSequence && Character == 'm') {
			ApplySgr(EscapeParameters, EscapeParameterLength);
		}

		if (!ControlSequence || IsEscapeTerminator(Character)) {
			Escape = false;
			ControlSequence = false;
		}

		return;
	}

	switch (Character) {
	case '\033':
		Escape = true;
		ControlSequence = false;
		EscapeParameterLength = 0;
		return;
	case '\n':
		NewLine();
		break;
	case '\r':
		CursorX = 0;
		break;
	case '\t':
		do {
			TextConsolePutc(' ');
		} while ((CursorX & 3u) != 0);
		return;
	case '\b':
		if (CursorX > 0) {
			--CursorX;
			TextBuffer[CursorY * VgaTextWidth + CursorX] = TextCell(' ');
		}
		break;
	default:
		if ((uint8_t)Character < ' ') {
			return;
		}

		TextBuffer[CursorY * VgaTextWidth + CursorX] = TextCell(Character);
		++CursorX;

		if (CursorX >= VgaTextWidth) {
			NewLine();
		}
		break;
	}

	UpdateCursor();
}
