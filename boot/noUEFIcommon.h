#include <stdint.h>
#include <stddef.h>
#define UINT8 uint8_t
#define UINT16 uint16_t
#define UINT32 uint32_t
#define UINT64 uint64_t
#define UINTN uintptr_t
#define CHAR8 int8_t
#define CHAR16 int16_t
#define STATUS UINT64 //仮置き
#define EFI_PHYSICAL_ADDRESS uintptr_t
#define  EFI_VIRTUAL_ADDRESS uintptr_t

#define PixelRedGreenBlueReserved8BitPerColor  0
#define PixelBlueGreenRedReserved8BitPerColor  1

#define EFI_LOAD_ERROR  0x8000000000000001ULL

#define LOG_SUCCESS 0
#define LOG_ERROR 1
#define LOG_WARNING 2
#define LOG_NONE 3
void L_PrintB(CHAR8 title,CHAR16 text,UINT64 num,UINT8 FLAGS);
UINT32 L_ChangeColor(UINT32 rgba, gop *gop);
void L_PrintBMP(file_info *bmp_image, gop gop, UINTN x, UINTN y,UINTN mask_start_x, UINTN mask_start_y,UINTN mask_end_x, UINTN mask_end_y);
