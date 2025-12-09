sudo qemu-system-aarch64 \
     -M virt \
     -cpu cortex-a53 \
     -smp 2 -m 4G  \
     -drive if=none,file=IoTG4_2.raw,format=raw,id=hd0 \
     -device virtio-blk-device,drive=hd0 \
     -netdev user,id=net2,hostfwd=tcp::2223-:22 \
     -device virtio-net-device,netdev=net2,mac=52:54:00:00:00:12 \
     -netdev tap,id=net1,ifname=tap11,script=no,downscript=no \
     -device virtio-net-device,netdev=net1,mac=52:54:00:00:00:11 \
     -netdev tap,id=net0,ifname=tap10,script=no,downscript=no \
     -device virtio-net-device,netdev=net0,mac=52:54:00:00:00:10 \
     -kernel linux-5.10-5.10.243-r0/arch/arm64/boot/Image \
     -append "root=/dev/vda rw console=ttyAMA0,115200 rootwait" \
     -nographic \
     -no-reboot

