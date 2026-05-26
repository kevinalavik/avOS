#ifndef AETHER_CURSOR_H
#define AETHER_CURSOR_H

#include <Adk/AdkCursor.h>

typedef AdkCursor AetherCursor;

void AetherCursorInit(AetherCursor *Crs, AdkImage *Img);
void AetherCursorDraw(const AetherCursor *Crs, AdkCanvas *Canvas);

#endif
