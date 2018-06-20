sudo tar xvfp debian-9.3-minimal-armhf-2017-12-09/armhf-rootfs-debian-stretch.tar -C /media/rootfs/ | pv -
sync
sudo sync
sudo chown root:root /media/rootfs/
sudo chmod 755 /media/rootfs/
sudo sh -c "echo '/dev/mmcblk0p1  /  auto  errors=remount-ro  0  1' >> /media/rootfs/etc/fstab"
sudo sh -c "echo 'auto lo
iface lo inet loopback
  
auto eth0
iface eth0 inet dhcp' >> /media/rootfs/etc/network/interfaces"
sync
