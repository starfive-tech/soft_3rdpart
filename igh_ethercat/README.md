# IgH EtherCAT主站及应用的编译与使用

基于 **rt-ethercat-release** 分支的 linux 内核。

## 主目录结构说明

igh_ethercat.sh igh_ethercat 主站安装、应用编译脚本，安装编译完成后会重新执行一次make打包镜像

application     应用目录，包含伺服电机的 Profile velocity 模式应用源文件以及 Makefile，该应用会随着主站安装脚本执行同步编译并安装

README.md       本文件

### 编译脚本

#### 使用方式

在 SDK 的 soft_3rdpart/igh_ethercat 路径下执行安装脚本：

```bash
./igh_ethercat.sh
```

#### 说明

该脚本会检测SDK所使用的 linux 内核版本是否为 rt-ethercat-release 且是否已经编译过，若确认在该分支上且编译过，则会拉取指定版本的 IgH EtherCAT 主站代码，进行主站以及Demo程序的编译。

编译完成后，主站文件输出到 work/buildroot_initramfs_sysroot 中的对应路径下，主站模块编译并根据 work/linux/include/config/kernel.release 文件中的 kernel release 版本安装到 work/module_install_path 中的对应路径下。

IgH EtherCAT主站启动脚本以及Demo程序会在主站编译完成后编译并安装到 work/buildroot_initramfs_sysroot/root 路径，即打包后的镜像的 /root 路径下。

### 启动脚本

EtherCAT 主站的使用需要绑定对应网口的 MAC 地址号，此 MAC 地址需要在 U-boot 命令行模式下通过命令获取：

```bash
printenv eth1addr
```

此处选择 VisionFive2 上的第二个网口作为 EtherCAT 通信，所以打印了 `eth1addr`。

例如：
```bash
StarFive # printenv eth1addr
eth1addr=6c:cf:39:00:38:2e
```

开机后，使用启动脚本：start_ethercat_master.sh 启动EtherCAT主站与网口：

```bash
# ./start_ethercat_master.sh 6c:cf:39:00:38:2e
```

### 应用

运行启动脚本启动EtherCAT主站与网口后，可运行 EtherCAT demo 应用：

```bash
# ./ectest_PV
```
