//ファイル名の先頭にaをつけることで一番上に表示させる
//common.cとリンクさせる
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>
#include <IndustryStandard/Acpi.h>
#include <Library/BaseLib.h>
#include <Guid/Acpi.h>
#include <Guid/FileInfo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleTextIn.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <stdint.h>
#include "elf.h"

#define PT_LOAD 1
#define KERNEL_PATH L"\\EFI\\technOS\\kernel\\kernel.elf"
#define LOGO_PATH L"\\EFI\\BOOT\\logo.bmp"
#define BACK_PATH L"\\EFI\\BOOT\\back.bmp"
#define MENU_PATH L"\\EFI\\BOOT\\menu.bmp"
#define KEY_PATH L"\\EFI\\BOOT\\key.bin"

#define LOG_SUCCESS 0
#define LOG_ERROR 1
#define LOG_WARNING 2
#define LOG_NONE 3

#define PAGE_SIZE 4096
#define CHECK 0x1000000000

typedef unsigned long long UINT64;





EFI_FILE_PROTOCOL *root_dir;
UINT8 debugmode = 0;

/*
https://www.docswell.com/s/uchan_nos/Z1J3W4-run-elf-binary-using-uefi#p5
https://www.docswell.com/s/uchan_nos/Z4Q4MQ-find-bugs-automatically-using-dbi#p7
https://qiita.com/ImagingSolAkira/items/30fd3727afa3076b8050
*/

/*
1.GET gop,memory,ACPI
2.load kernel(random)
3.load logo
4.handout
5.exit boot service

~booster~

6.virtual address set
7.kernel setup（handout->virtual kernel->virtual)
8.jump kernel
*/

//==================================
// 構造体定義
//==================================
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
//==================================
// booster/loader lib
// =================================

//==================================
// function
//==================================

