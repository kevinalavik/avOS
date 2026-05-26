#include <Adk/AdkImage.h>
#include <System/Fileio.h>
#include <System/Memory.h>
#include <Lib/Stdio.h>

#define TGA_SEEK_CUR 1

typedef struct {
	U8 IdLength;
	U8 ColorMapType;
	U8 ImageType;
	U16 ColorMapOrigin;
	U16 ColorMapLength;
	U8 ColorMapEntrySize;
	U16 XOrigin;
	U16 YOrigin;
	U16 Width;
	U16 Height;
	U8 Bpp;
	U8 Descriptor;
} __attribute__((packed)) TgaHeader;

#define TGA_TYPE_COLOR_MAPPED 1
#define TGA_TYPE_UNCOMPRESSED 2
#define TGA_TYPE_GRAYSCALE 3
#define TGA_TYPE_RLE_COLOR_MAPPED 9
#define TGA_TYPE_RLE 10
#define TGA_TYPE_RLE_GRAYSCALE 11

static U64 ReadLE16(const U8 *Buf)
{
	return (U64)Buf[0] | ((U64)Buf[1] << 8);
}

static U32 UnpackPixel(const U8 *P, U8 EntryBytes)
{
	switch (EntryBytes) {
	case 2: {
		U16 V = (U16)ReadLE16(P);
		U8 A = (V & 0x8000) ? 255 : 255;
		U8 R = (U8)(((V >> 10) & 0x1F) * 255 / 31);
		U8 G = (U8)(((V >> 5) & 0x1F) * 255 / 31);
		U8 B = (U8)(((V >> 0) & 0x1F) * 255 / 31);
		return (U32)B | ((U32)G << 8) | ((U32)R << 16) | ((U32)A << 24);
	}
	case 3:
		return (U32)P[0] | ((U32)P[1] << 8) | ((U32)P[2] << 16) | 0xFF000000u;
	case 4:
		return (U32)P[0] | ((U32)P[1] << 8) | ((U32)P[2] << 16) |
			   ((U32)P[3] << 24);
	default:
		return 0xFF000000u;
	}
}

static void PlacePixel(U32 *Pixels, U64 TotalPixel, U16 Width, U16 Height,
					   Bool BottomUp, U32 Color)
{
	U64 Y = TotalPixel / Width;
	U64 X = TotalPixel % Width;
	U64 OutY = BottomUp ? (Height - 1 - Y) : Y;
	Pixels[OutY * Width + X] = Color;
}

static int LoadColorMap(Handle Fd, const TgaHeader *Hdr, U8 **OutMap)
{
	U8 EntryBytes = (Hdr->ColorMapEntrySize + 7) / 8;
	U64 MapBytes = (U64)Hdr->ColorMapLength * EntryBytes;

	if (Hdr->ColorMapOrigin > 0) {
		U64 SkipBytes = (U64)Hdr->ColorMapOrigin * EntryBytes;
		U8 Dummy;
		for (U64 I = 0; I < SkipBytes; I++) {
			if (FileRead(Fd, &Dummy, 1) != 1)
				return -1;
		}
	}

	U8 *Map = (U8 *)MemoryAlloc(MapBytes);
	if (!Map)
		return -1;

	if ((U64)FileRead(Fd, Map, MapBytes) != MapBytes) {
		MemoryFree(Map);
		return -1;
	}

	*OutMap = Map;
	return 0;
}

static int LoadUncompressedTrueColor(Handle Fd, const TgaHeader *Hdr,
									 AdkImage *Out)
{
	U8 BppBytes = Hdr->Bpp / 8;
	U64 RowBytes = (U64)Hdr->Width * BppBytes;
	U64 DataSize = RowBytes * Hdr->Height;
	U64 PixelCount = (U64)Hdr->Width * Hdr->Height;
	Bool BottomUp = (Hdr->Descriptor & (1u << 5)) == 0;

	Out->Pixels = (U32 *)AdkObjectAlloc(Out->Mgr, PixelCount * 4);
	if (!Out->Pixels)
		return -1;

	U8 *Raw = (U8 *)MemoryAlloc(DataSize);
	if (!Raw) {
		AdkObjectFree(Out->Mgr, Out->Pixels);
		Out->Pixels = 0;
		return -1;
	}

	if ((U64)FileRead(Fd, Raw, DataSize) != DataSize) {
		MemoryFree(Raw);
		AdkObjectFree(Out->Mgr, Out->Pixels);
		Out->Pixels = 0;
		return -1;
	}

	for (U64 Y = 0; Y < Hdr->Height; Y++) {
		const U8 *Row = Raw + Y * RowBytes;
		U64 OutY = BottomUp ? (Hdr->Height - 1 - Y) : Y;
		U64 Base = OutY * Hdr->Width;
		for (U64 X = 0; X < Hdr->Width; X++)
			Out->Pixels[Base + X] = UnpackPixel(Row + X * BppBytes, BppBytes);
	}

	MemoryFree(Raw);
	return 0;
}

