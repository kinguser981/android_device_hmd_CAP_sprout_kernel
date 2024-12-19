make clean && make mrproper
make CONFIG=capspout_defconfig ARCH=arm64 SUBARCH=arm64
PATH="/workspace/Empty/clang/bin:/workspace/Empty/toolchains-for-snapdragon/aarch64-linux-android-4.9-master/aarch64-linux-android/bin:${PATH}" \
make -j$(nproc --all) O=out ARCH=arm64 CC=clang CLANG_TRIPLE=aarch64-linux-gnu- CROSS_COMPILE=aarch64-linux-android- -C $(pwd)
