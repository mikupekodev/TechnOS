
#include "noUEFIcommon.h"

extern void B_InitSerial(void);
extern void LOGOUT(UINT8 *text);
extern void L_HLT(void);

typedef struct
{
    UINT32 Type;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;

} EFI_MEMORY_DESCRIPTOR;

typedef struct
{
    UINT32 width;
    UINT32 height;
    UINT32 pps;
    UINT8 bpp;
    UINT8 fomat;
    UINT32 red_mask;
    UINT32 green_mask;
    UINT32 blue_mask;
    UINT32 reserved_mask;
    EFI_PHYSICAL_ADDRESS fb_base;
    EFI_PHYSICAL_ADDRESS fb_size;

} gop;

typedef struct
{
    UINTN file_size;
    EFI_PHYSICAL_ADDRESS file_addr;
} file_info;

typedef struct
{
    UINTN file_size;
    EFI_PHYSICAL_ADDRESS file_addr;
    UINTN load_size;
    EFI_PHYSICAL_ADDRESS load_addr;
    EFI_PHYSICAL_ADDRESS entry;

} file_elf_info;

typedef struct
{
    EFI_MEMORY_DESCRIPTOR *map;
    UINTN map_size;
    UINTN desc_size;
    UINT32 desc_version;
    UINTN map_key;
    UINTN entry_count;
    UINTN pages;
} mem_map;

typedef struct
{
    UINT8 debugmode;
    EFI_PHYSICAL_ADDRESS acpi;
    gop gop;
    mem_map mem;
    file_info bmp_image;
    file_elf_info kernel;
    file_elf_info booster;
    EFI_PHYSICAL_ADDRESS file_buffer;
} info;

void L_PrintB(CHAR8 title,CHAR16 text,UINT64 num,UINT8 FLAGS){
/* FLAGS
1 改行
2 type
  0 [title] text num
  1 text num title 
3 num enable (0=true)
4 debug massage
5 
6 
7 
8 
*/
}

UINT32 L_ChangeColor(UINT32 rgba, gop *gop)
{
    UINT8 r = (rgba >> 24) & 0xFF;
    UINT8 g = (rgba >> 16) & 0xFF;
    UINT8 b = (rgba >> 8) & 0xFF;
    UINT8 a = rgba & 0xFF;

    if (gop->fomat == PixelRedGreenBlueReserved8BitPerColor)
    {
        return ((UINT32)b << 24) |
               ((UINT32)g << 16) |
               ((UINT32)r << 8) |
               a;
    }
    else if (gop->fomat == PixelBlueGreenRedReserved8BitPerColor)
    {
        return ((UINT32)r << 24) |
               ((UINT32)g << 16) |
               ((UINT32)b << 8) |
               a;
    }
    else
    {
        return 0;
    }

    return 0;
}

void L_PrintBMP(file_info *bmp_image, gop gop, UINTN x, UINTN y,UINTN mask_start_x, UINTN mask_start_y,UINTN mask_end_x, UINTN mask_end_y)
{
    if (bmp_image == NULL ||
        bmp_image->file_addr == 0 ||
        bmp_image->file_size < 54)
    {
        L_PrintB("ERROR",L"Invalid BMP image",0,1);

        return;
    }

    UINT8 *bmp = (UINT8 *)(UINTN)bmp_image->file_addr;

    UINT32 bmp_width = *(UINT32 *)(bmp + 18);
    UINT32 bmp_height = *(UINT32 *)(bmp + 22);
    UINT16 bpp = *(UINT16 *)(bmp + 28);
    UINT32 offset = *(UINT32 *)(bmp + 10);

    if (bpp != 24 && bpp != 32)
    {
        L_PrintB("ERROR",L"Unsupported BMP format",0,1);
        return;
    }

    UINT32 bytes_per_pixel = bpp / 8;

    UINT32 stride =
        (bmp_width * bytes_per_pixel + 3) & ~3;

    if (mask_end_x == 0)
        mask_end_x = bmp_width;

    if (mask_end_y == 0)
        mask_end_y = bmp_height;

    UINT8 *fb = (UINT8 *)(UINTN)gop.fb_base;

    for (UINT32 py = mask_start_y; py < mask_end_y; py++)
    {
        for (UINT32 px = mask_start_x; px < mask_end_x; px++)
        {
            if (px >= bmp_width || py >= bmp_height)
                continue;

            UINT32 index =
                offset +
                (bmp_height - 1 - py) * stride +
                px * bytes_per_pixel;

            UINT8 b = bmp[index + 0];
            UINT8 g = bmp[index + 1];
            UINT8 r = bmp[index + 2];
            UINT8 a = 0xFF;

            if (bpp == 32)
                a = bmp[index + 3];

            UINT32 rgba =
                ((UINT32)r << 24) |
                ((UINT32)g << 16) |
                ((UINT32)b << 8) |
                a;

            UINT32 pixel = L_ChangeColor(rgba, &gop);

            UINTN screen_x =
                x + (px - mask_start_x);

            UINTN screen_y =
                y + (py - mask_start_y);

            if (screen_x < gop.width &&
                screen_y < gop.height)
            {
                UINT32 fb_index =
                    (screen_y * gop.pps + screen_x) * 4;

                *(UINT32 *)(fb + fb_index) = pixel;
            }
        }
    }
}