static int LoadUncompressedColorMapped(Handle Fd, const TgaHeader *Hdr,
									   AdkImage *Out)
{
	if (Hdr->ColorMapType != 1)
		return -1;

	U8 EntryBytes = (Hdr->ColorMapEntrySize + 7) / 8;
	U64 PixelCount = (U64)Hdr->Width * Hdr->Height;
	Bool BottomUp = (Hdr->Descriptor & (1u << 5)) == 0;

	U8 *Map = 0;
	if (LoadColorMap(Fd, Hdr, &Map) != 0)
		return -1;

	Out->Pixels = (U32 *)AdkObjectAlloc(Out->Mgr, PixelCount * 4);
	if (!Out->Pixels) {
		MemoryFree(Map);
		return -1;
	}

	U8 IdxBytes = Hdr->Bpp / 8;
	U64 DataSize = PixelCount * IdxBytes;

	U8 *Raw = (U8 *)MemoryAlloc(DataSize);
	if (!Raw) {
		MemoryFree(Map);
		AdkObjectFree(Out->Mgr, Out->Pixels);
		Out->Pixels = 0;
		return -1;
	}

	if ((U64)FileRead(Fd, Raw, DataSize) != DataSize) {
		MemoryFree(Raw);
		MemoryFree(Map);
		AdkObjectFree(Out->Mgr, Out->Pixels);
		Out->Pixels = 0;
		return -1;
	}

	for (U64 I = 0; I < PixelCount; I++) {
		U64 Idx = (IdxBytes == 2) ? (U64)ReadLE16(Raw + I * 2) : (U64)Raw[I];
		U32 Color = 0xFF000000u;
		if (Idx < Hdr->ColorMapLength)
			Color = UnpackPixel(Map + Idx * EntryBytes, EntryBytes);
		PlacePixel(Out->Pixels, I, Hdr->Width, Hdr->Height, BottomUp, Color);
	}

	MemoryFree(Raw);
	MemoryFree(Map);
	return 0;
}

static int LoadUncompressedGrayscale(Handle Fd, const TgaHeader *Hdr,
									 AdkImage *Out)
{
	U64 PixelCount = (U64)Hdr->Width * Hdr->Height;
	Bool BottomUp = (Hdr->Descriptor & (1u << 5)) == 0;

	Out->Pixels = (U32 *)AdkObjectAlloc(Out->Mgr, PixelCount * 4);
	if (!Out->Pixels)
		return -1;

	U8 *Raw = (U8 *)MemoryAlloc(PixelCount);
	if (!Raw) {
		AdkObjectFree(Out->Mgr, Out->Pixels);
		Out->Pixels = 0;
		return -1;
	}

	if ((U64)FileRead(Fd, Raw, PixelCount) != PixelCount) {
		MemoryFree(Raw);
		AdkObjectFree(Out->Mgr, Out->Pixels);
		Out->Pixels = 0;
		return -1;
	}

	for (U64 Y = 0; Y < Hdr->Height; Y++) {
		U64 OutY = BottomUp ? (Hdr->Height - 1 - Y) : Y;
		U64 Base = OutY * Hdr->Width;
		for (U64 X = 0; X < Hdr->Width; X++) {
			U8 V = Raw[Y * Hdr->Width + X];
			Out->Pixels[Base + X] =
				(U32)V | ((U32)V << 8) | ((U32)V << 16) | 0xFF000000u;
		}
	}

	MemoryFree(Raw);
	return 0;
}

static int LoadRleTrueColor(Handle Fd, const TgaHeader *Hdr, AdkImage *Out)
{
	U64 PixelCount = (U64)Hdr->Width * Hdr->Height;
	U8 BppBytes = Hdr->Bpp / 8;
	Bool BottomUp = (Hdr->Descriptor & (1u << 5)) == 0;

	Out->Pixels = (U32 *)AdkObjectAlloc(Out->Mgr, PixelCount * 4);
	if (!Out->Pixels)
		return -1;

	U64 Total = 0;
	U8 Packet[5];

	while (Total < PixelCount) {
		if (FileRead(Fd, Packet, 1) != 1)
			goto fail;

		Bool RleRun = (Packet[0] & 0x80) != 0;
		U64 Count = (U64)(Packet[0] & 0x7F) + 1;

		if (RleRun) {
			if ((Size)FileRead(Fd, Packet + 1, BppBytes) != BppBytes)
				goto fail;
			U32 Color = UnpackPixel(Packet + 1, BppBytes);
			for (U64 I = 0; I < Count && Total < PixelCount; I++, Total++)
				PlacePixel(Out->Pixels, Total, Hdr->Width, Hdr->Height,
						   BottomUp, Color);
		} else {
			for (U64 I = 0; I < Count && Total < PixelCount; I++, Total++) {
				if ((Size)FileRead(Fd, Packet, BppBytes) != BppBytes)
					goto fail;
				PlacePixel(Out->Pixels, Total, Hdr->Width, Hdr->Height,
						   BottomUp, UnpackPixel(Packet, BppBytes));
			}
		}
	}
	return 0;

fail:
	AdkObjectFree(Out->Mgr, Out->Pixels);
	Out->Pixels = 0;
	return -1;
}

