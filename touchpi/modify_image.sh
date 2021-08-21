#!/bin/zsh
## (c) Bernhard Tittelbach 2017

local TARGETIMAGE=${1}
local BBHOSTNAME=${2:-ventilation.realraum.at}
local MOUNTPTH=$(mktemp -d)
local APTSCRIPT=./aptscript.sh
local CONFIGTXTAPPEND=./config.txt.append
local LOCALROOT=./rootfs
local MYSSHPUBKEY=~/.ssh/id_ed25519.pub
local PIUSER=pi
local FANCYNAME="$(date +%Y-%m-%d)-ventilation.realraum"

[[ -z $TARGETIMAGE ]] && {echo "Give .img filename"; exit 1}

echo "Modify and and configure $TARGETIMAGE ?"
read -q || exit 1

## will be defined in mountimagemvvar
local LOOPDEV
local LOOPNUM

extendimage() {
  local num_expected_partitions=2
  local addrootsize=$((1*1024*1024*1024))
  local targetsize=$((7*1024*1024*1024))
  local p3home_minsize=$((768*1024*1024))
  local num_partitions_in_image=$(sfdisk -d -q "$TARGETIMAGE" | tail "+6" | grep -v '^$' | wc -l)
  [[ $num_partitions_in_image -eq $num_expected_partitions ]] || return 1
  ./extend_image_and_resize_last_partition.py "$TARGETIMAGE" +"$addrootsize"
  ./extend_image_and_append_partition.py "$TARGETIMAGE" +"$p3home_minsize"
  ./extend_image_and_append_partition.py "$TARGETIMAGE" "$targetsize"
}

umountimage() {
  [[ -e ${MOUNTPTH}/etc/ld.so.preload.disabled ]] && sudo mv ${MOUNTPTH}/etc/ld.so.preload.disabled ${MOUNTPTH}/etc/ld.so.preload
  [[ -e ${LOOPDEV}p4 ]] && sudo umount ${LOOPDEV}p4
  [[ -e ${LOOPDEV}p3 ]] && sudo umount ${LOOPDEV}p3
  [[ -e ${LOOPDEV}p1 ]] && sudo umount ${LOOPDEV}p1
  [[ -e ${LOOPDEV}p2 ]] && sudo umount ${LOOPDEV}p2
  sleep 1
  sudo kpartx  -d -v $TARGETIMAGE
  [[ -n ${LOOPNUM} ]] && sudo losetup -d /dev/${LOOPNUM}
}

mountimagemvvar() {
    kpartoutputfile=$(mktemp)
    sudo kpartx  -a -v $TARGETIMAGE | tee "$kpartoutputfile" || return $?
    [[ -s $kpartoutputfile ]] || return 1
    LOOPNUM=$(head -n1 "$kpartoutputfile" | cut -d' ' -f 3 | sed 's:p[0-9]$::')
    LOOPDEV=/dev/mapper/${LOOPNUM}
    rm -f "$kpartoutputfile"
    sleep 1.0
    sudo e2fsck -f ${LOOPDEV}p2
    sudo resize2fs ${LOOPDEV}p2 || return $?
    sudo mount ${LOOPDEV}p2 ${MOUNTPTH} -o acl || return $?
    sudo mount ${LOOPDEV}p1 ${MOUNTPTH}/boot || return $?
    if [[ -e ${LOOPDEV}p3 ]] ; then
        if ! sudo mount ${LOOPDEV}p3 ${MOUNTPTH}/home -o acl; then
            sudo mkfs -L home -t ext4 -b 4096 ${LOOPDEV}p3 || exit 2
            mvhome || return $?
            sudo mount ${LOOPDEV}p3 ${MOUNTPTH}/home -o acl || return $?
        fi
    fi
    if [[ -e ${LOOPDEV}p4 ]] ; then
        if ! sudo mount ${LOOPDEV}p4 ${MOUNTPTH}/var; then
            sudo mkfs -L var -t ext4 -b 4096 ${LOOPDEV}p4 || exit 2
            mvvar || return $?
            sudo mount ${LOOPDEV}p4 ${MOUNTPTH}/var || return $?
        fi
    fi
}

