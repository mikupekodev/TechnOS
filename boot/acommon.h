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