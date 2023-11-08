#!/bin/bash

current_path=$(pwd)
work_path=${current_path}/../../work
buildroot_initramfs_sysroot_path=${work_path}/buildroot_initramfs_sysroot
linux_path=${work_path}/linux
kernel_release_file=${linux_path}/include/config/kernel.release
install_mod_path=${work_path}/module_install_path
toolchains_path=${work_path}/buildroot_initramfs/host/bin

cd ../../linux
linux_branch=$(git rev-parse --abbrev-ref HEAD)

if [ "$linux_branch" == "rt-ethercat-release" ]; then
    echo "Linux source code is on the branch: 'rt-ethercat-release'."
    git pull
    cd ../
    make clean
    make
else
    echo "The Linux source code is not on the 'rt-ethercat-release' branch. Exiting."
    cd ${current_path}
    exit 1
fi

cd ${current_path}

if [ -d "${buildroot_initramfs_sysroot_path}" ] && [ -d "${linux_path}" ] && [ -d "${install_mod_path}" ]; then
  echo "Both directories(${buildroot_initramfs_sysroot_path} and ${linux_path}) exist. Proceeding with the script..."
else
  echo "One or both of the directories(${buildroot_initramfs_sysroot_path} and ${linux_path}) do not exist, the SDK may not have been fully compiled. Please check."
  exit 1 
fi

repo_url="https://gitlab.com/etherlab.org/ethercat.git"

# Don't using stable-1.5, witch will cause:
# Making install in tool
# make[1]: 进入目录“/home/atlas/visionfive/soft_3rdpart/ethercat/tool”
#   CXXLD    ethercat
# /home/atlas/visionfive/work/buildroot_initramfs/host/lib/gcc/riscv64-buildroot-linux-gnu/12.2.0/../../../../riscv64-buildroot-linux-gnu/bin/ld: ../master/soe_errors.o: can't link soft-float modules with double-float modules
# /home/atlas/visionfive/work/buildroot_initramfs/host/lib/gcc/riscv64-buildroot-linux-gnu/12.2.0/../../../../riscv64-buildroot-linux-gnu/bin/ld: failed to merge target specific data of file ../master/soe_errors.o
# collect2: error: ld returned 1 exit status
# branch_name="stable-1.5"
branch_name="master"
commit_id="775b93de5bab9c572d3e71a9c50b90f25c3edb0e"

check_commit_id() {
  local commit_id="$1"
  local current_commit=$(git rev-parse HEAD)

  if [ "$current_commit" != "$commit_id" ]; then
    echo "Switching to the specified commit $commit_id..."
    git checkout $commit_id
    if [ $? -eq 0 ]; then
      echo "Switched to the specified commit $commit_id successfully."
    else
      echo "Failed to switch to the specified commit $commit_id."
    fi
  else
    echo "The current branch is already on the specified commit $commit_id."
  fi
}

if [ -d "ethercat" ]; then
  echo "The 'ethercat' directory already exists..."
  cd ethercat

  current_branch=$(git symbolic-ref --short -q HEAD)
  
  if [ "$current_branch" == "$branch_name" ]; then
    echo "The 'ethercat' repository is already cloned and on the '$branch_name' branch."
    check_commit_id "$commit_id"
  else
    git checkout $branch_name
    if [ $? -eq 0 ]; then
      echo "Switched to the '$branch_name' branch successfully."
      make clean
      check_commit_id "$commit_id"
    else
      echo "Failed to switch to the '$branch_name' branch."
    fi
  fi
  cd ../
else
  echo "Cloning 'ethercat' repository and checking out the '$branch_name' branch..."
  git clone ${repo_url} ${directory_name}
  if [ $? -eq 0 ]; then
    cd "$directory_name"
    git checkout $branch_name
    check_commit_id "$commit_id"
    if [ $? -eq 0 ]; then
      echo "Cloned 'ethercat' repository and checked out the '$branch_name' branch successfully."
    else
      echo "Failed to checkout the '$branch_name' branch."
    fi
  else
    echo "Failed to clone the 'ethercat' repository."
  fi
fi

echo ""
echo "==============================Compiling IgH-EtherCAT=============================="

cd ethercat

./bootstrap

CC=${toolchains_path}/riscv64-buildroot-linux-gnu-gcc
CXX=${toolchains_path}/riscv64-buildroot-linux-gnu-g++

./configure --prefix=${buildroot_initramfs_sysroot_path} --with-linux-dir=${linux_path} --enable-8139too=no --enable-generic=yes --enable-hrtimer=yes CC=${CC} CXX=${CXX} --host=riscv64-buildroot-linux-gnu

echo ""
echo "--------------------make--------------------"

make

echo ""
echo "--------------------make modules--------------------"

make ARCH=riscv CROSS_COMPILE=${toolchains_path}/riscv64-buildroot-linux-gnu- modules VERBOSE=1 

echo ""
echo "--------------------make install--------------------"

make DESTDIR=${buildroot_initramfs_sysroot_path} install

echo ""
echo "--------------------make modules_install--------------------"

kernel_release=$(cat ${kernel_release_file})

if [ -d "${buildroot_initramfs_sysroot_path}/lib/modules/${kernel_release}" ]; then
  echo "The directory exists. Proceeding with make modules_install..."
  make INSTALL_MOD_PATH=${install_mod_path} modules_install
  echo "modules has been installed in the ${install_mod_path}/lib/modules/${kernel_release}"
else
  echo "The directory does not exist. Please check the path."
fi

echo ""
echo "==============================Compiling EtherCAT Application=============================="

cd ../application

CC=${CC} make

cp ectest_PV  ${buildroot_initramfs_sysroot_path}/root

echo ""
echo "==============================Generating 'start_ethercat_master.sh'=============================="

cd ${buildroot_initramfs_sysroot_path}/root

cat <<EOF > start_ethercat_master.sh
#!/bin/bash

if [ \$# -eq 0 ]; then
  echo "Usage: $0 <MAC address>"
  exit 1
fi

mac_address="\$1"

modprobe phylink
insmod /lib/modules/${kernel_release}/ethercat/master/ec_master.ko main_devices="\$mac_address"
insmod /lib/modules/${kernel_release}/ethercat/devices/ec_generic.ko
modprobe pcs_xpcs

cd /lib/modules/${kernel_release}/kernel/drivers/net/ethernet/stmicro/stmmac/
insmod stmmac.ko
insmod stmmac-platform.ko
insmod dwmac-starfive-plat.ko

cd /root
EOF

chmod +x start_ethercat_master.sh

echo ""
echo "==============================Re-compiling SDK=============================="

cd ${current_path}/../../

make

cd ${current_path}
