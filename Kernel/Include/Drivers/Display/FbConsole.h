#ifndef DRIVERS_DISPLAY_FBCONSOLE_H
#define DRIVERS_DISPLAY_FBCONSOLE_H

#include <Drivers/Device.h>
#include <stdbool.h>
#include <stddef.h>

struct flanterm_context;

extern Driver FbConsoleDriver;
extern Device FbConsoleDevice;

void FbConsoleInit(struct flanterm_context *Ctx);
bool FbConsoleAvailable(void);
void ConsoleFbClaimed(void);
void FbConsoleForceClaim(void);

#endif
