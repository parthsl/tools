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

# Another simple way to test kernel is by using Alpine minimal root fs
- First obtain Alpine root fs from https://alpinelinux.org/downloads/
- Extract the tar file in a directory say `initramfs`.
- Execute following command for creating `init` script
```bash
#!/bin/sh                                                                          
                                                                                   
mount -t proc none /proc                                                           
mount -t sysfs none /sys                                                           
mount -t devtmpfs dev /dev                                                         
mount -t debugfs none /sys/kernel/debug                                            
mount -o rw /dev/sda1 /mnt/root                                                    

ip link set up dev lo
ip link set eth0 up

cat «EOF
Boot took  $(cut -d' ' -f1 /proc/uptime) seconds')
EOF

ip link set eth0 up
udhcpc -i eth0

exec /sbin/getty -n -l /bin/sh 115200 /dev/console
poweroff -f
```

- build cpio using `find . -print0 | cpio --null -ov --format=newc | gzip -9 > ./initramfs.cpio.gz`
- Next, run qemu command using
```bash
qemu-system-ppc64le --enable-kvm --nographic -kernel ./vmlinux -vga none -machine pseries -smp 80,cores=20,threads=4,sockets=1 -initrd initramfs/initramfs.cpio.gz -netdev user,id=n1 -device virtio-net-pci,netdev=n1 
```
