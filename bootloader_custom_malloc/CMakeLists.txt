set(srcs "bootloader_custom_malloc.c")

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "include"
                    REQUIRES ${requires})