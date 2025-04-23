#!/bin/bash

current_path=$(pwd)
work_path=${current_path}/../../work
buildroot_initramfs_sysroot_path=${work_path}/buildroot_initramfs_sysroot
buildroot_rootfs_path=${work_path}/buildroot_rootfs
linux_path=${work_path}/linux
kernel_release_file=${linux_path}/include/config/kernel.release
install_mod_path=${work_path}/module_install_path
toolchains_path=${work_path}/buildroot_initramfs/host/bin

# --------------- Determining whether to compile the SD card image. ---------------
sdcard_img=0

# Determine if compile 'sdcard.img' and check if the root filesystem is compiled.
if [ "$#" -eq 0 ]; then
  echo "Compile 'image.fit' only."
  echo "If you need to compile 'sdcard.img', usage: '$0 img'"
elif [ "$1" = "img" ]; then
  if [ -d "${buildroot_rootfs_path}" ]; then
    echo "Compile both 'image.fit' and 'sdcard.img'"
    sdcard_img=1
  else
    echo "Could not add application to sdcard image, please run 'make buildroot_rootfs -j$(nproc)' first."
    exit 1
  fi
else
  echo "The argument is not 'img'"
fi

# --------------- Check if the kernel is on the corresponding commit. ---------------
if [[ -d "../../linux" ]]; then
  echo "The 'linux' directory already exists..."

  cd ../../linux || exit 1
  linux_branch=$(git rev-parse --abbrev-ref HEAD)
  git pull

  case "$linux_branch" in
    "vf2-6.6.y-devel-rtlinux")
      patch_dir="6.6"
      ;;
    "vf2-6.12.y-devel-rtlinux")
      patch_dir="6.12"
      ;;
    *)
      echo "Error: Unsupported branch '$linux_branch'"
      exit 1
      ;;
  esac

  # Iterate through patches starting with "0001*"in the linux_patches directory
  echo "Applying patches."
   for patch_file in ${current_path}/linux_patch/${patch_dir}/000*; do
    if [[ "$patch_file" =~ /000[1-3]*-.*\.patch ]]; then
      # Applying patches by using 'git am', and ignore the possible conflicts
      git am --3way --ignore-whitespace "$patch_file" || {
        # if 'git am' fails, output error messages and skip this patch.
        echo "Failed to apply patch: $patch_file"
        # clean 'git am' status.
        git am --abort
        # Skip the current loop iteration and continue with the next patch file.
        continue
      }
      echo "Patch applied: $patch_file"
    fi
  done

  cd ../
  make clean
  make -j$(nproc)
  cd ${current_path}
fi

if [ -d "${buildroot_initramfs_sysroot_path}" ] && [ -d "${linux_path}" ] && [ -d "${install_mod_path}" ]; then
  echo "Both directories(${buildroot_initramfs_sysroot_path} and ${linux_path}) exist. Proceeding with the script..."
else
  echo "One or both of the directories(${buildroot_initramfs_sysroot_path} and ${linux_path}) do not exist, the SDK may not have been fully compiled. Please check."
  exit 1
fi

# --------------- Check EtherCAT repo. ---------------
repo_url="https://gitlab.com/etherlab.org/ethercat.git"

# Don't using stable-1.5, witch will cause:
# Making install in tool
# make[1]: 进入目录“/home/atlas/visionfive/soft_3rdpart/ethercat/tool”
#   CXXLD    ethercat
# /home/atlas/visionfive/work/buildroot_initramfs/host/lib/gcc/riscv64-buildroot-linux-gnu/12.2.0/../../../../riscv64-buildroot-linux-gnu/bin/ld: ../master/soe_errors.o: can't link soft-float modules with double-float modules
# /home/atlas/visionfive/work/buildroot_initramfs/host/lib/gcc/riscv64-buildroot-linux-gnu/12.2.0/../../../../riscv64-buildroot-linux-gnu/bin/ld: failed to merge target specific data of file ../master/soe_errors.o
# collect2: error: ld returned 1 exit status
# branch_name="stable-1.5"
ethercat_branch_name="master"
ethercat_commit_id="4f529ade671ac60602a72168b368668f39c0855c"

if [ -d "ethercat" ]; then
  echo "The 'ethercat' directory already exists..."

  cd ethercat

  ethercat_current_branch=$(git symbolic-ref --short -q HEAD)

  if [ "$ethercat_current_branch" == "$ethercat_branch_name" ]; then
    echo "The 'ethercat' repository is already cloned and on the '$ethercat_branch_name' branch."
    if [[ $(git rev-parse HEAD) != $ethercat_commit_id ]]; then
      git checkout $ethercat_commit_id
    fi
  else
    git checkout $ethercat_branch_name
    if [ $? -eq 0 ]; then
      echo "Switched to the '$ethercat_branch_name' branch successfully."
      make clean
      if [[ $(git rev-parse HEAD) != $ethercat_commit_id ]]; then
        git checkout $ethercat_commit_id
      fi
    else
      echo "Failed to switch to the '$ethercat_branch_name' branch."
    fi
  fi
