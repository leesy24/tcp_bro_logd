#! /bin/sh

## version 1.0 at 2020.08.19

CNT=0
MAX=30

sleep 1
while [ ! -e /dev/sda1 ] && [ ! -e /dev/sdb1 ] && [ $CNT -lt $MAX ]; do
	sleep 1
	CNT=$((CNT + 1))
	echo "Retry... " $CNT "find /dev/sda1 or /dev/sdb1"
done
[ $CNT -eq $MAX ] && {
	echo "Timed out to mount USB..."
	exit 1
}

if [ -e /tmp/mmc ]; then
	##----------------(if used MMC from pinetd.c)##
	umount -f /tmp/mmc
fi

echo "make /tmp/usb"
mkdir -p /tmp/usb
if [ -e /dev/sda1 ]; then
	echo "Mount /dev/sda1 on /tmp/usb"
	mount -t vfat /dev/sda1 /tmp/usb
elif [ -e /dev/sdb1 ]; then
	echo "Mount /dev/sdb1 on /tmp/usb"
	mount -t vfat /dev/sdb1 /tmp/usb
fi

exit 0