static int LoadRleColorMapped(Handle Fd, const TgaHeader *Hdr, AdkImage *Out)
{
	if (Hdr->ColorMapType != 1)
		return -1;

	U8 EntryBytes = (Hdr->ColorMapEntrySize + 7) / 8;
	U8 IdxBytes = Hdr->Bpp / 8;
	U64 PixelCount = (U64)Hdr->Width * Hdr->Height;
	Bool BottomUp = (Hdr->Descriptor & (1u << 5)) == 0;

	U8 *Map = 0;
	if (LoadColorMap(Fd, Hdr, &Map) != 0)
		return -1;

	Out->Pixels = (U32 *)AdkObjectAlloc(Out->Mgr, PixelCount * 4);
	if (!Out->Pixels) {
		MemoryFree(Map);
		return -1;
	}

	U64 Total = 0;
	U8 Packet[3];

	while (Total < PixelCount) {
		if (FileRead(Fd, Packet, 1) != 1)
			goto fail;

		Bool RleRun = (Packet[0] & 0x80) != 0;
		U64 Count = (U64)(Packet[0] & 0x7F) + 1;

		if (RleRun) {
			if ((Size)FileRead(Fd, Packet + 1, IdxBytes) != IdxBytes)
				goto fail;
			U64 Idx =
				(IdxBytes == 2) ? (U64)ReadLE16(Packet + 1) : (U64)Packet[1];
			U32 Color = (Idx < Hdr->ColorMapLength) ?
							UnpackPixel(Map + Idx * EntryBytes, EntryBytes) :
							0xFF000000u;
			for (U64 I = 0; I < Count && Total < PixelCount; I++, Total++)
				PlacePixel(Out->Pixels, Total, Hdr->Width, Hdr->Height,
						   BottomUp, Color);
		} else {
			for (U64 I = 0; I < Count && Total < PixelCount; I++, Total++) {
				if ((Size)FileRead(Fd, Packet + 1, IdxBytes) != IdxBytes)
					goto fail;
				U64 Idx = (IdxBytes == 2) ? (U64)ReadLE16(Packet + 1) :
											(U64)Packet[1];
				U32 Color =
					(Idx < Hdr->ColorMapLength) ?
						UnpackPixel(Map + Idx * EntryBytes, EntryBytes) :
						0xFF000000u;
				PlacePixel(Out->Pixels, Total, Hdr->Width, Hdr->Height,
						   BottomUp, Color);
			}
		}
	}

	MemoryFree(Map);
	return 0;

fail:
	MemoryFree(Map);
	AdkObjectFree(Out->Mgr, Out->Pixels);
	Out->Pixels = 0;
	return -1;
}

static int LoadRleGrayscale(Handle Fd, const TgaHeader *Hdr, AdkImage *Out)
{
	U64 PixelCount = (U64)Hdr->Width * Hdr->Height;
	Bool BottomUp = (Hdr->Descriptor & (1u << 5)) == 0;

	Out->Pixels = (U32 *)AdkObjectAlloc(Out->Mgr, PixelCount * 4);
	if (!Out->Pixels)
		return -1;

	U64 Total = 0;
	U8 Byte;

	while (Total < PixelCount) {
		U8 Hdr2;
		if (FileRead(Fd, &Hdr2, 1) != 1)
			goto fail;

		Bool RleRun = (Hdr2 & 0x80) != 0;
		U64 Count = (U64)(Hdr2 & 0x7F) + 1;

		if (RleRun) {
			if (FileRead(Fd, &Byte, 1) != 1)
				goto fail;
			U32 Color =
				(U32)Byte | ((U32)Byte << 8) | ((U32)Byte << 16) | 0xFF000000u;
			for (U64 I = 0; I < Count && Total < PixelCount; I++, Total++)
				PlacePixel(Out->Pixels, Total, Hdr->Width, Hdr->Height,
						   BottomUp, Color);
		} else {
			for (U64 I = 0; I < Count && Total < PixelCount; I++, Total++) {
				if (FileRead(Fd, &Byte, 1) != 1)
					goto fail;
				U32 Color = (U32)Byte | ((U32)Byte << 8) | ((U32)Byte << 16) |
							0xFF000000u;
				PlacePixel(Out->Pixels, Total, Hdr->Width, Hdr->Height,
						   BottomUp, Color);
			}
		}
	}
	return 0;

fail:
	AdkObjectFree(Out->Mgr, Out->Pixels);
	Out->Pixels = 0;
	return -1;
}