else
  echo "Cloning 'ethercat' repository and checking out the '$ethercat_branch_name' branch..."

  git clone ${repo_url}

  if [ $? -eq 0 ]; then
    cd ethercat

    git checkout $ethercat_branch_name
    git checkout "$ethercat_commit_id"

    if [ $? -eq 0 ]; then
      echo "Cloned 'ethercat' repository and checked out the '$ethercat_branch_name' branch successfully."
    else
      echo "Failed to checkout the '$ethercat_branch_name' branch."
    fi
  else
    echo "Failed to clone the 'ethercat' repository."
    exit 1
  fi
fi

# Iterate through patches starting with "0001*" to "0003*" in the ethercat_patches directory
echo "Applying patches."
for patch_file in ../ethercat_patch/${patch_dir}/000*; do
  if [[ "$patch_file" =~ /000[1-3]*-.*\.patch ]]; then
    # Applying patches by using 'git am', and ignore the possible conflicts
    git am --3way --ignore-whitespace "$patch_file" || {
      # if 'git am' fails, output error messages and skip this patch.
      echo "Failed to apply patch: $patch_file"
      # clean 'git am' status.
      git am --abort
      # Skip the current loop iteration and continue with the next patch file.
      continue
    }
    echo "Patch applied: $patch_file"
  fi
done

cd ../

echo ""
echo "==============================Compiling IgH-EtherCAT=============================="

cd ethercat

./bootstrap

CC=${toolchains_path}/riscv64-buildroot-linux-gnu-gcc
CXX=${toolchains_path}/riscv64-buildroot-linux-gnu-g++

./configure --prefix=${buildroot_initramfs_sysroot_path} --with-linux-dir=${linux_path} --enable-8139too=no --enable-generic=yes --enable-hrtimer=yes --enable-dwc=yes --with-dwc-kernel=6.12 CC=${CC} CXX=${CXX} --host=riscv64-buildroot-linux-gnu

echo ""
echo "--------------------make--------------------"

make -j$(nproc)

echo ""
echo "--------------------make modules--------------------"

make ARCH=riscv CROSS_COMPILE=${toolchains_path}/riscv64-buildroot-linux-gnu- modules VERBOSE=1

echo ""
echo "--------------------make install--------------------"

make DESTDIR=${buildroot_initramfs_sysroot_path} install

echo ""
echo "--------------------copy module files--------------------"

ethercat_file_path=${current_path}/ethercat_files

if [ -d "${ethercat_file_path}" ]; then
  rm -r ${ethercat_file_path}
fi

mkdir ${ethercat_file_path}

kernel_release=$(cat ${kernel_release_file})

if [ -d "${buildroot_initramfs_sysroot_path}/lib/modules/${kernel_release}" ]; then
  echo "The directory exists. Proceeding with make modules_install..."
  # make INSTALL_MOD_PATH=${install_mod_path} modules_install
  make INSTALL_MOD_PATH=${ethercat_file_path} modules_install
  cp -r ${ethercat_file_path}/lib/modules/${kernel_release}/ethercat ${buildroot_initramfs_sysroot_path}/root
  echo "modules has been copied to ${buildroot_initramfs_sysroot_path}/root"

  if [ $sdcard_img -eq 1 ]; then
    echo "Copy ethercat_files to '${buildroot_rootfs_path}/target/root'."
    cp -r ${ethercat_file_path}/lib/modules/${kernel_release}/ethercat ${buildroot_rootfs_path}/target/root

    if [ $? -eq 0 ]; then
      echo "Copy modules to '${buildroot_rootfs_path}/target/root' success."
    else
      echo "Copy modules to '${buildroot_rootfs_path}/target/root' fail."
      cd ${current_path}
      exit 1
    fi
  fi

else
  echo "The directory does not exist. Please check the path."
  cd ${current_path}
  exit 1
fi

echo ""
echo "==============================Compiling EtherCAT Application=============================="

cd ../application

CC=${CC} make

cp ectest_PV ${buildroot_initramfs_sysroot_path}/root

if [ $sdcard_img -eq 1 ]; then
  echo "Copy application to '${buildroot_rootfs_path}/target/root'."
  cp ectest_PV ${buildroot_rootfs_path}/target/root

  if [ $? -eq 0 ]; then
    echo "Copy application to '${buildroot_rootfs_path}/target/root' success."
  else
    echo "Copy application to '${buildroot_rootfs_path}/target/root' fail."
    cd ${current_path}
    exit 1
  fi
fi

cd ../

echo ""
echo "==============================Copying 'start_ethercat_master.sh'=============================="

chmod +x start_ethercat_master_v6.sh

cp start_ethercat_master_v6.sh ${buildroot_initramfs_sysroot_path}/root

if [ $sdcard_img -eq 1 ]; then
  echo "Copy script to '${buildroot_rootfs_path}/target/root'."
  cp start_ethercat_master_v6.sh ${buildroot_rootfs_path}/target/root

  if [ $? -eq 0 ]; then
    echo "Copy script to '${buildroot_rootfs_path}/target/root' success."
  else
    echo "Copy script to '${buildroot_rootfs_path}/target/root' fail."
    cd ${current_path}
    exit 1
  fi
fi

echo ""
echo "==============================Re-compiling SDK=============================="

cd ${current_path}/../../

make -j$(nproc)

if [ $sdcard_img -eq 1 ]; then
  make img
fi

cd ${current_path}
