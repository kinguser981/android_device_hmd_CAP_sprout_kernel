make clean && make mrproper
export ARCH=arm64
export SUBARCH=arm64
make O=out capspout_defconfig
make -j$(nproc --all) O=out CC=/workspace/Empty/toolchains/clang-r428724/bin/clang CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=/workspace/Empty/toolchains/aarch64-linux-android-4.9/bin/aarch64-linux-android-
