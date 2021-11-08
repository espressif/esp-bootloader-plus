set(includes "include/apps")
set(priv_incs "xz-embedded/linux/include/linux" "xz-embedded/linux/lib/xz" "port/include")
set(srcs "apps/esp_xz_decompressor.c"
        "xz-embedded/linux/lib/xz/xz_dec_bcj.c"
        "xz-embedded/linux/lib/xz/xz_dec_lzma2.c"
        "xz-embedded/linux/lib/xz/xz_dec_stream.c")

if(BOOTLOADER_BUILD)
        set(priv_requir bootloader_support)
endif()

idf_component_register(SRCS ${srcs}
                        INCLUDE_DIRS ${includes}
                        PRIV_INCLUDE_DIRS ${priv_incs}
                        PRIV_REQUIRES ${priv_requir}
                        LDFRAGMENTS "${ldfragments}")

idf_build_set_property(COMPILE_DEFINITIONS "-DXZ_INTERNAL_CRC32=1" APPEND)

if(BOOTLOADER_BUILD AND CONFIG_IDF_TARGET_ESP32C3)
        target_linker_script(${COMPONENT_LIB} INTERFACE "linker.lf")
endif()