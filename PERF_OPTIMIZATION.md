# 采集与板卡生命周期性能优化

本版本保持现有命令号、图像格式、编码参数和 DMA 地址不变，主要调整资源生命周期和等待方式。

## 运行后查看实际耗时

板端运行：

```sh
./sdi_pal_pro > /tmp/sdi_pal_perf.log 2>&1
```

操作一次打开板卡、开始采集、停止采集、关闭采集后查看：

```sh
grep '\[PERF\]' /tmp/sdi_pal_perf.log
```

会得到以下运行时实测项：

```text
[PERF] board open ready: ... us
[PERF] TYPE2 chn=... elapsed=... us
[PERF] TYPE9 chn=... elapsed=... us
[PERF] TYPE10 chn=... elapsed=... us
[PERF] board close complete: ... us
```

如果上位机每次启动同时发送 TYPE2 和 TYPE9，则“开始采集”总时间为两项之和；如果参数未变化且只发送 TYPE9，则以 TYPE9 为准。

日志中的 `started` 和 `finished` 都是单调时钟微秒值，因此也可以直接计算连续命令的总耗时：

```text
开始采集总耗时 = TYPE9.finished - TYPE2.started
停止采集耗时   = TYPE10.elapsed
```

## 固定等待削减

以下是可以从代码直接计算的固定等待，不包含 RGA、MPP、DMA 和资源释放的真实执行时间。

| 操作 | 修改前固定等待 | 修改后固定等待 | 固定等待削减 |
|---|---:|---:|---:|
| 开始采集 TYPE2 + TYPE9 | 约 1212 ms | 约 4 ms | 约 1208 ms |
| 停止采集 TYPE10 编码路径 | 约 4905 ms | 约 5 ms | 约 4900 ms |
| 打开板卡 | 200 ms PCIe 线程盲等待 | 0 ms，改为 ready 事件 | 约 200 ms |
| 关闭板卡，两路均未运行 | 约 9612 ms | 无固定等待 | 约 9612 ms |

TYPE10 修改前约 4905 ms 的构成：

- PCIe 寄存器顺序写入约 5 ms。
- 停止入口 `sleep(1)`，1000 ms。
- RGA 线程固定等待，1000 ms。
- OSD 两个线程各固定等待，2000 ms。
- 裁剪线程固定等待，100 ms。
- MPP 退出和阶段等待，200 ms。
- `drop_caches` 三次等待，600 ms。

修改后这些固定等待全部移除，停止时等待真实在途任务结束并 join 工作线程。

## 资源申请调整

修改前 `pcie_impl_init()` 在板卡打开阶段一次申请约 1.5 GiB 虚拟空间，包括：

- 80 MiB DMA 读缓冲。
- 16 MiB 公共缓冲。
- 4 路最大 4K 写缓冲，每路约 288 MiB。
- 一路约 288 MiB 复位缓冲。
- MPP 公共写缓冲。

修改后板卡打开阶段只申请 DMA 读缓冲、公共缓冲和小型 MPP 缓冲：

- 4 路最大写缓冲改为首次调用 `align_write_buf_get()` 时分配。
- 复位缓冲改为首次调用时分配。
- OSD 的 128 MiB 初始化由 `malloc + memset` 改为 `calloc`。

因此使用一路采集时，不会再提前申请四路写缓冲和复位缓冲。

## 预期板端范围

下面是工程目标，不是当前开发机的实测值。最终数据以 `[PERF]` 日志为准：

| 操作 | 目标 |
|---|---:|
| 已配置后 TYPE9 到 ACK | 小于 100 ms |
| TYPE2 + TYPE9 完整启动 | 比原版本减少至少 1.2 秒；异常旧 DMA 最多增加 50 ms |
| TYPE10 完整停止到 ACK | 通常 50～250 ms |
| 板卡打开 ready | 目标小于 300 ms |
| 板卡关闭 complete | 目标小于 300 ms |

停止耗时主要取决于停止时正在处理的一帧以及 MPP teardown。DMA 或 MPP 异常阻塞时，最多等待 `STREAM_STOP_TIMEOUT_MS`，然后返回停止超时并保留资源。

## PCIe DMA 迟到返回保护

停止采集时先关闭 DMA admission 并递增 generation，再关闭 FPGA 采集入口和 reset 全局描述符队列。

PCIe 读线程在发起 DMA 前记录当前 generation。DMA 返回时如果 generation 已变化或 admission 已关闭，则直接丢弃该次 DMA，不把描述符放入分析队列。分析线程取出描述符后还会再次校验 generation。

TYPE9 启动前会：

```text
暂停 DMA admission
递增 generation
reset 全局 PCIe 描述符队列
reset VFIFO
打开 FPGA 采集入口
启动采集状态
恢复 DMA admission
```

如果停止前启动的 DMA 在 TYPE9 准备阶段仍未返回，最多等待 50 ms；超时则 TYPE9 返回失败，避免在 PCIe 读线程仍被旧 DMA 占用时产生“启动成功但没有数据”的假启动。

异常日志：

```text
[INFO] drop late PCIe DMA generation=... current=... count=...
[INFO] discard stale PCIe descriptor generation=... current=...
[ERROR] PCIe DMA did not become idle before start
```
