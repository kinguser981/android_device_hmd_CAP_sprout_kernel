make clean && make mrproper
export ARCH=arm64
export SUBARCH=arm64
export CROSS_COMPILE=aarch64-linux-android-
export CLANG_TRIPLE=aarch64-linux-gnu-
make capspout_defconfig CROSS_COMPILE=aarch64-linux-android-
PATH="/workspace/Empty/clang/bin:/workspace/Empty/toolchains-for-snapdragon/aarch64-linux-android-4.9-master/aarch64-linux-android/bin:${PATH}" \
make -j$(nproc --all) O=out CC=clang CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-android- -C $(pwd)



make clean && make mrproper
make ARCH=arm64 SUBARCH=arm64 CROSS_COMPILE=/workspace/Empty/gcc/bin/aarch64-linux-android-
make CROSS_COMPILE=/workspace/Empty/gcc/bin/aarch64-linux-android- ARCH=arm64 capspout_defconfig O=out -j$(nproc --all)

export ARCH=arm64
make O=out capspout_defconfig

make -j$(nproc --all) O=out CC=/workspace/Empty/toolchains/clang-r428724/bin/clang CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=/workspace/Empty/toolchains/aarch64-linux-android-4.9/bin/aarch64-linux-android-









make clean && make mrproper
export ARCH=arm64
make O=out capspout_defconfig
make -j$(nproc --all) O=out CC=/workspace/Empty/toolchains/clang-r428724/bin/clang CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=/workspace/Empty/toolchains/aarch64-linux-android-4.9/bin/aarch64-linux-android-