mvhome() {
  local TDIR=$(mktemp -d)
  sudo mount ${LOOPDEV}p3 $TDIR || return $?
  sudo mv ${MOUNTPTH}/home/*(D) ${TDIR}/ || return $?
  sudo umount ${LOOPDEV}p3 || return $?
  echo '/dev/mmcblk0p3  /home           ext4    defaults,noatime,acl  0       1' | sudo tee -a ${MOUNTPTH}/etc/fstab
  sync
}

mvvar() {
  local TDIR=$(mktemp -d)
  sudo mount ${LOOPDEV}p4 $TDIR || return $?
  sudo mv ${MOUNTPTH}/var/*(D) ${TDIR}/ || return $?
  sudo umount ${LOOPDEV}p4 || return $?
  echo '/dev/mmcblk0p4  /var            ext4    defaults,noatime  0       1' | sudo tee -a ${MOUNTPTH}/etc/fstab
  sync
}

modify_fstab() {
  sudo sed 's:\(\s/boot\s\+\S\+\s\+defaults\):\1,ro:' -i ${MOUNTPTH}/etc/fstab
  sudo sed 's:\(\s/\s\+\S\+\s\+defaults\):\1,ro:' -i ${MOUNTPTH}/etc/fstab
  echo 'tmpfs           /tmp            tmpfs    defaults,size=64m  0       1' | sudo tee -a ${MOUNTPTH}/etc/fstab
  echo '/dev/disk/by-path/platform-3f980000.usb-usb-0:1.3:1.0-scsi-0:0:0:0      /media/usbstick auto    x-systemd.automount,noauto,rw,x-systemd.idle-timeout=10,x-systemd.device-timeout=2,nofail  0 0' | sudo tee -a ${MOUNTPTH}/etc/fstab
  echo '/dev/disk/by-path/platform-3f980000.usb-usb-0:1.3:1.0-scsi-0:0:0:0-part1        /media/usbstick1        auto    x-systemd.automount,x-systemd.idle-timeout=10,noauto,rw,x-systemd.device-timeout=2,nofail  0  0' | sudo tee -a ${MOUNTPTH}/etc/fstab
  sudo mkdir -p ${MOUNTPTH}/media/usbstick ${MOUNTPTH}/media/usbstick1
}

local -a nspawnbindopts
config_qemu_nspawn_binds() {
  if [[ -L "$MOUNTPTH"/lib ]]; then
    ## from 2021 on raspios decided to pool /lib into /usr/lib und make /lib a symlink. That does not work well with systemd-nspawn --bind
    nspawnbindopts=(--bind-ro /usr/bin/qemu-arm --bind-ro /usr/libexec/qemu-binfmt/arm-binfmt-P --overlay-ro /lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu:/usr/lib/x86_64-linux-gnu --bind-ro /lib64)
  else
    ## for old and sane images
    nspawnbindopts=(--bind-ro /usr/bin/qemu-arm --bind-ro /usr/libexec/qemu-binfmt/arm-binfmt-P --bind-ro /lib/x86_64-linux-gnu --bind-ro /usr/lib/x86_64-linux-gnu/ --bind-ro /lib64)
  fi
}

runchroot() {
  [[ -z $nspawnbindopts ]] && config_qemu_nspawn_binds
  sudo systemd-nspawn --resolv-conf=bind-host $nspawnbindopts -D "$MOUNTPTH" --console=interactive -- $*
}

runchroot_untouchedresolveconf() {
  [[ -z $nspawnbindopts ]] && config_qemu_nspawn_binds
  sudo systemd-nspawn --resolv-conf=off $nspawnbindopts -D "$MOUNTPTH" --console=interactive -- $*
}

runchrootpipe() {
  [[ -z $nspawnbindopts ]] && config_qemu_nspawn_binds
  sudo systemd-nspawn $nspawnbindopts -D "$MOUNTPTH" --console=pipe -- $*
}

disableAutoResizeFS() {
  sudo sed -i 's|init=/usr/lib/raspi-config/init_resize.sh\s*||' "${MOUNTPTH}/boot/cmdline.txt"
  sudo rm -f "${MOUNTPTH}"/etc/init.d/resize2fs_once
}


disabledhcpfor() {
  local interfaces="${1:-usb0}"
  local dhcpcdconf="${MOUNTPTH}/etc/dhcpcd.conf"
  echo -e "\ndenyinterfaces $interfaces" | sudo tee -a "$dhcpcdconf"
}

gobuildandcp() {
( cd "$1"
  export GOARCH=arm
  export GOOS=linux
  export CGO_ENABLED=0
  export GOFLAGS=-mod=mod ##go1.16 bugfix https://github.com/golang/go/issues/44129
  go build
)
  sudo mkdir -p "${2}"
  sudo cp -v "${1}/${1:t}" "${2}"
}


TRAPEXIT() {umountimage}
extendimage || exit 1
mountimagemvvar || exit 2
TRAPINT() {
  print "Caught SIGINT, aborting."
  umountimage
  return $(( 128 + $1 ))
}

## disable ld.so.preload which contains /usr/lib/arm-linux-gnueabihf/libarmmem.so (custom new/delete for RPi) which throws qemu errors
##   will be re-enabled in umountimage
sudo mv ${MOUNTPTH}/etc/ld.so.preload ${MOUNTPTH}/etc/ld.so.preload.disabled || {echo " ERROR could not disable ld.so.preload "; exit 3}

## install/remove packages
runchrootpipe /bin/bash < $APTSCRIPT
## move ld.so.preload AGAIN since apt update might have reinstalled it
[[ -e ${MOUNTPTH}/etc/ld.so.preload ]] && sudo mv ${MOUNTPTH}/etc/ld.so.preload ${MOUNTPTH}/etc/ld.so.preload.disabled

## settings and stuff
sudo rsync -va --chown=root:root --keep-dirlinks ${LOCALROOT}/  ${MOUNTPTH}/

## newest zsh config
sudo cp ~/.zshrc ~/.zshrc.local ${MOUNTPTH}/home/${PIUSER}/
sudo cp ~/.zshrc ~/.zshrc.local ${MOUNTPTH}/root/

## Hostname
echo "$BBHOSTNAME" | sudo tee ${MOUNTPTH}/etc/hostname

## ssh keys
sudo mkdir -p ${MOUNTPTH}/root/.ssh ${MOUNTPTH}/home/pi/.ssh
cat $MYSSHPUBKEY | sudo tee -a ${MOUNTPTH}/root/.ssh/authorized_keys
cat $MYSSHPUBKEY | sudo tee -a ${MOUNTPTH}/home/pi/.ssh/authorized_keys

##chown user dir back from --chown=root:root
sudo chown 1000:1000 -R ${MOUNTPTH}/home/${PIUSER}(N) ${MOUNTPTH}/home/debian(N)
modifyBootConfigForTouch "$MOUNTPTH/boot/config.txt"
disabledhcpfor "usb0"
disabledhcpfor "eth0"
disableAutoResizeFS
sudo cp -rf ${MOUNTPTH}/usr/share/X11/xorg.conf.d/10-evdev.conf ${MOUNTPTH}/usr/share/X11/xorg.conf.d/45-evdev.conf

gobuildandcp ../ventilationinterface ${MOUNTPTH}/home/pi/bin/
sudo rsync --chown=1000:1000 -vr ../ventilationinterface/public ${MOUNTPTH}/home/pi/bin/
sudo setcap cap_net_bind_service=ep ${MOUNTPTH}/home/pi/bin/ventilationinterface
## instead of enable, rsync the symlink
#runchroot /bin/systemctl enable ventilationinterface.service
runchroot /usr/sbin/addgroup pi tty
runchroot /bin/loginctl enable-linger pi
runchroot /bin/systemctl disable networking.service dhcpcd.service
runchroot /bin/systemctl mask networking.service dhcpcd.service
runchroot /bin/systemctl enable systemd-networkd.service ssh.service



runchroot /usr/bin/passwd pi
### ssh-keygen for hostkeys...
runchroot /usr/sbin/dpkg-reconfigure openssh-server
runchroot /usr/bin/chsh -s /bin/zsh root
runchroot /usr/bin/chsh -s /bin/zsh ${PIUSER}

modify_fstab

### install LCD Waveshare driver
git clone --depth 1 https://github.com/waveshare/LCD-show "$MOUNTPTH"/home/pi/LCD-show
runchroot /usr/bin/bash -x /home/pi/LCD-show/LCD32-show


### modify resolv.conf
cat <<EOF >! "$MOUNTPTH"/etc/resolv.conf
nameserver 192.168.127.254
options edns0 trust-ad
search lan realraum.at
EOF

### zero out free space
for zerofile in "$MOUNTPTH"/zero "$MOUNTPTH"/var/zero "$MOUNTPTH"/home/zero "$MOUNTPTH"/boot/zero; do
  sudo dd if=/dev/zero of="$zerofile"
  sudo rm "$zerofile"
done

###
umountimage
sync

### rename image
mv "$TARGETIMAGE" "${FANCYNAME}.img"
echo "compressing image... (this might take a long while)"
zstd --rm "${FANCYNAME}.img"
echo
echo
echo Image moved to "${FANCYNAME}.img.zst"
echo

