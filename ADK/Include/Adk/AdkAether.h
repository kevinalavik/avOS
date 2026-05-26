#ifndef ADK_AETHER_H
#define ADK_AETHER_H

#include <System/Types.h>

#define ADK_AETHER_TITLE_MAX 64u
#define ADK_AETHER_LINE_MAX 128u
#define ADK_AETHER_INPUT_MAX 96u
#define ADK_AETHER_CWD_MAX 128u

typedef enum {
	AdkAetherMessageCreateWindow = 1,
	AdkAetherMessageWindowReady = 2,
	AdkAetherMessageKeyInput = 3,
	AdkAetherMessageClose = 4,
	AdkAetherMessageTerminalLine = 5,
	AdkAetherMessageTerminalPrompt = 6,
	AdkAetherMessageTerminalClear = 7,
	AdkAetherMessageSpawnRequest = 8,
	AdkAetherMessageRawFrameReady = 9,
} AdkAetherMessageType;

typedef enum {
	AdkAetherWindowKindText = 1,
	AdkAetherWindowKindRaw = 2,
} AdkAetherWindowKind;

typedef struct {
	U32 Kind;
	U32 Width;
	U32 Height;
	char Title[ADK_AETHER_TITLE_MAX];
} AdkAetherCreateWindowMessage;

typedef struct {
	U32 WindowId;
	U64 SharedMemId;
} AdkAetherWindowReadyMessage;

#define AdkKeyEscape 0x001Bu
#define AdkKeyBackspace 0x0008u
#define AdkKeyTab 0x0009u
#define AdkKeyEnter 0x000Au
#define AdkKeySpace 0x0020u
#define AdkKeyLeftShift 0x0100u
#define AdkKeyRightShift 0x0101u
#define AdkKeyLeftCtrl 0x0102u
#define AdkKeyRightCtrl 0x0103u
#define AdkKeyLeftAlt 0x0104u
#define AdkKeyRightAlt 0x0105u
#define AdkKeyUp 0x0110u
#define AdkKeyDown 0x0111u
#define AdkKeyLeft 0x0112u
#define AdkKeyRight 0x0113u

#define AdkKeyModShift 0x01u
#define AdkKeyModCtrl 0x02u
#define AdkKeyModAlt 0x04u
#define AdkKeyModCaps 0x08u

typedef struct {
	char Char;
	U16 Key;
	U8 Pressed;
	U8 Modifiers;
} AdkAetherKeyInputMessage;

typedef struct {
	char Text[ADK_AETHER_LINE_MAX];
} AdkAetherTerminalLineMessage;

typedef struct {
	char CurrentPath[ADK_AETHER_CWD_MAX];
	char Input[ADK_AETHER_INPUT_MAX];
} AdkAetherTerminalPromptMessage;

typedef struct {
	char Path[ADK_AETHER_CWD_MAX];
} AdkAetherSpawnRequestMessage;

#endif
