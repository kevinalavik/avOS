#ifndef ADK_MAIN_H
#define ADK_MAIN_H

#include <System/Types.h>
#include <System/Pulse.h>
#include <Adk/AdkAether.h>

typedef struct AdkAppContext AdkAppContext;

typedef struct {
	const char *Title;
	U32 WindowWidth;
	U32 WindowHeight;
	U32 WindowKind;
	Bool (*Initialize)(AdkAppContext *Context);
	void (*Update)(AdkAppContext *Context);
	void (*CharInput)(AdkAppContext *Context, char Ch);
	void (*Pulse)(AdkAppContext *Context, const PulseMessage *Message);
	void (*Shutdown)(AdkAppContext *Context);
} AdkApplication;

struct AdkAppContext {
	ProcessId ProcessId;
	ProcessId ServerProcessId;
	U32 WindowId;
	Bool IsRunning;
	void *UserData;
	void *RawBuffer;
	U32 RawBufferWidth;
	U32 RawBufferHeight;
};

int AdkRunApplication(const AdkApplication *Application);
void AdkAppRequestExit(AdkAppContext *Context);
Bool AdkAppPostLine(AdkAppContext *Context, const char *Text);
Bool AdkAppUpdatePrompt(AdkAppContext *Context, const char *CurrentPath,
						const char *Input);
Bool AdkAppClearTerminal(AdkAppContext *Context);
Bool AdkAppSpawnGraphical(AdkAppContext *Context, const char *Path);

#endif
