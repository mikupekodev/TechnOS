#include "noUEFIcommon.h"

// ===========
// BOOSTER
// ===========
#define PML4_IDX(num) (((num) >> 39) & 0x1FF)
#define PDPT_IDX(num) (((num) >> 30) & 0x1FF)
#define PD_IDX(num) (((num) >> 21) & 0x1FF)
#define PT_IDX(num) (((num) >> 12) & 0x1F)

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE (1ULL << 1)
#define PAGE_USER (1ULL << 2)
#define PAGE_NX (1ULL << 63)

void MapPage() {}

void main_booster()
{
}