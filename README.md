# esp bootloader plus

## Overview

* [中文版](./README_CN.md)

The [esp bootloader plus](https://github.com/espressif/esp-bootloader-plus) is an enhanced bootloader based on [ESP-IDF](https://github.com/espressif/esp-idf) [custom bootloader](https://github.com/espressif/esp-idf/tree/master/examples/custom_bootloader). The firmware update function is supported in the bootloader stage by decompressing the compressed firmware or applying patches to perform patching.  The following table shows the Espressif SoCs that are compatible with `esp bootloader plus` and their corresponding ESP-IDF versions.

| Chip     | ESP-IDF Release/v4.4                                         | ESP-IDF Master                                               |
| -------- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| ESP32-C3 | [![alt text](https://camo.githubusercontent.com/bd5f5f82b920744ff961517942e99a46699fee58737cd9b31bf56e5ca41b781b/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e)](https://camo.githubusercontent.com/bd5f5f82b920744ff961517942e99a46699fee58737cd9b31bf56e5ca41b781b/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e) | [![alt text](https://camo.githubusercontent.com/bd5f5f82b920744ff961517942e99a46699fee58737cd9b31bf56e5ca41b781b/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e)](https://camo.githubusercontent.com/bd5f5f82b920744ff961517942e99a46699fee58737cd9b31bf56e5ca41b781b/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e) |
| ESP32-C2 | *N/A*                                                        | [![alt text](https://camo.githubusercontent.com/bd5f5f82b920744ff961517942e99a46699fee58737cd9b31bf56e5ca41b781b/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e)](https://camo.githubusercontent.com/bd5f5f82b920744ff961517942e99a46699fee58737cd9b31bf56e5ca41b781b/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e) |

By using esp bootloader plus, you can easily use the following three OTA upgrade methods:  

- Full upgrade: OTA server sends the `new_app.bin` to ESP device, and the ESP device applies the `new_app.bin` directly to complete the firmware upgrade.  
- Compression upgrade: OTA server sends the `compressed_new_app.bin` to ESP device. The ESP device decompresses the received `compressed_new_app.bin` in bootloader, and then apply the decompressed firmware to complete the firmware upgrade.  
- Diff compression upgrade (aka incremental upgrade or delta upgrade): calculates the difference between the new firmware and the original firmware, and generates a `patch.bin`. ESP device downloads the `patch.bin` from OTA server, then applies the `patch.bin` and the local original firmware to get the new firmware, completes the firmware upgrade.  

Note: Each of the above upgrade methods is downward compatible. Compression upgrade is compatible with full upgrade; Diff compression upgrade is compatible with compression upgrade and full upgrade.  

## Advantages

The advantages of compression upgrade and diff compression upgrade include:  

- Save flash space. The size of compressed firmware is smaller, therefore it requires less storage space.  
- Save transmission time and improve upgrade success rate. With small firmware size, compression upgrade and differential compression upgrade can shorten the time required to transmit firmware. In this case, it can also improve the upgrade success rate for devices with limited bandwidth or poor network environment.   
- Save OTA power consumption. Our test shows that during an OTA process, the power consumption used to transmit firmware accounts for 80% of the entire power consumption for OTA. Therefore, the smaller firmware size, the less power consumption.  
- Save transmission traffic. The size of compressed firmware or patch file is smaller than the original firmware, so the traffic required for transmission is smaller. If your product is charged by data traffic, then this method can save your maintenance costs.  

## How to Use esp bootloader plus

1. To get esp bootloader plus, you need to navigate to your project directory and clone the repository with `git clone`:  

```
git clone https://github.com/espressif/esp-bootloader-plus.git bootloader_components
```
Example: After cloning the esp bootloader plus in the `hello_world` project, the contents of the `hello_world` directory are as follows:  

```
bootloader_components        — Bootloader directory      
  + esp-xz                   - Esp xz decompress lib
  + main                     - Main source files of the bootloader
main 				         — Main source files of the application 
server_certs                 — The directory used to store certificate
Makefile/CMakeLists.txt    — Makefiles of the application project
```
2. Run the command `idf.py menuconfig `to configure the project.  
3. Run the command `idf.py flash monitor ` to program the target and monitor the output.  

## How to Generate Compressed Firmware and Patch Files

The `custom_ota_gen.py` in the tools directory can be used to generate compressed firmware or patch files.  

The following packages need to be installed before using `custom_ota_gen.py`. You can use the following command to install the required packages:  

- For Linux Users:

```
python -m pip install pycrypto
```

- For Windows Users:

```
pip install pycryptodome
```

**Example 1：**  

```
custom_ota_gen.py -i build/hello-world.bin
```
Run the above command in your project directory will generate the `custom_ota_binaries/hello-world.bin.xz.packed` in the current directory. It is the new compressed firmware after compressing `hello-world.bin`. The suffix `. xz` indicates that it uses `esp-xz` compression algorithm. The suffix `.packed` indicates that it has added a compressed header.  

**Example 2:**  

```
custom_ota_gen.py -i hello-world.bin --sign_key secure_boot_signing_key.pem
```
Run the above command in your project directory will generate the  `custom_ota_binaries/hello-world.bin.xz.packed.signed` in the current directory. It is the compressed firmware and signed after compression. If [Secure Boot](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/secure-boot-v2.html) is used, you can use `--sign_key` option specifies the signature key to sign the compressed firmware, otherwise the ESP device will consider the compressed firmware as illegal data, resulting in upgrade failure.  

Note: To append the image signature to the existing binary, `espsecure.py` needs to be installed. You can refer to [ESP-IDF-Get Started](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html) to install the tool. 

**Example 3:**    

If you need to generate patch files, please switch to the `tools` directory and run the script `install_tools.sh`. Successful execution will prompt:

```
The tools is installed
```

Execute the following command to generate the patch file：

```
custom_ota_gen.py -hv v2 -c xz -d ddelta -i new_hello_world.bin -b build/hello_world.bin
```

Running the above command will generate a `custom_ota_binaries/patch.xz.packed` file in the current directory, which is the patch file obtained by the difference between the new firmware `new_hello_world.bin` and the old firmware `hello_world.bin`.

## How to Test Compressed OTA
You can use  [hello_world](https://github.com/espressif/esp-idf/tree/master/examples/get-started/hello_world) project to test the compression upgrade:  

1）Enter the `hello_world` directory and run command `git clone https://github.com/espressif/esp-bootloader-plus.git bootloader_components` to clone esp bootloader plus.  

2）Create a new partition file `partitions_example.csv` in the `hello_world` directory as the following table:  

```
# Name,   Type, SubType, Offset,   Size, Flags
phy_init, data, phy, 0xf000,        4k
nvs,      data, nvs,       ,        28k
otadata,  data, ota,       ,        8k
ota_0,    app,  ota_0,     ,        1216k,
storage,  data, 0x22,      ,        640k,
```
3）Add the default configuration file `sdkconfig.defaults` in the `hello_world` directory, and copy the following configuration to this file:
```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_example.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions_example.csv"
CONFIG_PARTITION_TABLE_OFFSET=0xb000

CONFIG_BOOTLOADER_DECOMPRESSOR_XZ=y
```

4）Execute command `idf.py set-target esp32c3` in the `hello_world` directory.  

5）Run command `idf.py flash monitor` to compile and flash the new firmware. The log will be as:  

```
I (978) cpu_start: Starting scheduler.
Hello world!
This is esp32c3 chip with 1 CPU core(s), WiFi/BLE, silicon revision 3, 2MB external flash
Minimum free heap size: 329596 bytes
Restarting in 10 seconds...
Restarting in 9 seconds...
```

6）Modify the `printf("Hello world!\n")` in the `app_main()` function of `hello_world` to be `printf("Hello world, Hello esp32!\n")`, and then run command `idf.py build` to build a new firmware.   

7）Run command `bootloader_components/tools/custom_ota_gen.py -i build/hello_world.bin`, it will create a new sub-directory `custom_ota_binaries` and generate the new compressed firmware `hello_world.bin.xz.packed` in it.   

8）Run command `esptool.py -p PORT --after no_reset write_flash 0x150000 custom_ota_binaries/hello_world.bin.xz.packed` to burn the new compressed firmware into the ESP device's `storage` partition. Please note that the parameter `PORT` should be replaced with the actual port of your device.    

9）Run command `idf.py monitor` to check device log, it should be as:   

```
I (285) cpu_start: Starting scheduler.
Hello world, Hello esp32!
This is esp32c3 chip with 1 CPU core(s), WiFi/BLE, silicon revision 3, 2MB external flash
Minimum free heap size: 329596 bytes
Restarting in 10 seconds...
Restarting in 9 seconds...
```

## How to Test Diff OTA
You can use  [hello_world](https://github.com/espressif/esp-idf/tree/master/examples/get-started/hello_world) project to test the compression upgrade:  

1）Enter the `hello_world` directory and run command `git clone https://github.com/espressif/esp-bootloader-plus.git bootloader_components` to clone esp bootloader plus.  

2）Create a new partition file `partitions_example.csv` in the `hello_world` directory as the following table:  

```
# Name,   Type, SubType, Offset,   Size, Flags
phy_init, data, phy, 0xf000,        4k
nvs,      data, nvs,       ,        28k
otadata,  data, ota,       ,        8k
ota_0,    app,  ota_0,     ,        1M,
ota_1,    app,  ota_1,     ,        1M,
storage,  data, 0x22,      ,        640k,
```
3）Add the default configuration file `sdkconfig.defaults` in the `hello_world` directory, and copy the following configuration to this file:
```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_example.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions_example.csv"
CONFIG_PARTITION_TABLE_OFFSET=0xd000

CONFIG_BOOTLOADER_DIFF_ENABLED=y
CONFIG_BOOTLOADER_DIFF_DDELTA=y
CONFIG_BOOTLOADER_DECOMPRESSOR_XZ=y
```

4）Execute command `idf.py set-target esp32c3` in the `hello_world` directory.  

5）Run command `idf.py flash monitor` to compile and flash the new firmware. The log will be as:  

```
I (978) cpu_start: Starting scheduler.
Hello world!
This is esp32c3 chip with 1 CPU core(s), WiFi/BLE, silicon revision 3, 2MB external flash
Minimum free heap size: 329596 bytes
Restarting in 10 seconds...
Restarting in 9 seconds...
```

6）Run the command `cp build/hello_world.bin current_app.bin` to backup the current firmware to the current directory.

7）Modify the `printf("Hello world!\n")` in the `app_main()` function of `hello_world` to be `printf("Hello world, Hello esp32!\n")`, and then run command `idf.py build` to build a new firmware.   

8）Execute the command `bootloader_components/tools/custom_ota_gen.py -hv v2 -c xz -d ddelta -i build/hello_world.bin -b current_app.bin`, the custom_ota_binaries directory will be generated in this directory; the patch file `patch.xz.packed` will be included in the custom_ota_binaries directory.

9）Run command `esptool.py -p PORT --after no_reset write_flash 0x220000 custom_ota_binaries/patch.xz.packed` to burn the patch file into the ESP device's `storage` partition. Please note that the parameter `PORT` should be replaced with the actual port of your device.    

10）Run command `idf.py monitor` to check device log, it should be as:   

```
I (285) cpu_start: Starting scheduler.
Hello world, Hello esp32!
This is esp32c3 chip with 1 CPU core(s), WiFi/BLE, silicon revision 3, 2MB external flash
Minimum free heap size: 329596 bytes
Restarting in 10 seconds...
Restarting in 9 seconds...
```


## Examples

- [examples/decompress/xz_compressed_ota](https://github.com/espressif/esp-iot-solution/tree/master/examples/decompress/xz_compressed_ota)

## Note
1. If your flash partition has only two partitions, one for storing apps and one for storing compressed firmware, then you cannot rollback to the old version if using compression upgrade. In this case, please ensure the availability and correctness of your compressed firmware before upgrading to it.    
1. When using diff OTA, please make a backup of the app firmware burned on the device, because when generating the patch file, the firmware running in the device must be provided to generate the correct patch file.
1. The generation of the new firmware is completed in the bootloader stage. If the device is powered off during the process of generating the new firmware, the device will regenerate the new firmware at the next reboot.


If you have any question in use, please feel free to contact us.

