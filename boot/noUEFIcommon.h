#include <stdint.h>
#include <stddef.h>
#define UINT8 uint8_t
#define UINT16 uint16_t
#define UINT32 uint32_t
#define UINT64 uint64_t
#define UINTN uintptr_t
#define CHAR8 int8_t
#define CHAR16 int16_t
#define STATUS UINT8 //仮置き
#define EFI_PHYSICAL_ADDRESS uintptr_t
#define  EFI_VIRTUAL_ADDRESS uintptr_t

#define PixelRedGreenBlueReserved8BitPerColor  0
#define PixelBlueGreenRedReserved8BitPerColor  1

#define EFI_LOAD_ERROR  0x8000000000000001ULL

#define LOG_SUCCESS 0
#define LOG_ERROR 1
#define LOG_WARNING 2
#define LOG_NONE 3