AdkImage *AdkImageLoadTga(AdkObjectManager *Mgr, const char *Path)
{
	if (!Mgr || !Path)
		return 0;

	Handle Fd = FileOpen(Path);
	if (Fd == FileInvalid)
		return 0;

	U8 Raw[18];
	if (FileRead(Fd, Raw, 18) != 18) {
		FileClose(Fd);
		return 0;
	}

	TgaHeader Hdr;
	Hdr.IdLength = Raw[0];
	Hdr.ColorMapType = Raw[1];
	Hdr.ImageType = Raw[2];
	Hdr.ColorMapOrigin = (U16)ReadLE16(Raw + 3);
	Hdr.ColorMapLength = (U16)ReadLE16(Raw + 5);
	Hdr.ColorMapEntrySize = Raw[7];
	Hdr.XOrigin = (U16)ReadLE16(Raw + 8);
	Hdr.YOrigin = (U16)ReadLE16(Raw + 10);
	Hdr.Width = (U16)ReadLE16(Raw + 12);
	Hdr.Height = (U16)ReadLE16(Raw + 14);
	Hdr.Bpp = Raw[16];
	Hdr.Descriptor = Raw[17];

	Bool Valid = 0;
	switch (Hdr.ImageType) {
	case TGA_TYPE_COLOR_MAPPED:
	case TGA_TYPE_RLE_COLOR_MAPPED:
		Valid = (Hdr.ColorMapType == 1) && (Hdr.Bpp == 8 || Hdr.Bpp == 16) &&
				(Hdr.ColorMapEntrySize == 15 || Hdr.ColorMapEntrySize == 16 ||
				 Hdr.ColorMapEntrySize == 24 || Hdr.ColorMapEntrySize == 32);
		break;
	case TGA_TYPE_UNCOMPRESSED:
	case TGA_TYPE_RLE:
		Valid = (Hdr.ColorMapType == 0) &&
				(Hdr.Bpp == 16 || Hdr.Bpp == 24 || Hdr.Bpp == 32);
		break;
	case TGA_TYPE_GRAYSCALE:
	case TGA_TYPE_RLE_GRAYSCALE:
		Valid = (Hdr.ColorMapType == 0) && (Hdr.Bpp == 8);
		break;
	}

	if (!Valid || Hdr.Width == 0 || Hdr.Height == 0) {
		FileClose(Fd);
		return 0;
	}

	if (Hdr.IdLength > 0)
		FileSeek(Fd, (SSize)Hdr.IdLength, TGA_SEEK_CUR);

	AdkImage *Img = (AdkImage *)AdkObjectAlloc(Mgr, sizeof(AdkImage));
	if (!Img) {
		FileClose(Fd);
		return 0;
	}
	Img->Pixels = 0;
	Img->Width = Hdr.Width;
	Img->Height = Hdr.Height;
	Img->Bpp = Hdr.Bpp;
	Img->Mgr = Mgr;

	int Result = -1;
	switch (Hdr.ImageType) {
	case TGA_TYPE_COLOR_MAPPED:
		Result = LoadUncompressedColorMapped(Fd, &Hdr, Img);
		break;
	case TGA_TYPE_UNCOMPRESSED:
		Result = LoadUncompressedTrueColor(Fd, &Hdr, Img);
		break;
	case TGA_TYPE_GRAYSCALE:
		Result = LoadUncompressedGrayscale(Fd, &Hdr, Img);
		break;
	case TGA_TYPE_RLE_COLOR_MAPPED:
		Result = LoadRleColorMapped(Fd, &Hdr, Img);
		break;
	case TGA_TYPE_RLE:
		Result = LoadRleTrueColor(Fd, &Hdr, Img);
		break;
	case TGA_TYPE_RLE_GRAYSCALE:
		Result = LoadRleGrayscale(Fd, &Hdr, Img);
		break;
	}

	if (Result != 0) {
		AdkObjectFree(Mgr, Img);
		FileClose(Fd);
		return 0;
	}

	FileClose(Fd);
	return Img;
}

void AdkImageFree(AdkObjectManager *Mgr, AdkImage *Img)
{
	if (!Mgr || !Img)
		return;
	if (Img->Pixels)
		AdkObjectFree(Mgr, Img->Pixels);
	AdkObjectFree(Mgr, Img);
}