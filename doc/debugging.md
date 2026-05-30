# 嵌入式调试指南

> 本项目运行在 RK3566 目标板上，宿主机为 WSL2。覆盖三种调试场景和对应工具。

---

## 一、调试工具总览

| 场景 | 工具 | 适用情况 |
|------|------|----------|
| 程序崩溃（Segfault） | Core Dump + gdb-multiarch | 事后分析崩溃现场 |
| 程序卡死（Hang） | strace 跟踪系统调用 | 找到卡在哪个线程、哪个调用 |
| 实时断点调试 | gdbserver + gdb-multiarch | 需要交互式断点、单步、查看变量 |

---

## 二、环境准备

### 2.1 宿主机工具

```bash
sudo apt install gdb-multiarch
```

### 2.2 目标板工具

| 文件 | 说明 |
|------|------|
| `scripts/gdbserver` | 从 Buildroot debug-root 提取的 aarch64 gdbserver |
| `strace` | 板子自带（Buildroot 预装） |

### 2.3 编译带调试符号的版本

`ai-cam-ui-widget.pro` 中添加：

```qmake
CONFIG += c++11 debug
QMAKE_CFLAGS_DEBUG += -g
QMAKE_CXXFLAGS_DEBUG += -g
```

重新编译：

```bash
cd build-aarch64
/home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host/bin/qmake \
    -o Makefile ../ai-cam-ui-widget.pro \
    -spec devices/linux-buildroot-g++ \
    CONFIG+=debug CONFIG+=qml_debug
make -j2
```

验证：`readelf -S build-aarch64/ai-cam-ui-widget | grep debug`

> 发布时去掉 `debug` 和 `-g`，减小二进制和 core 文件体积。

---

## 三、场景一：程序崩溃（Core Dump 分析）

程序发生 Segfault 时，内核生成 core 文件（内存快照），用 gdb-multiarch 事后还原崩溃现场。

### 3.1 板子上开启 Core Dump

```bash
ulimit -c unlimited
echo "/tmp/core.%e.%p" > /proc/sys/kernel/core_pattern
/root/ai-cam-ui-widget
```

> `ulimit -c` 和 `core_pattern` 只对当前 shell 生效，重启需重新设置。

崩溃后确认 core 文件：

```bash
ls -la /tmp/core.*
```

### 3.2 传输 Core 文件到 WSL

板子上起 HTTP 服务：

```bash
cd /tmp && python3 -m http.server 8080
```

WSL 中下载：

```bash
wget http://<板子IP>:8080/core.QThread.977 -O /tmp/core.QThread.977
```

### 3.3 GDB 分析

```bash
gdb-multiarch build-aarch64/ai-cam-ui-widget /tmp/core.QThread.977 \
  -ex "set sysroot /home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host/aarch64-buildroot-linux-gnu/sysroot" \
  -ex "bt" \
  -ex "info threads" \
  -ex "thread apply all bt" \
  -ex quit
```

> **必须加 `set sysroot`**，否则共享库函数名显示 `??`。

常用命令：

| 命令 | 说明 |
|------|------|
| `bt` | 当前崩溃线程的调用栈 |
| `info threads` | 列出所有线程 |
| `thread apply all bt` | 所有线程的调用栈 |
| `thread N` / `frame N` | 切换线程 / 栈帧 |
| `p variable` | 打印变量值 |
| `info registers` | 查看寄存器（定位空指针） |
| `x/Ngx addr` | 查看内存 |

### 3.4 常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| 没有 core 文件 | `ulimit -c` 默认 0 | `ulimit -c unlimited` |
| GDB 显示 `??` | 没设 sysroot | 加 `set sysroot` |
| core 和二进制不匹配 | 版本不一致 | 确保同一次编译产出 |
| 被 SIGKILL 杀死无 core | SIGKILL 不可捕获 | 检查 `dmesg` OOM 记录 |

---

## 四、场景二：程序卡死（strace 分析）

程序卡住但不崩溃时，用 strace 跟踪所有线程的系统调用，定位卡在哪个线程。

### 4.1 跟踪方法

```bash
# 板子上：从头启动并跟踪所有线程（-f 跟踪子线程）
strace -f /root/ai-cam-ui-widget 2>&1 | tee /tmp/strace.log

# 复现卡住后 Ctrl+C 停掉 strace
# 将 strace.log 拷贝到宿主机分析
```

如果需要附加到已运行的进程：

```bash
# 先解除 ptrace 限制
echo 0 > /proc/sys/kernel/yama/ptrace_scope
strace -p $(pidof ai-cam-ui-widget) -f 2>&1 | tee /tmp/strace.log
```

