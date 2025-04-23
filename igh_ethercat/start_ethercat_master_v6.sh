#!/bin/bash

if [ $# -eq 0 ]; then
  echo "Usage: $0 <MAC address>"
  exit 1
fi

mac_address="$1"

modprobe phylink
insmod ethercat/master/ec_master.ko main_devices="${mac_address}"
insmod ethercat/devices/ec_generic.ko
modprobe pcs_xpcs

insmod ethercat/devices/dwc/ec_dwc.ko
insmod ethercat/devices/dwc/ec_dwc_plat.ko
insmod ethercat/devices/dwc/ec_dwc_starfive.ko

