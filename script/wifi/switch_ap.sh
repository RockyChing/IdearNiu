#!/bin/sh

killall -q wpa_supplicant
killall -q udhcpc
killall -q udhcpd
killall -q hostapd

echo "start switch to AP mode..."

mkdir -p /var/lib/misc
touch /var/lib/misc/udhcpd.leases
ifconfig wlan0 down
sleep 1
ifconfig wlan0 192.168.1.1 up
sleep 2

hostapd -B /etc/wifi/hostapd.conf
sleep 1
udhcpd /etc/wifi/udhcpd.conf
rm /tmp/udhcpd_running
while [ 1 ]
do
    ps | grep -i udhcpd | grep -v grep > /tmp/udhcpd_running
    if [ -s /tmp/udhcpd_running ]; then
       break
    else
       killall -q udhcpd
       udhcpd /etc/wifi/udhcpd.conf
       sleep 2
    fi
done

echo