### 4.2 分析方法

**第一步：统计各线程最后活动**

```bash
# 列出所有线程 PID
grep -oP '\[pid\s+\d+\]' strace.log | sort -u

# 每个 PID 最后出现的位置
for pid in $(grep -oP '\[pid\s+\K\d+' strace.log | sort -u); do
  line=$(grep -n "\[pid   $pid\]" strace.log | tail -1)
  echo "PID $pid: $line"
done
```

**第二步：找到停止工作的线程**

```bash
# 每个线程最后一次有意义的系统调用（排除 idle 调用）
for pid in <PID列表>; do
  last=$(grep "\[pid   $pid\]" strace.log | \
    grep -v -E "ppoll|futex|nanosleep|SIGIO|rt_sigreturn|ERESTART|read\(34" | tail -1)
  echo "PID $pid: $last"
done
```

**第三步：搜索错误日志**

```bash
grep -i "error\|fail\|EBUSY\|EINVAL" strace.log | grep "write(2,"
```

### 4.3 关键判据

| 线程状态 | 含义 |
|----------|------|
| ppoll/pselect6 循环 | Qt 事件循环，正常 idle |
| futex WAIT | 等待锁或条件变量 |
| clock_nanosleep 循环 | 定时轮询 |
| read 返回 0 循环 | 读 signalfd，正常 idle |
| **最后调用是 error/fail** | **该线程出错后停止，很可能是卡住的根因** |

### 4.4 实际案例

录像画面卡住，strace 分析发现摄像头线程（PID 887）最后活动：

```
VIDIOC_S_FMT failed: EBUSY
RTNodeSourceV4L2 {openDev:300} failed to open task node for video/v4l2
```

ISP 设备被占用，摄像头停止出帧。详见 `troubleshooting.md` 问题 2。

---

## 五、场景三：实时断点调试（gdbserver）

需要交互式调试时使用。采用 Host-Target 架构，通过 WiFi TCP 直连：

```
宿主机 (WSL2)                目标板 (RK3566)
gdb-multiarch ── TCP ────▶ gdbserver :1234
                            ai-cam-ui-widget
```

### 5.1 从头启动调试

```bash
# 板子上
gdbserver :1234 /root/ai-cam-ui-widget

# WSL 中
./scripts/debug.sh
```

### 5.2 附加到运行中的进程

```bash
# 板子上
gdbserver :1234 --attach $(pidof ai-cam-ui-widget)

# WSL 中
./scripts/debug.sh
```

GDB 连上后：

```gdb
(gdb) set sysroot /home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host/aarch64-buildroot-linux-gnu/sysroot
(gdb) thread apply all bt
```

### 5.3 常用命令

| 命令 | 说明 |
|------|------|
| `b function` / `b file.cpp:line` | 设断点 |
| `c` | 继续运行 |
| `Ctrl+C` | 中断程序 |
| `n` / `s` | 单步（不进入 / 进入函数） |
| `p variable` | 打印变量 |
| `watch variable` | 监视变量变化 |
| `bt` / `frame N` | 调用栈 / 切换栈帧 |

### 5.4 已知限制

gdbserver（Buildroot 2018）与 gdb-multiarch 9.2 存在协议兼容问题，`thread apply all bt` 可能报 `Protocol error`。此时改用 strace 或 core dump。

---

## 六、WiFi 网络配置

板子仅支持 2.4GHz WiFi。

```bash
# 板子上
cat > /tmp/wpa_supplicant.conf << 'EOF'
network={
    ssid="你的WiFi名"
    psk="你的WiFi密码"
    key_mgmt=WPA-PSK
}
EOF
wpa_supplicant -B -i wlan0 -c /tmp/wpa_supplicant.conf
sleep 5
udhcpc -i wlan0
ifconfig wlan0
```

也可使用项目脚本：`scripts/connect_wifi.sh`

---

## 七、附录

### 7.1 gdbserver 来源

```
/home/lzp/linux_sdk/buildroot/output/rockchip_rk3566/host/aarch64-buildroot-linux-gnu/debug-root/usr/bin/gdbserver
```

### 7.2 编译环境问题

| 问题 | 解决 |
|------|------|
| RK Buildroot 编译 gdb 失败 | 直接用 debug-root 中已有的 gdbserver |
| GDB 14.2 编译 gdbserver 缺 GMP/MPFR | 放弃，用 Buildroot 已有版本 |
| 二进制缺少调试符号 | `.pro` 中显式加 `QMAKE_CFLAGS_DEBUG += -g` |
