set(srcs "bootloader_start.c"
         "bootloader_custom_ota.c"
         "bootloader_custom_utility.c"
         "bootloader_decompressor_none.c"
         "bootloader_decompressor_common.c"
         "bootloader_diff_none.c"
         "bootloader_storage_flash.c"
         )
set(priv_incs ".")
set(requires "bootloader" "bootloader_support" "spi_flash")

if(CONFIG_BOOTLOADER_DECOMPRESSOR_XZ)
    list(APPEND srcs "bootloader_decompressor_xz.c")
endif()

if(CONFIG_BOOTLOADER_DIFF_DDELTA)
        list(APPEND srcs "bootloader_diff_ddelta.c")
endif()

list(APPEND requires "esp-xz" "bootloader_custom_malloc")

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "../include"
                    PRIV_INCLUDE_DIRS ${priv_incs}
                    REQUIRES ${requires})

idf_build_get_property(target IDF_TARGET)
# Use the linker script files from the actual bootloader
set(scripts "${IDF_PATH}/components/bootloader/subproject/main/ld/${target}/bootloader.ld"
            "${IDF_PATH}/components/bootloader/subproject/main/ld/${target}/bootloader.rom.ld")

target_linker_script(${COMPONENT_LIB} INTERFACE "${scripts}")
