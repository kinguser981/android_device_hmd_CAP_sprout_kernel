make clean && make mrproper
export ARCH=arm64
export SUBARCH=arm64
export CROSS_COMPILE=aarch64-linux-android-
export CLANG_TRIPLE=aarch64-linux-gnu-
make capspout_defconfig CROSS_COMPILE=aarch64-linux-android-
PATH="/workspace/Empty/clang/bin:/workspace/Empty/toolchains-for-snapdragon/aarch64-linux-android-4.9-master/aarch64-linux-android/bin:${PATH}" \
make -j$(nproc --all) O=out CC=clang CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-android- -C $(pwd)
