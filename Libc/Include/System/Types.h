#ifndef SYSTEM_TYPES_H
#define SYSTEM_TYPES_H

typedef unsigned char U8;
typedef signed char S8;
typedef unsigned short U16;
typedef signed short S16;
typedef unsigned int U32;
typedef signed int S32;
typedef unsigned long long U64;
typedef signed long long S64;

typedef unsigned char Byte;
typedef unsigned short Word;
typedef unsigned int DWord;
typedef unsigned long long QWord;

typedef signed char SByte;
typedef signed short SWord;
typedef signed int SDWord;
typedef signed long long SQWord;

typedef unsigned char Bool;

typedef void *Pointer;

typedef unsigned long Size;
typedef long SSize;

typedef unsigned long Offset;
typedef long SOffset;

typedef unsigned long Handle;

typedef unsigned long ProcessId;

typedef unsigned long FileDescriptor;

typedef unsigned long DirHandle;

#define True 1
#define False 0

#define FileInvalid ((Handle) - 1)
#define DirInvalid ((Handle) - 1)
#define ProcessInvalid ((Handle) - 1)

#define DirEntryDirectory 1

#endif
