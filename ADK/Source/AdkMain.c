#include <Adk/AdkMain.h>
#include <Adk/AdkAether.h>
#include <Lib/String.h>
#include <System/Memory.h>
#include <System/Process.h>
#include <System/Pulse.h>

static Bool AdkPostMessage(ProcessId TargetId, U32 Type, const void *Payload,
						   U32 Size)
{
	return PulseSend(TargetId, Type, Payload, Size) == 0;
}

int AdkRunApplication(const AdkApplication *Application)
{
	AdkAppContext Context;

	for (Size Index = 0; Index < sizeof(Context); Index++)
		((Byte *)&Context)[Index] = 0;

	Context.ProcessId = ProcessCurrentId();
	Context.ServerProcessId = ProcessParentId();
	Context.WindowId = 0;
	Context.IsRunning = True;

	if (Application == 0 || Context.ServerProcessId == 0)
		return 1;

	AdkAetherCreateWindowMessage CreateMessage;
	CreateMessage.Kind = Application->WindowKind;
	CreateMessage.Width = Application->WindowWidth;
	CreateMessage.Height = Application->WindowHeight;
	StringCopy(CreateMessage.Title, sizeof(CreateMessage.Title),
			   Application->Title);

	if (!AdkPostMessage(Context.ServerProcessId, AdkAetherMessageCreateWindow,
						&CreateMessage, sizeof(CreateMessage)))
		return 1;

	if (Application->Initialize && !Application->Initialize(&Context))
		return 1;

	while (Context.IsRunning) {
		PulseMessage Message;
		S64 Status;

		do {
			Status = PulseReceive(&Message);
			if (Status <= 0)
				break;

			if (Message.Type == AdkAetherMessageWindowReady &&
				Message.Size >= sizeof(AdkAetherWindowReadyMessage)) {
				AdkAetherWindowReadyMessage *Ready =
					(AdkAetherWindowReadyMessage *)Message.Payload;
				Context.WindowId = Ready->WindowId;
				if (Ready->SharedMemId != 0) {
					Context.RawBuffer = SharedMemMap(Ready->SharedMemId);
					if (Context.RawBuffer == 0) {
						return 1;
					}
					Context.RawBufferWidth = Application->WindowWidth;
					Context.RawBufferHeight = Application->WindowHeight;
				}
			} else if (Message.Type == AdkAetherMessageKeyInput) {
				AdkAetherKeyInputMessage Compat;
				AdkAetherKeyInputMessage *Input = &Compat;

				Compat.Char = 0;
				Compat.Key = 0;
				Compat.Pressed = 1;
				Compat.Modifiers = 0;

				if (Message.Size >= sizeof(AdkAetherKeyInputMessage)) {
					Input = (AdkAetherKeyInputMessage *)Message.Payload;
				} else if (Message.Size >= 1) {
					Compat.Char = (char)Message.Payload[0];
					Compat.Key = (U16)(U8)Compat.Char;
				} else {
					Input = 0;
				}

				if (Input != 0) {
					if (Application->Pulse)
						Application->Pulse(&Context, &Message);
					if (Application->CharInput && Input->Pressed && Input->Char)
						Application->CharInput(&Context, Input->Char);
				}
			} else if (Message.Type == AdkAetherMessageClose) {
				Context.IsRunning = False;
			} else if (Application->Pulse) {
				Application->Pulse(&Context, &Message);
			}
		} while (Status > 0);

		if (Application->Update)
			Application->Update(&Context);

		if (Application->WindowKind == AdkAetherWindowKindRaw &&
			Context.RawBuffer != 0) {
			AdkPostMessage(Context.ServerProcessId,
						   AdkAetherMessageRawFrameReady, 0, 0);
		}

		__asm__ volatile("pause" ::: "memory");
	}

	if (Application->Shutdown)
		Application->Shutdown(&Context);

	return 0;
}

void AdkAppRequestExit(AdkAppContext *Context)
{
	if (Context)
		Context->IsRunning = False;
}

Bool AdkAppPostLine(AdkAppContext *Context, const char *Text)
{
	AdkAetherTerminalLineMessage Message;

	if (Context == 0 || Text == 0)
		return False;

	StringCopy(Message.Text, sizeof(Message.Text), Text);
	return AdkPostMessage(Context->ServerProcessId,
						  AdkAetherMessageTerminalLine, &Message,
						  sizeof(Message));
}

Bool AdkAppUpdatePrompt(AdkAppContext *Context, const char *CurrentPath,
						const char *Input)
{
	AdkAetherTerminalPromptMessage Message;

	if (Context == 0)
		return False;

	StringCopy(Message.CurrentPath, sizeof(Message.CurrentPath),
			   CurrentPath != 0 ? CurrentPath : "");
	StringCopy(Message.Input, sizeof(Message.Input), Input != 0 ? Input : "");
	return AdkPostMessage(Context->ServerProcessId,
						  AdkAetherMessageTerminalPrompt, &Message,
						  sizeof(Message));
}

Bool AdkAppClearTerminal(AdkAppContext *Context)
{
	if (Context == 0)
		return False;

	return AdkPostMessage(Context->ServerProcessId,
						  AdkAetherMessageTerminalClear, 0, 0);
}

Bool AdkAppSpawnGraphical(AdkAppContext *Context, const char *Path)
{
	AdkAetherSpawnRequestMessage Message;

	if (Context == 0 || Path == 0)
		return False;

	StringCopy(Message.Path, sizeof(Message.Path), Path);
	return AdkPostMessage(Context->ServerProcessId,
						  AdkAetherMessageSpawnRequest, &Message,
						  sizeof(Message));
}
