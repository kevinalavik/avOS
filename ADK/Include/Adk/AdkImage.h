#ifndef ADK_IMAGE_H
#define ADK_IMAGE_H

#include <System/Types.h>
#include <Adk/AdkObjectManager.h>

typedef struct {
	U32 *Pixels;
	U16 Width;
	U16 Height;
	U8 Bpp;
	AdkObjectManager *Mgr;
} AdkImage;

AdkImage *AdkImageLoadTga(AdkObjectManager *Mgr, const char *Path);
void AdkImageFree(AdkObjectManager *Mgr, AdkImage *Img);

#endif