EFI_STATUS B_GetFile(file_info *file_info, CHAR16 *path)
{
    /*
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL *Root;*/
    EFI_FILE_PROTOCOL *File;
    EFI_STATUS st;
    // opem

    st = root_dir->Open(root_dir, &File, path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(st))
    {
        L_PrintB("error",L"can’t open the file | code:",st,1);
        return EFI_LOAD_ERROR;
    }

    // read
    UINTN InfoSize = 0;

    st = File->GetInfo(
        File,
        &gEfiFileInfoGuid,
        &InfoSize,
        NULL);

    if (st != EFI_BUFFER_TOO_SMALL)
    {
        File->Close(File);
        return st;
    }

    EFI_FILE_INFO *FileInfo = AllocatePool(InfoSize);

    if (FileInfo == NULL)
    {
        File->Close(File);
        return EFI_OUT_OF_RESOURCES;
    }

    st = File->GetInfo(
        File,
        &gEfiFileInfoGuid,
        &InfoSize,
        FileInfo);

    if (EFI_ERROR(st))
    {
        FreePool(FileInfo);
        File->Close(File);
        return st;
    }

    UINTN FileSize = FileInfo->FileSize;
    FreePool(FileInfo);
    EFI_PHYSICAL_ADDRESS file_buf_addr;
    UINTN pages = (FileSize + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;

    st = gBS->AllocatePages(
        AllocateAnyPages,
        EfiLoaderData,
        pages,
        &file_buf_addr);

    if (EFI_ERROR(st))
    {
        File->Close(File);
        L_PrintB("error",L"Failed to allocate pages for file | code:",st,1);
        return st;
    }
    if (FileSize == 0)
    {
        File->Close(File);
        L_PrintB("error",L"File is empty | code:",st,1);
        return EFI_LOAD_ERROR;
    }

    VOID *FileBuffer = (VOID *)(UINTN)file_buf_addr;
    if (FileBuffer == NULL)
    {
        File->Close(File);
        L_PrintB("error",L"Failed to allocate memory for file buffer | code:",st,1);
        return EFI_OUT_OF_RESOURCES;
    }
    st = File->SetPosition(File, 0);
    if (EFI_ERROR(st))
    {
        L_PrintB("error",L"Failed to set file position | code:",st,1);
        gBS->FreePages((EFI_PHYSICAL_ADDRESS)(UINTN)FileBuffer, pages);
        File->Close(File);
        return st;
    }
    st = File->Read(File, &FileSize, FileBuffer);
    if (EFI_ERROR(st))
    {
        L_PrintB("error",L"can't read the file | code:",st,1);
        gBS->FreePages((EFI_PHYSICAL_ADDRESS)(UINTN)FileBuffer, pages);
        return EFI_LOAD_ERROR;
    }
    File->Close(File);

    file_info->file_addr = (EFI_PHYSICAL_ADDRESS)(UINTN)FileBuffer;
    file_info->file_size = FileSize;
    return EFI_SUCCESS;
}

EFI_STATUS B_Sha256(EFI_PHYSICAL_ADDRESS data, UINTN size, UINT8 hash[32])
{
    L_HLT();
    return EFI_SUCCESS;
}

EFI_STATUS B_GetACPI(EFI_PHYSICAL_ADDRESS *acpi_addr)
{
    EFI_CONFIGURATION_TABLE *config_table = gST->ConfigurationTable;
    UINTN num_tables = gST->NumberOfTableEntries;

    for (UINTN i = 0; i < num_tables; i++)
    {
        if (CompareGuid(&config_table[i].VendorGuid, &gEfiAcpi20TableGuid))
        {
            *acpi_addr = (EFI_PHYSICAL_ADDRESS)(UINTN)config_table[i].VendorTable;
            L_PrintB("loader",L"ACPI 2.0 table found at address: 0x",acpi_addr,1);
            return EFI_SUCCESS;
        }
    }

    for (UINTN i = 0; i < num_tables; i++)
    {
        if (CompareGuid(&config_table[i].VendorGuid, &gEfiAcpi10TableGuid))
        {
            *acpi_addr = (EFI_PHYSICAL_ADDRESS)(UINTN)config_table[i].VendorTable;
            L_PrintB("loader",L"ACPI table found at address: 0x",acpi_addr,1);
            return EFI_SUCCESS;
        }
    }
    L_PrintB("error",L"ACPI table not found | code:",EFI_NOT_FOUND,1);
    return EFI_NOT_FOUND;
}

EFI_STATUS B_GetGop(gop *scr)

{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_STATUS st;

    st = gBS->LocateProtocol(
        &gEfiGraphicsOutputProtocolGuid,
        NULL,
        (VOID **)&gop);
    if (EFI_ERROR(st))
    {
        L_PrintB("error",L"couldn’t allocate memory | code:",st,1);
        return st;
    }
    // debug
    for (UINT32 i = 0; i < gop->Mode->MaxMode; i++)
    {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
        UINTN size;

        st = gop->QueryMode(gop, i, &size, &info);
        if (EFI_ERROR(st))
            continue;

        if (info->HorizontalResolution == 1920 &&
            info->VerticalResolution == 1080)
        {

            gop->SetMode(gop, i);
            gBS->FreePool(info);
            break;
        }

        gBS->FreePool(info);
    }
    // debug
    scr->width = gop->Mode->Info->HorizontalResolution;
    scr->height = gop->Mode->Info->VerticalResolution;
    scr->pps = gop->Mode->Info->PixelsPerScanLine;
    scr->fb_base = gop->Mode->FrameBufferBase;
    scr->fomat = gop->Mode->Info->PixelFormat;
    scr->red_mask = gop->Mode->Info->PixelInformation.RedMask;
    scr->green_mask = gop->Mode->Info->PixelInformation.GreenMask;
    scr->blue_mask = gop->Mode->Info->PixelInformation.BlueMask;
    scr->reserved_mask = gop->Mode->Info->PixelInformation.ReservedMask;
    return EFI_SUCCESS;
}

EFI_STATUS B_GetMmap(mem_map *m)
{
    EFI_STATUS st;
    m->map_size = 0;
    m->map = NULL;
    m->desc_size = 0;
    m->map_key = 0;
    m->desc_version = 0;

    st = gBS->GetMemoryMap(
        &m->map_size,
        m->map,
        &m->map_key,
        &m->desc_size,
        &m->desc_version);

    if (st != EFI_BUFFER_TOO_SMALL)
    {
        L_PrintB("error",L"st != EFI_BUFFER_TOO_SMALL | code:",st,1);
        return st;
    }

    m->map_size += m->desc_size * 16;

    UINTN pages = (m->map_size + PAGE_SIZE - 1) / PAGE_SIZE;
    EFI_PHYSICAL_ADDRESS map_addr;

    st = gBS->AllocatePages(
        AllocateAnyPages,
        EfiLoaderData,
        pages,
        &map_addr);

    if (EFI_ERROR(st))
    {
        L_PrintB("error",L"couldn't allocate memory. | code:",st,1);
        return st;
    }

    m->map = (EFI_MEMORY_DESCRIPTOR *)(UINTN)map_addr;
    m->pages = pages;

    st = gBS->GetMemoryMap(
        &m->map_size,
        m->map,
        &m->map_key,
        &m->desc_size,
        &m->desc_version);

    if (EFI_ERROR(st))
    {
        L_PrintB("error",L"GetMemoryMap error | code:",st,1);
        return st;
    }

    m->entry_count = m->map_size / m->desc_size;
    return EFI_SUCCESS;
}

EFI_STATUS B_GetELF(file_elf_info *elf_file_info, CHAR16 *path)
{
    EFI_STATUS st;
    file_info get_file_info;
    st = B_GetFile(&get_file_info, path);
    if (EFI_ERROR(st))
    {
        L_PrintB("error",L"Failed to load elf file | code:",st,1);

        return st;
    }

    // is ELF?
    Elf64_Ehdr *Ehdr = (Elf64_Ehdr *)elf_file_info->file_addr;
    Elf64_Phdr *Phdr = (Elf64_Phdr *)((UINT8 *)Ehdr + Ehdr->e_phoff);

    if (Ehdr->e_ident[0] != 0x7F ||
        Ehdr->e_ident[1] != 'E' ||
        Ehdr->e_ident[2] != 'L' ||
        Ehdr->e_ident[3] != 'F')
    {
        L_PrintB("error",L"This ain’t ELF | code:",0x55AA,1);
        //bios
        return EFI_LOAD_ERROR;
    }
    // is 64bit?
    if (Ehdr->e_ident[EI_CLASS] != ELFCLASS64)
    {
        L_PrintB("error",L"This ain’t 64-bit | code:",0x64,1);
        //64bit
        return EFI_LOAD_ERROR;
    }

    // なぜEFI_HASH2_PROTOCOLを使わないかって? UEFIが改変されてても影響を受けないからさ!!
    // UEFIがウイルスに乗っ取られてるとEFI_HASH2_PROTOCOLは定数を出力するだけで突破できる
    // エディターで短くする用
    do
    {
        UINT8 Hash[32];
        B_Sha256(elf_file_info->file_addr, elf_file_info->file_size, Hash);

    } while (0);

    // get start
    UINT64 MinAddr = UINT64_MAX;
    UINT64 MaxAddr = 0;

    for (int i = 0; i < Ehdr->e_phnum; i++)
    {
        if (Phdr[i].p_type != PT_LOAD)
            continue;
        if (Phdr[i].p_memsz == 0)
            continue;
        MinAddr = MIN(MinAddr, Phdr[i].p_vaddr);
        MaxAddr = MAX(MaxAddr, Phdr[i].p_vaddr + Phdr[i].p_memsz);
    }

    elf_file_info->file_addr = get_file_info.file_addr;
    elf_file_info->file_size = get_file_info.file_size;
    elf_file_info->load_addr = MinAddr;
    elf_file_info->load_size = MaxAddr - MinAddr;
    elf_file_info->entry = Ehdr->e_entry;
    return EFI_SUCCESS;
}

EFI_STATUS B_MakeHandout(info *boot, gop *gop, mem_map *map, file_info *bmp, EFI_PHYSICAL_ADDRESS ACPIaddr)
{
    boot->gop = *gop;
    boot->mem = *map;
    boot->acpi = ACPIaddr;
    boot->bmp_image = *bmp;

    return EFI_SUCCESS;
}

EFI_STATUS B_ExitBootServices(info *boot_info, EFI_HANDLE ImageHandle)
{
    EFI_STATUS st;
    UINTN buf_size = boot_info->mem.pages * PAGE_SIZE;

    while (1)
    {
        UINTN map_size = buf_size;

        // GetMemoryMap → 即ExitBootServices
        // この間に AllocatePages/FreePages を一切呼ばない
        st = gBS->GetMemoryMap(
            &map_size,
            boot_info->mem.map,
            &boot_info->mem.map_key,
            &boot_info->mem.desc_size,
            &boot_info->mem.desc_version);

        if (EFI_ERROR(st))
            continue;

        st = gBS->ExitBootServices(ImageHandle, boot_info->mem.map_key);
        if (!EFI_ERROR(st))
        {
            boot_info->mem.map_size = map_size;
            boot_info->mem.entry_count =
                map_size / boot_info->mem.desc_size;
            break;
        }
    }
    return EFI_SUCCESS;
}

void B_JmpBooster(info *boot_info)
{
    asm volatile("cli");
    void (*Entry)(info *) = (void (*)(info *))boot_info->booster.entry;
    Entry(boot_info);
    while (1)
        ;
}

//==================================
// GUI
//==================================
void menu_main(file_info *bmp, gop *gop)
{
    B_GetFile(bmp, MENU_PATH);
    L_PrintBMP(bmp, *gop, 0, 0, 0, 0, 0, 0);
    L_PrintB("loader",L"Menu displayed. Press any key to continue...",0,5);
L_HLT();
}

//==================================
// main
//==================================
EFI_STATUS EFIAPI UefiMain(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_STATUS st;

    st = gBS->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID **)&loaded_image);

    st = gBS->HandleProtocol(
        loaded_image->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID **)&fs);

    fs->OpenVolume(fs, &root_dir);

    gop gop;
    mem_map mem;
    info boot_info;
    file_info bmp;

    EFI_INPUT_KEY Key;
    EFI_PHYSICAL_ADDRESS ACPIaddr;
    boot_info.debugmode = 0;

    // step0
    B_InitSerial();

    st = B_GetGop(&gop);
    if (EFI_ERROR(st))
    {
        L_PrintB("error",L"Failed to get GOP | code:",st,1);
        L_HLT();
    }
    for (int i = 0; i < 30; i++)
    {
        if (gST->ConIn->ReadKeyStroke(gST->ConIn, &Key) == EFI_SUCCESS)
        {

            if (Key.ScanCode == SCAN_NULL && Key.UnicodeChar == L'm')
            {
                menu_main(&bmp, &gop);
            }
            else if (Key.ScanCode == SCAN_NULL && Key.UnicodeChar == L'd')
            {
                boot_info.debugmode = 1;
                debugmode = 1;
            }
            else
            {
                Print(L".");
            }
        }
        else
        {
            Print(L".");
        }

        gBS->Stall(100000);
    }
    /*
 ______                       _                    _
(____  \                _    | |                  | |
 ____)  )  ___    ___  | |_  | |  ___    ____   _ | |  ____   ____
|  __  (  / _ \  / _ \ |  _) | | / _ \  / _  | / || | / _  ) / ___)
| |__)  )| |_| || |_| || |__ | || |_| |( ( | |( (_| |( (/ / | |
|______/  \___/  \___/  \___)|_| \___/  \_||_| \____| \____)|_|


*/
    L_PrintB("loader",L"HelloWorld",0,5);
    L_PrintB("loader",L"",0,5);

