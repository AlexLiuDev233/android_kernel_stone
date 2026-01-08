#!/usr/bin/env bash
#
# Copyright (C) 2022-2023 Neebe3289 <neebexd@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Script for krenel compilation !!

# Load variables from config.env
export $(grep -v '^#' config.env | xargs)

# Path
MainPath="$(readlink -f -- $(pwd))"
MainClangPath="${MainPath}/clang"
CrossCompileFlagTriple="aarch64-linux-gnu-"
CrossCompileFlag64="aarch64-linux-gnu-"
CrossCompileFlag32="arm-linux-gnueabi-"

# Clone toolchain
[[ "$(pwd)" != "${MainPath}" ]] && cd "${MainPath}"
function getclang() {
  if [ ! -f "${MainClangPath}/bin/clang" ]; then
    echo "[!] Using AOSP Clang, cloning..."
    mkdir -p clang
    aria2c -s16 -x16 -k1M https://github.com/cctv18/oneplus_sm8650_toolchain/releases/download/LLVM-Clang18-r510928/clang-r510928.zip -o clang.zip &&
    unzip -q clang.zip -d clang &&
    rm -rf clang.zip
    ClangPath="${MainClangPath}"
    export PATH="${ClangPath}/bin:${PATH}"
    cd ${ClangPath}
  else
    echo "[!] Clang already exists. Skipping..."
    ClangPath="${MainClangPath}"
    export PATH="${ClangPath}/bin:${PATH}"
  fi
  if [ ! -f "${MainClangPath}/bin/clang" ]; then
    export KBUILD_COMPILER_STRING="$(${MainClangPath}/bin/clang --version | head -n 1)"
  else
    export KBUILD_COMPILER_STRING="Unknown"
  fi
}

# Enviromental variable
DEVICE_MODEL="Redmi Note 12 5G/POCO X5 5G"
DEVICE_CODENAME="stone"
BUILD_TIME="$(TZ="Asia/Shanghai" date "+%Y%m%d")"
export DEVICE_DEFCONFIG="stone_defconfig"
export ARCH="arm64"
export KBUILD_BUILD_USER="AlexLiuDev233+cctv18"
export KBUILD_BUILD_HOST="localhost"
export KERNEL_NAME="XiaomiKernel"
export SUBLEVEL="v5.4.$(cat "${MainPath}/Makefile" | grep "SUBLEVEL =" | sed 's/SUBLEVEL = *//g')"
IMAGE="${MainPath}/out/arch/arm64/boot/Image"
DTB_IMAGE="${MainPath}/out/arch/arm64/boot/dts/vendor/xiaomi/moonstone.dtb"
CORES="$(nproc --all)"
BRANCH="$(git rev-parse --abbrev-ref HEAD)"

# Start Compile
START=$(date +"%s")

compile(){
sed -i 's/# CONFIG_LLVM_POLLY is not set/CONFIG_LLVM_POLLY=y/g' ${MainPath}/arch/$ARCH/configs/$DEVICE_DEFCONFIG || echo ""
echo "CONFIG_LTO_NONE=y" >> ${MainPath}/arch/$ARCH/configs/$DEVICE_DEFCONFIG
ARGS="O=out \
    ARCH=$ARCH \
    CC=clang \
    LD=ld.lld \
    LLVM=1 \
    LLVM_IAS=1 \
    AR=llvm-ar \
    NM=llvm-nm \
    OBJCOPY=llvm-objcopy \
    OBJDUMP=llvm-objdump \
    STRIP=llvm-strip \
    CLANG_TRIPLE=${CrossCompileFlagTriple} \
    CROSS_COMPILE=${CrossCompileFlag64} \
    CROSS_COMPILE_ARM32=${CrossCompileFlag32}"
make $ARGS $DEVICE_DEFCONFIG
make $ARGS olddefconfig
make $ARGS -j"$CORES" |& tee out/output.txt

   if [[ -f "$IMAGE" ]]; then
      echo "Build Successful."
   else
      echo "❌ Compile Kernel for $DEVICE_CODENAME failed, Check console log to fix it!"
      if [ "$CLEANUP" = "yes" ];then
        cleanup
      fi
      exit 1
   fi
}

# Cleanup function
function cleanup() {
    cd ${MainPath}
    if [ "$CLEANUP" = "yes" ];then
      sudo rm -rf out/
    fi
}

getclang
compile
END=$(date +"%s")
DIFF=$(($END - $START))
