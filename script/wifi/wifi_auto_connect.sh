#!/bin/sh

ifconfig wlan0 down

killall -q wpa_supplicant
killall -q udhcpc
killall -q udhcpd
killall -q hostapd

ifconfig wlan0 up
echo "Connecting to $1 ..."
sleep 1

if [ -z $1 ]; then
	echo "Usage: ./wifi_auto_connect.sh ssid or"
	echo "       ./wifi_auto_connect.sh ssid passwd"
	exit 0
fi

WPA_CONF=/tmp/wpa_supplicant.conf

echo "ctrl_interface=/var/run/wpa_supplicant" > ${WPA_CONF}
echo "update_config=1" >> ${WPA_CONF}
echo "" >> ${WPA_CONF}
echo "network={" >> ${WPA_CONF}
echo "ssid=\"$1\"" >> ${WPA_CONF}

if [ -z $2 ]; then
	echo "key_mgmt=NONE" >> ${WPA_CONF}
else
	echo "pairwise=CCMP TKIP" >> ${WPA_CONF}
	echo "group=CCMP TKIP WEP104 WEP40" >> ${WPA_CONF}
	echo "psk=\"$2\"" >> ${WPA_CONF}
fi

echo "}" >> ${WPA_CONF}
echo "" >> ${WPA_CONF}

wpa_supplicant -B -Dnl80211 -iwlan0 -c${WPA_CONF}

udhcpc -R -i wlan0 -p /var/run/udhcpc.pid
echo
echo "Connected!"