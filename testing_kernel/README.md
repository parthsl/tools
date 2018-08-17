### Test kernel in qemu. Useful for testing and debugging kernel from within the given kernel.
Untar qemu-kernel.tar.gz with cmd : `tar xvzf qemu-kernel.tar.gz`

#### Run below command to start qemu in the same console.
```
qemu-system-x86_64 -kernel ./bzImage -initrd initramfs.cpio.gz -enable-kvm -vga virtio -hda linux -nographic -append 'console=ttyS0'
```

#### To rebuild initramfs use below command.
```
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../initramfs.cpio.gz
```

##### Current initramfs contains busybox statically build. Rebuild busybox for other architecture in static mode(use menuconfig to build static version) and copy install/ to current dir.
```
make menuconfig(set BUILD_STATIC=y)
make
make install
cp -a <busy-box-path>/_install/* .
```

#### Enabled with kernel traces
- Goto `cd /sys/kernel/debug/tracing`
- Start tracing `echo 1 > events/enable`
- Read trace `cat trace | less`
- Flush traces with `cat trace_pipe | head`

