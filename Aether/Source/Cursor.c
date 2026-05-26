#include <Aether/Cursor.h>

void AetherCursorInit(AetherCursor *Crs, AdkImage *Img)
{
	AdkCursorInit(Crs, Img);
}

void AetherCursorDraw(const AetherCursor *Crs, AdkCanvas *Canvas)
{
	AdkCursorDraw(Crs, Canvas);
}
