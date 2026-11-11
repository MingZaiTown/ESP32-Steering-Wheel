import threading
import time

import hidraw as hid

VID, PID   = 0x303A, 0xAAAA

def open_device():
    devs = hid.enumerate(VID, PID)
    if not devs:
        raise SystemExit("没找到设备")
    for d in devs:
        print(f"found: {d['product_string']} sn={d['serial_number']} path={d['path']}")
    dev = hid.device()
    dev.open_path(devs[0]['path'])      # 用 path 打开，避免歧义
    dev.set_nonblocking(0)
    return dev

# 读线程：一直收输入报告
def reader(dev, stop):
    while not stop.is_set():
        data = dev.read(64, timeout_ms=200)   # 阻塞最多 200ms
        if data:
            b = bytes(data)
            print(f"IN  id=0x{b[0]:02X}  {b.hex(' ')}")


def writer(dev, stop):
    while not stop.is_set():
        dev.write(bytes([0x03]) + bytes([1,3,5,4,0,1,0,0]))
        time.sleep(1)

dev  = open_device()
stop = threading.Event()
t = threading.Thread(target=reader, args=(dev, stop), daemon=True)
t2 = threading.Thread(target=writer, args=(dev, stop), daemon=True)
t.start()
t2.start()
t.join()        # 主线程等读线程结束（Ctrl+C 时结束）
