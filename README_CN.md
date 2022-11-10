# esp bootloader plus

  ## 概述

  * [English Version](./README.md)

  [esp bootloader plus](https://gitee.com/esp-components/esp-bootloader-plus) 是乐鑫基于 [ESP-IDF](https://github.com/espressif/esp-idf) 的 [custom bootloader](https://github.com/espressif/esp-idf/tree/master/examples/custom_bootloader) 推出的增强版 bootloader，支持在 bootloader 阶段对`压缩`或`差分 + 压缩`的固件进行 `解压缩`或`解压缩 + 反差分`，来升级原有固件。  下表总结了适配 `esp bootloader plus` 的乐鑫芯片以及其对应的 ESP-IDF 版本：

| Chip     | ESP-IDF Release/v4.4                                         | ESP-IDF Master                                               |
| -------- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| ESP32-C3 | [![alt text](https://camo.githubusercontent.com/bd5f5f82b920744ff961517942e99a46699fee58737cd9b31bf56e5ca41b781b/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e)](https://camo.githubusercontent.com/bd5f5f82b920744ff961517942e99a46699fee58737cd9b31bf56e5ca41b781b/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e) | [![alt text](https://camo.githubusercontent.com/bd5f5f82b920744ff961517942e99a46699fee58737cd9b31bf56e5ca41b781b/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e)](https://camo.githubusercontent.com/bd5f5f82b920744ff961517942e99a46699fee58737cd9b31bf56e5ca41b781b/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e) |
| ESP32-C2 | *N/A*                                                        | [![alt text](https://camo.githubusercontent.com/bd5f5f82b920744ff961517942e99a46699fee58737cd9b31bf56e5ca41b781b/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e)](https://camo.githubusercontent.com/bd5f5f82b920744ff961517942e99a46699fee58737cd9b31bf56e5ca41b781b/68747470733a2f2f696d672e736869656c64732e696f2f62616467652f2d737570706f727465642d677265656e) |

  使用 esp bootloader plus 可以轻松支持下述三种 OTA 更新方式：  

  - 全量更新：服务器将完整的  new_app.bin 发送到设备端，设备端直接应用  new_app.bin 完成固件更新。  
  - 压缩更新：服务器将压缩后的  new_app.bin，即 compressed_app.bin 发送到设备端，设备端应用解压后得到的  new_app.bin 完成固件更新。  
  - 差分压缩更新：服务器使用设备端正在运行的 origin_app.bin 和 需要更新的 new_app.bin 计算出一个补丁文件 patch.bin，然后将补丁文件发送到设备端；设备端使用补丁，执行反差分，恢复出 new_app.bin，并应用 new_app.bin 完成固件更新。  

  注：每种更新方式向下兼容。压缩更新兼容全量更新；差分压缩更新兼容压缩更新和全量更新。

  ## 优势

  增强版 bootloader 提供的压缩更新、差分压缩更新的优势包括：  

  - 节省硬件 flash 空间。压缩后的固件体积更小，因此需要的存储空间更小。  
  - 节省传输时间，提高更新成功率。部署在带宽受限，网络环境差的设备，可能因设备掉线、传输的固件数据大导致更新成功率低，传输时间长。压缩更新、差分压缩更新缩短传输固件所需的时间，提高更新成功率。  
  - 节省 OTA 功耗。测试表明，一次 OTA 过程中，传输固件数据所消耗的能源超过总的能源消耗的 80% ，因此传输的数据量越小，越节省功耗。  
  - 节省传输流量。压缩后的固件、补丁文件的体积更小，因此传输需要的流量更小。对流量计费的设备将节省维护成本。  

  ## 使用方法

  1. 请至工程目录下使用下述命令 clone 该仓库的内容：  

  ```
  git clone https://gitee.com/esp-components/esp-bootloader-plus bootloader_components
  ```

  示例：以在 `hello_world` 目录运行该命令为例，clone 该仓库内容后，`hello_world` 目录的内容如下所示：  

  ```
  bootloader_components        — Bootloader directory      
    + esp-xz                   - Esp xz decompress lib
    + main                     - Main source files of the bootloader
  main 				         — Main source files of the application 
  server_certs                 — The directory used to store certificate
  Makefile/CMakeLists.txt    — Makefiles of the application project
  ```

  2. 运行 `idf.py menuconfig` 配置工程，然后编译运行。  

  ## 如何生成压缩固件、补丁文件

  您可以使用 tools 目录下的 `custom_ota_gen.py` 来生成压缩的固件或者补丁文件。  

  使用 `custom_ota_gen.py` 前需要安装以下软件包。您可以使用下述命令安装需要的软件包：  

- Linux 环境：

```
python -m pip install pycrypto
```

- Windows 环境：

```
pip install pycryptodome
```

**示例1：**  

  ```
  custom_ota_gen.py -i build/hello_world.bin
  ```

  在用户工程目录运行上述命令将在当前目录生成一个 `custom_ota_binaries/hello_world.bin.xz.packed` 文件，该文件就是压缩 `hello_world.bin` 得到的压缩固件。其中后缀 `.xz`代表其使用的压缩算法是 `esp-xz` 算法；后缀 `.packed`代表其已经添加了压缩头部。  

**示例2：**  

  ```
  custom_ota_gen.py -i hello_world.bin --sign_key secure_boot_signing_key.pem
  ```

  在用户工程目录运行上述命令将在当前目录生成一个 `custom_ota_binaries/hello_world.bin.xz.packed.signed` 文件，该文件是压缩 `hello_world.bin` 得到的压缩固件，并在压缩后做了签名。如果您使用了 [Secure Boot](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/secure-boot-v2.html) 功能，请使用`--sign_key`指定签名的密钥，否则设备将认为该压缩固件是非法数据，导致更新失败。  

  注：使用签名功能，需要安装 `espsecure.py` 工具，您可以参考 [ESP-IDF编程指南-快速入门](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32c3/get-started/index.html) 安装该工具。  

**示例3：**  
  如果您需要生成补丁文件，请先切换到 `tools` 目录下运行脚本 `install_tools.sh`。执行成功将提示:

  ```
  The tools is installed
  ```

  执行命令：

  ```
  custom_ota_gen.py -hv v2 -c xz -d ddelta -i new_hello_world.bin -b build/hello_world.bin
  ```

 在用户工程目录运行上述命令将在当前目录生成一个 `custom_ota_binaries/patch.xz.packed` 文件，该文件是新固件 `new_hello_world.bin` 和旧固件`hello_world.bin`差分得到的补丁文件。

  ## 测试压缩更新

  您可以使用 [ESP-IDF](https://github.com/espressif/esp-idf) 中的 [hello_world](https://github.com/espressif/esp-idf/tree/release/v4.4/examples/get-started/hello_world) 工程来测试 esp bootloader plus 压缩更新的功能：  
  1）切换到 `hello_world` 目录运行命令 `git clone https://gitee.com/esp-components/esp-bootloader-plus bootloader_components` 克隆 esp bootloader plus。  
  2）在 `hello_world` 目录新建分区表文件 partitions_example.csv，文件内容如下：  

  ```
  # Name,   Type, SubType, Offset,   Size, Flags
  phy_init, data, phy, 0xf000,        4k
  nvs,      data, nvs,       ,        28k
  otadata,  data, ota,       ,        8k
  ota_0,    app,  ota_0,     ,        1216k,
  storage,  data, 0x22,      ,        640k,
  ```

  3）在 `hello_world` 目录添加默认配置文件 `sdkconfig.defaults`， 并将下述配置拷贝到该文件：

  ```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_example.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions_example.csv"
CONFIG_PARTITION_TABLE_OFFSET=0xb000

CONFIG_BOOTLOADER_DECOMPRESSOR_XZ=y
  ```
  4）在 `hello_world` 目录执行 `idf.py set-target esp32c3`。  

  5）运行命令 `idf.py flash monitor` 编译并烧录固件，烧录后程序打印 log:  

  ```
  I (978) cpu_start: Starting scheduler.
  Hello world!
  This is esp32c3 chip with 1 CPU core(s), WiFi/BLE, silicon revision 3, 2MB external flash
  Minimum free heap size: 329596 bytes
  Restarting in 10 seconds...
  Restarting in 9 seconds...
  ```

  6）修改 `hello_world` app_main() 函数中的 printf("Hello world!\n") 语句为 printf("Hello world, Hello esp32!\n")，然后运行命令 `idf.py build` 编译生成新固件。  

  7）执行命令 `bootloader_components/tools/custom_ota_gen.py -i build/hello_world.bin` ，将在该目录下生成 custom_ota_binaries 目录；在custom_ota_binaries 目录下将包含压缩后的文件 hello_world.bin.xz.packed。  

  8）执行命令 `esptool.py -p PORT --after no_reset write_flash 0x150000 custom_ota_binaries/hello_world.bin.xz.packed` 将压缩后的新固件写入到设备的 storage 分区（注：PORT 为当前设备的串口号）。  

  9）执行命令 `idf.py monitor` 查看设备更新后的 log，若显示下述 log，则更新成功。  

  ```
  I (285) cpu_start: Starting scheduler.
  Hello world, Hello esp32!
  This is esp32c3 chip with 1 CPU core(s), WiFi/BLE, silicon revision 3, 2MB external flash
  Minimum free heap size: 329596 bytes
  Restarting in 10 seconds...
  Restarting in 9 seconds...
  ```

  ## 测试差分更新

  您可以使用 [ESP-IDF](https://github.com/espressif/esp-idf) 中的 [hello_world](https://github.com/espressif/esp-idf/tree/release/v4.4/examples/get-started/hello_world) 工程来测试 esp bootloader plus 压缩更新的功能：  
  1）切换到 `hello_world` 目录运行命令 `git clone https://gitee.com/esp-components/esp-bootloader-plus bootloader_components` 克隆 esp bootloader plus。  
  2）在 `hello_world` 目录新建分区表文件 partitions_example.csv，文件内容如下：  

  ```
  # Name,   Type, SubType, Offset,   Size, Flags
  phy_init, data, phy, 0xf000,        4k
  nvs,      data, nvs,       ,        28k
  otadata,  data, ota,       ,        8k
  ota_0,    app,  ota_0,     ,        1M,
  ota_1,    app,  ota_1,     ,        1M,
  storage,  data, 0x22,      ,        640k,
  ```

  3）在 `hello_world` 目录添加默认配置文件 `sdkconfig.defaults`， 并将下述配置拷贝到该文件：

```
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_example.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions_example.csv"
CONFIG_PARTITION_TABLE_OFFSET=0xd000

CONFIG_BOOTLOADER_DIFF_ENABLED=y
CONFIG_BOOTLOADER_DIFF_DDELTA=y
CONFIG_BOOTLOADER_DECOMPRESSOR_XZ=y
```

  4）在 `hello_world` 目录执行 `idf.py set-target esp32c3`。  

  5）运行命令 `idf.py flash monitor` 编译并烧录固件，烧录后程序打印 log:  

  ```
  I (978) cpu_start: Starting scheduler.
  Hello world!
  This is esp32c3 chip with 1 CPU core(s), WiFi/BLE, silicon revision 3, 2MB external flash
  Minimum free heap size: 329596 bytes
  Restarting in 10 seconds...
  Restarting in 9 seconds...
  ```

  6）运行命令 `cp build/hello_world.bin current_app.bin` 备份当前固件到当前目录。

  7）修改 `hello_world` app_main() 函数中的 printf("Hello world!\n") 语句为 printf("Hello world, Hello esp32!\n")，然后运行命令 `idf.py build` 编译生成新固件。  

  8）执行命令 `bootloader_components/tools/custom_ota_gen.py -hv v2 -c xz -d ddelta -i build/hello_world.bin -b current_app.bin` ，将在该目录下生成 custom_ota_binaries 目录；在custom_ota_binaries 目录下将包含补丁文件 patch.xz.packed。    

  9）执行命令 `esptool.py -p PORT --after no_reset write_flash 0x220000 custom_ota_binaries/patch.xz.packed` 将补丁文件写入到设备的 storage 分区（注：PORT 为当前设备的串口号）。  

  10）执行命令 `idf.py monitor` 查看设备更新后的 log，若显示下述 log，则更新成功。  

  ```
  I (285) cpu_start: Starting scheduler.
  Hello world, Hello esp32!
  This is esp32c3 chip with 1 CPU core(s), WiFi/BLE, silicon revision 3, 2MB external flash
  Minimum free heap size: 329596 bytes
  Restarting in 10 seconds...
  Restarting in 9 seconds...
  ```

  ## 示例

  - [examples/decompress/xz_compressed_ota](https://github.com/espressif/esp-iot-solution/tree/master/examples/decompress/xz_compressed_ota)

  ## 使用注意

1. 当使用压缩更新时，双分区情况下，不支持版本回滚。  
当您的 flash 分区仅有一个存储 app 的分区，和一个存储压缩固件的分区时，压缩更新将无法提供版本回滚的功能，请在下发压缩固件前验证压缩固件的可用性和正确性。  
2. 当使用差分更新时，请备份烧录到设备上的 app 固件，因为制作补丁文件时，必须提供设备中正在运行的固件，才能制作正确的补丁文件。
3. 新固件的生成是在 bootloader 阶段完成的，若生成新固件的过程中设备掉电，设备将在下次重启时重新生成新固件。


如在使用中有任何问题，请联系我们。  
