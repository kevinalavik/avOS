#ifndef MEM_PAGING_H
#define MEM_PAGING_H

#include <Boot/BootInfo.h>
#include <stdint.h>

uint32_t PagingBuildKernelMap(const BootFramebuffer *Framebuffer);
uint64_t PagingGetHhdmOffset(void);

#endif
