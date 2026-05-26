#ifndef AETHER_BACKGROUND_H
#define AETHER_BACKGROUND_H

#include <Adk/AdkBackground.h>

typedef AdkBackground AetherBackground;

void AetherBackgroundDraw(const AetherBackground *Background,
						  AdkCanvas *Canvas);

#endif
