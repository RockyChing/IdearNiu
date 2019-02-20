#!/bin/sh

ifconfig wlan0 down
killall -q wpa_supplicant
killall -q udhcpd
killall -q udhcpc
killall -q hostapd
ifconfig wlan0 up
sleep 2

echo "start connecting..."
wpa_supplicant -B -Dnl80211 -iwlan0 -c/etc/wifi/wpa_supplicant.conf
rm /tmp/wpa_running
while [ 1 ]
do
    ps | grep -i wpa_supplicant | grep -v grep > /tmp/wpa_running
    if [ -s /tmp/wpa_running ]; then
       break
    else
       sleep 1
    fi
done

udhcpc -R -i wlan0 -p /var/run/udhcpc.pid
echo
