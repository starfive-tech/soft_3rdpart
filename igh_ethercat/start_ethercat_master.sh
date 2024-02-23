#!/bin/bash

if [ $# -eq 0 ]; then
  echo "Usage: $0 <MAC address>"
  exit 1
fi

mac_address="$1"

modprobe phylink
insmod /lib/modules/$(uname -r)/ethercat/master/ec_master.ko main_devices="${mac_address}"
insmod /lib/modules/$(uname -r)/ethercat/devices/ec_generic.ko
modprobe pcs_xpcs

cd /lib/modules/$(uname -r)/kernel/drivers/net/ethernet/stmicro/stmmac/
insmod stmmac.ko
insmod stmmac-platform.ko
insmod dwmac-starfive-plat.ko

cd /root
