#!/bin/sh
# Buildroot 一键连接 WiFi
# 用法: ./connect_wifi.sh [ssid] [password]
# 默认: SSID=123, 密码=123123123

SSID="${1:-123}"
PASS="${2:-123123123}"
IFACE="wlan0"

# 检查网卡是否存在
if ! ip link show "$IFACE" > /dev/null 2>&1; then
    echo "错误: 未找到网络接口 $IFACE"
    echo "可用接口:"
    ip link show | grep -E '^\d+:' | awk -F': ' '{print "  "$2}'
    exit 1
fi

# 启用网卡
ip link set "$IFACE" up
echo "已启用 $IFACE"

# 杀掉已有的 wpa_supplicant 和 udhcpc
killall wpa_supplicant 2>/dev/null
killall udhcpc 2>/dev/null
sleep 1

# 生成 wpa_supplicant 配置
CONF="/tmp/wpa_supplicant.conf"
cat > "$CONF" <<EOF
network={
    ssid="$SSID"
    psk="$PASS"
    key_mgmt=WPA-PSK
}
EOF
echo "SSID: $SSID"

# 启动 wpa_supplicant
wpa_supplicant -B -i "$IFACE" -c "$CONF"
if [ $? -ne 0 ]; then
    echo "错误: wpa_supplicant 启动失败"
    exit 1
fi
echo "wpa_supplicant 已启动，等待 5 秒建立连接..."
sleep 5

# 获取 IP 地址
echo "正在获取 IP..."
udhcpc -i "$IFACE"
if [ $? -ne 0 ]; then
    echo "错误: DHCP 获取 IP 失败"
    exit 1
fi

# 显示网络信息
echo ""
echo "=== 网络信息 ==="
ip addr show "$IFACE" | grep "inet " | awk '{print "IP: "$2}'
echo "SSID: $SSID"
echo "接口: $IFACE"
