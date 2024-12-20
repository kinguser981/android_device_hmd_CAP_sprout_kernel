make clean && make mrproper
export ARCH=arm64
make O=out capspout_defconfig
make -j$(nproc --all) O=out CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=/workspace/Empty/toolchains-for-snapdragon/aarch64-linux-android-4.9-master/bin/aarch64-linux-android-