/*
    Print(L" ______                       _                    _               \n");
    Print(L"|  __ \\                _    | |                  | |              \n");
    Print(L"| |__)  )  ___    ___  | |_  | |  ___    ____   _ | |  ____   ____ \n");
    Print(L"|  __  (  _ \\  / _ \\ |  _) | | / _ \\  / _  | / || | / _  ) / ___)\n");
    Print(L"| |__)  )| |_| || |_| || |__ | || |_| |( ( | |( (_| |( (/ / | |    \n");
    Print(L"|______/  \\___/  \\___/  \\___)|_| \\___/  \\_||_| \\____| \\____)|_|    \n");
*/
    L_PrintB("loader",L" ______                       _                    _               ",0,5);
    L_PrintB("loader",L"|  __ \\                _    | |                  | |              ",0,5);
    L_PrintB("loader",L"| |__)  )  ___    ___  | |_  | |  ___    ____   _ | |  ____   ____ ",0,5);
    L_PrintB("loader",L"|  __  (  _ \\  / _ \\ |  _) | | / _ \\  / _  | / || | / _  ) / ___)",0,5);
    L_PrintB("loader",L"| |__)  )| |_| || |_| || |__ | || |_| |( ( | |( (_| |( (/ / | |    ",0,5);
    L_PrintB("loader",L"|______/  \\___/  \\___/  \\___)|_| \\___/  \\_||_| \\____| \\____)|_|    ",0,5);




    L_PrintB("loader",L"technOS bootloader v2.0",0,5);

    if (boot_info.debugmode)
    {
        L_PrintB("loader",L"Debug mode is enabled",0,5);
    }
    gBS->Stall(3000000);
    // step1
    st = B_GetMmap(&mem);
    if (EFI_ERROR(st))
    {
            L_PrintB("error",L"Failed to get memory map | code:",st,1);
        L_HLT();
    }
    st = B_GetFile(&bmp, LOGO_PATH);
    if (EFI_ERROR(st))
    {
        L_PrintB("error",L"Failed to load logo image | code:",st,1);
        L_HLT();

    }

    st = B_GetACPI(&ACPIaddr);
    if (EFI_ERROR(st))
    {
        L_PrintB("error",L"Failed to get ACPI table | code:",st,1);
        L_HLT();

    }

    // step2
    st = B_GetELF(&boot_info.kernel, KERNEL_PATH);
    if (EFI_ERROR(st))
    {
        L_PrintB("error",L"Failed to load kernel ELF file | code:",st,1);
        L_HLT();

    }

    st = B_MakeHandout(&boot_info, &gop, &mem, &bmp, ACPIaddr);
    if (EFI_ERROR(st))
    {
        L_PrintB("error",L"Failed to create handout | code:",st,1);
        L_HLT();

    }

    L_PrintB("loader",L"Boot process completed successfully",0,1);
    L_PrintB("loader",L"kernel_entry       : 0x",boot_info.kernel.entry,1);
    L_PrintB("loader",L"kernel_file_start  : 0x",boot_info.kernel.file_addr,1);
    L_PrintB("loader",L"kernel_file_size   : 0x",boot_info.kernel.file_size,1);

    L_PrintB("loader",L"booster_entry      : 0x",boot_info.booster.entry,1);
    L_PrintB("loader",L"booster_file_start : 0x",boot_info.booster.file_addr,1);
    L_PrintB("loader",L"booster_file_size  : 0x",boot_info.booster.file_size,1);

    L_PrintB("loader",L"ACPI addr          : 0x",boot_info.acpi,1);

    L_PrintB("loader",L"framebuffer addr   : 0x",boot_info.gop.fb_base,1);
    L_PrintB("loader",L"framebuffer size   : 0x",boot_info.gop.fb_size,1);
    L_PrintB("loader",L"framebuffer format : ",boot_info.gop.fomat,1);
    L_PrintB("loader",L"framebuffer width  : ",boot_info.gop.width,1);
    L_PrintB("loader",L"framebuffer height : ", boot_info.gop.height,1);
    L_PrintB("loader",L"framebuffer pps    : ",boot_info.gop.pps,1);


    // step3
    B_ExitBootServices(&boot_info, ImageHandle);
    B_JmpBooster(&boot_info);
    return EFI_SUCCESS;
}
