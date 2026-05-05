# TCMT Windows Client 架构 / 协议 / 共享内存 / 功能规划总文档（全量更新版）
版本：v0.14  
更新时间：2025-10-21  
说明：本文件为"全量"文档。合并 Version9 和 Version7，包含原始章节 1~27 + 扩展 28(含 28.14) / 29 / 30 / 31，并补充所有关键缺失细节（结构字段、位标、哈希规范、刷新间隔、偏移生成、错误分类、并发/性能/版本策略等），并在第 4/8/18 章新增 4.5 / 8.5 / 18.4 说明。当前仅文档，不含任何实现代码；如需代码请发指令："要代码：模块名"。

## 目录
1. 目标与阶段路线总览  
2. 当前模块状态一览  
3. 共享内存结构与扩展策略（含完整结构声明 & 位标 & 哈希规范）  
4. 共享内存全量偏移表（理论 + 生成流程 + 获取指引）  
5. 写入完整性：writeSequence 奇偶 + 报警机制  
6. 指令系统设计与实施步骤  
7. 指令清单（现有 / 预留 / 占位）  
8. 错误与日志规范（主日志 + errors.log + 环形对接 + 统一格式 + JSON 错误处理暂缓）  
9. 主板 / BIOS 信息采集规划  
10. 进程枚举（第一版无 CPU%）  
11. USB 枚举与去抖（debounce）  
12. 环形趋势缓冲（short/long + snapshotVersion 定义）  
13. 插件系统最小 SDK 约定（占位）  
14. SMART 复检指令占位与异步策略（含正常刷新间隔）  
15. 进程控制与黑名单策略  
16. 配置文件 core.json 草案（新增 agingWindow / agingThresholdScore 等）  
17. 风险与回退策略  
18. 验收与测试步骤总表（含自检分类 + 单元测试 TODO）  
19. 原生温度采集（NVAPI / ADL / WMI / SMART 异步）  
20. 迁移步骤（旧结构→新结构 + 温度过渡）  
21. FAQ / 常见坑提示  
22. 实施批次任务分解（最新排序）  
23. 后续扩展路线图（分析 / 安全 / 跨平台）  
24. 未来跨平台抽象占位（Provider 接口）  
25. 收尾与后续触发点  
26. 附：偏移与结构校验章节（整块 SHA256 + diff 机制）  
27. MCP 服务器构想（订阅过滤 + 限速行为）  
28. 自检与扩展分析（sys.selftest / SMART 评分趋势 / sys.metrics / 性能优化 / LLM 预备 / critical 分类）  
28.14 SMART Aging Curve（老化曲线草图 + 配置化）  
29. 事件回放模拟（命名/保留策略）  
30. 硬件扩展补充（字段与无效值约定）  
31. CLI 设计与实施（含 degradeMode 行为 & DATA_NOT_READY 格式）  
32. 单位与精度规范  
33. 并发与线程安全模型  
34. 版本与兼容策略（abiVersion 映射）  
35. 历史与保留策略（快照/事件/Aging 持久化）  
36. 安全与权限初步（角色/RBAC/哈希链占位）  
37. 构建与工具脚本命名（偏移/自检/打包）

---

## 1. 目标与阶段路线总览
| 阶段 | 描述 | 状态 | 备注 |
|------|------|------|------|
| Phase 1 | 基础硬件采集 + 共享内存稳定 | 进行中 | 扩展与奇偶序列即将实现 |
| Phase 2 | 原生温度迁移 + SMART 异步 | 规划完成 | 温度优先，SMART 异步稍后 |
| Phase 3 | 前端日志环形缓冲落地 | 规划完成 | 诊断基础 |
| Phase 3a | 偏移 JSON 自动生成 + 哈希校验 | 前移 | 与环形并行 |
| Phase 4 | 指令系统最小集 | 待实现 | 提供基础操作入口 |
| Phase 5 | SMART 异步刷新 | 待实现 | 评分前置条件 |
| Phase 6 | 自检 sys.selftest | 待实现 | 启动可信度 |
| Phase 7 | 温度事件合并 + urgent | 待实现 | 用户体验 |
| Phase 8 | SMART 寿命评分 | 待实现 | 健康分析核心 |
| Phase 8.1 | SMART Aging Curve | 规划 | 预测剩余安全天数 |
| Phase 9 | sys.metrics 聚合资源 | 待实现 | 指标汇总 |
| Phase 10 | 自适应刷新 | 待实现 | 动态刷新间隔 |
| Phase 11 | LLM 接入预备 | 低优先级 | 描述 + 最小化视图 |
| Phase 12 | MCP MVP | 很低优先级 | 外部协议占位 |
| Phase 13 | Plugin Tick 性能统计 | 最低 | 插件后置 |
| Phase 14 | Event Replay 基础 | 规划 | 历史重建 |
| Phase 15 | Aging Curve 深化 | 规划 | 平滑与多阈值 |
| Phase 16 | Extended Hardware Resources | 规划 | nvme/memory/cpu_features |

---

## 2. 当前模块状态一览
| 模块 | 现状 | 问题 | 下一步 |
|------|------|------|--------|
| SharedMemory | 旧结构 | 无扩展字段 | 扩展 + writeSequence |
| writeSequence | 未实现 | 脏读风险 | 奇偶协议接入 |
| 温度 | 托管桥 | 精度性能不足 | 原生迁移 |
| SMART 异步 | 未实现 | 无健康评分数据 | 异步线程 |
| 主板/BIOS | 未采集 | 字段空 | WMI/SMBIOS 填充 |
| 进程枚举 | 未实现 | 无进程列表 | 基础枚举 |
| USB | 未实现 | 插拔日志风暴 | 去抖策略 |
| 趋势缓冲 | 未实现 | 无曲线数据 | short/long 环形 |
| 指令系统 | 未实现 | 无外部调用入口 | 最小集落地 |
| 环形日志缓冲 | 规划完成 | 无具体结构 | 编码实现 |
| 偏移校验 | 手工核对 | 易错难维护 | 自动 JSON + debug 工具 |
| SMART 评分 | 未实现 | 无健康指标 | 规则+趋势 |
| 温度事件合并 | 未实现 | 刷屏风险 | 合并 + urgent 机制 |
| sys.metrics | 未实现 | 无统一指标输出 | 聚合刷新 |
| 自适应刷新 | 未实现 | 固定间隔不优 | 负载阈值动态调整 |
| MCP | 构想 | 未接入 | 后置 |
| 插件 SDK | 占位 | 无执行 | 后置 |
| Aging Curve | 未实现 | 无趋势预测 | 评分之后 |
| Event Replay | 未实现 | 无历史回放 | 快照与事件文件 |
| Extended Hardware | 未实现 | 诊断不够细 | 扩展资源采集 |

---

## 3. 共享内存结构与扩展策略
### 3.1 结构演进准则
- 仅尾部追加，不在中间插入；移除字段需保留占位长度或 bump abiVersion + 标记弃用。
- 所有字段 POD；禁止动态分配、指针、std::string。
- 对齐：`#pragma pack(push,1)`。

### 3.2 SharedMemoryBlock 完整声明（示意，最终以头文件为准）
```
struct TemperatureSensor {
  char name[32];          // UTF-8，不足填0
  int16_t valueC_x10;     // 温度*10 (0.1°C)，不可用 -1
  uint8_t flags;          // bit0=valid, bit1=urgentLast
};

struct SmartDiskScore {
  char diskId[32];
  int16_t score;          // 0-100，-1 不可用
  int32_t hoursOn;
  int16_t wearPercent;    // 0-100，-1 不可用
  uint16_t reallocated;
  uint16_t pending;
  uint16_t uncorrectable;
  int16_t temperatureC;   // -1 不可用
  uint8_t recentGrowthFlags; // bit0=reallocated增, bit1=wear突增
};

struct SharedMemoryBlock {
  uint32_t abiVersion;          // 0x00010014
  uint32_t writeSequence;       // 奇偶协议：启动0
  uint32_t snapshotVersion;     // 完整刷新+1
  uint32_t reservedHeader;      // 对齐

  uint16_t cpuLogicalCores;
  int16_t  cpuUsagePercent_x10; // 未实现 -1
  uint64_t memoryTotalMB;
  uint64_t memoryUsedMB;

  TemperatureSensor tempSensors[32];
  uint16_t tempSensorCount;

  SmartDiskScore smartDisks[16];
  uint8_t smartDiskCount;

  char baseboardManufacturer[128];
  char baseboardProduct[64];
  char baseboardVersion[64];
  char baseboardSerial[64];
  char biosVendor[64];
  char biosVersion[64];
  char biosDate[32];
  uint8_t secureBootEnabled;
  uint8_t tpmPresent;
  uint16_t memorySlotsTotal;
  uint16_t memorySlotsUsed;

  uint8_t futureReserved[64]; // bit0=degradeMode bit1=hashMismatch bit2=sequenceStallWarn
  uint8_t sharedmemHash[32];  // SHA256(结构除自身)

  uint8_t extensionPad[128];  // 预留
};
```
说明：最终以编译生成 offsets JSON 为准。

### 3.3 futureReserved 位标定义
| 位 | 名称 | 含义 | 触发条件 |
|----|------|------|----------|
| bit0 | degradeMode | 结构/哈希/版本不匹配降级 | size/hash/version mismatch |
| bit1 | hashMismatch | sha256 与偏移 JSON 不符（abiVersion 相同） | 校验发现差异 |
| bit2 | sequenceStallWarn | 连续奇数序列≥阈值 | 写线程阻塞 |
| bit3-7 | 保留 | 未来扩展 | - |

### 3.4 writeSequence 行为与溢出
- uint32 递增，溢出→0（偶数稳定）。
- 奇偶判断：奇数=写进行中；偶数=稳定。
- 初始 0；写开始 seq=1；写结束 seq=2。

### 3.5 sharedmemSha256 计算规范
- 范围：结构全部字节，先将 sharedmemHash 置 0，再计算。
- 算法：SHA256，无盐（v0.14）；v0.15 计划增加 salt。
- 生成：启动后一次；偏移 JSON 写入。
- CLI 可重新计算（后续可加缓存）。

### 3.6 snapshotVersion 定义
- 每次完整硬件刷新成功 +1（CPU/温度/SMART 至少一项更新）。
- 连续相同 snapshotVersion ≥5：趋势缓冲暂停追加。

### 3.7 并发与访问
- 单写多读；写线程标记奇数→写→hash→偶数。
- 自检遇奇数使用上次稳定快照。

### 3.8 结构升级流程
1. 尾部追加字段。
2. 更新 abiVersion。
3. 生成新的 offsets JSON + hash。
4. 更新文档。
5. 校验通过后发布。

---

## 4. 共享内存全量偏移表
### 4.1 生成脚本
- 名称：generate_offsets.exe
- 输出：docs/runtime/offsets_sharedmem_v0_14.json
- 字段：name/offset/size/flags

### 4.2 示例
```
{
  "abiVersion":"0x00010014",
  "sizeof":125818,
  "generatedTs":1697890000123,
  "sharedmemSha256":"<64hex>",
  "fields":[
    {"name":"abiVersion","offset":0,"size":4},
    {"name":"writeSequence","offset":4,"size":4},
    {"name":"snapshotVersion","offset":8,"size":4},
    ...
  ]
}
```

### 4.3 校验优先级
version mismatch > size mismatch > hash mismatch > field offset mismatch

### 4.4 差异处理策略
- version mismatch：置 bit0（degradeMode）
- size/hash mismatch：置 bit0 & bit1
- 字段偏移差异：WARN + differences 列表

### 4.5 Offset Table Retrieval Instructions
运行 generate_offsets.exe 获取最新偏移表并写入 docs/runtime/offsets_sharedmem_v0_14.json。禁止手工硬编码偏移值；任何实现若未通过该工具生成的偏移文件即视为不可信。测试阶段先实现工具，后引入 CLI 校验。

---

## 5. 写入完整性：writeSequence 奇偶 + 报警机制
| 阶段 | seq | 描述 |
|------|-----|------|
| 写开始 | seq 奇数 | 标记写进行中 |
| 写结束 | seq 偶数 | 写完成可读 |
| 读端看到奇数 | 使用旧快照 | 防止撕裂 |
| 连续奇数 ≥N 次 | WARN | 写线程阻塞 |
默认 N=5；后续可升级双缓冲或版本原子号。

---

## 6. 指令系统设计与实施步骤
请求 JSON：`{ "id":"req-1","type":"refresh.hardware","args":{} }`  
响应 JSON：`{ "id":"req-1","type":"refresh.hardware","status":"ok","data":{...} }`  
错误：`{ "id":"req-1","status":"error","error":{"code":"NOT_SUPPORTED","message":"..."} }`  
初版不做鉴权；后续与 MCP 角色权限整合。

---

## 7. 指令清单（现有 / 预留 / 占位）
| 指令 | 状态 | 说明 |
|------|------|------|
| system.ping | 计划 | 健康检测 |
| refresh.hardware | 计划 | 强制刷新快照 |
| disk.smart.scan | 占位 | SMART 复检触发 |
| baseboard.basic | 占位 | 主板信息返回 |
| log.config.update | 预留 | 动态限流级别 |
| process.list | 预留 | 进程枚举 |
| process.kill | 预留 | 高危操作（后期） |
| usb.rescan | 预留 | USB 重新枚举 |
| stats.export | 预留 | 趋势导出 |
| mcp.describe | 预留 | MCP 元信息 |
| plugin.list | 预留 | 插件枚举 |

---

## 8. 错误与日志规范
主日志：DEBUG/INFO/WARN/ERROR 全量。  
errors.log：仅 WARN/ERROR。  
统一格式：`YYYY-MM-DD HH:MM:SS.mmm LEVEL message key=value ...`  
环形日志对接：  
- 超限：标记 `[RATE_LIMITED]`  
- 覆盖：`WARN frontend_log_overrun lost=<count>`  
- 配置更新：`INFO backend_log_config_update ...`  
- 哈希失败：`ERROR frontend_log_hash_mismatch seq=<seq>`  

### 8.5 JSON Error Handling Pending
JSON 错误处理（标准化 error.code/message/hint/detail）暂缓，等待 CPP-parsers 增加错误 API（has()/lastError()/typed get）。当前仅在指令响应中使用简单 error 对象；复杂场景后续补。

---

## 9. 主板 / BIOS 信息采集规划
字段：manufacturer / product / version / serial（可遮蔽） / biosVendor / biosVersion / biosDate / formFactor / memorySlotsTotal / memorySlotsUsed / secureBootEnabled / tpmPresent / virtualizationFlags。  
策略：启动异步一次性采集 → 更新 lastBaseboardRefreshTs。

---

## 10. 进程枚举（第一版无 CPU%）
周期：1.5~2 秒。  
字段：PID / 名称 / WorkingSetMB / blacklistFlag。  
不写入共享内存，使用后台缓存。  
CPU% 后期通过两次时间差值计算。

---

## 11. USB 枚举与去抖
监听设备变化 → 设置 pending → debounceMs=500 延迟实际枚举。  
字段：deviceId / friendlyName / vendorId / productId / class / lastChangeTs。  
减少重复插拔日志风暴。

---

## 12. 环形趋势缓冲
指标初版：CPUUsage%、MemoryUsage%、GpuTemperature。  
short 环形：60 点；long 环形：300 点。  
暂停：snapshotVersion 未变 ≥5 次停止追加。  
未来：磁盘 IO、网络吞吐扩展。

---

## 13. 插件系统最小 SDK 约定（占位）
函数：Plugin_GetInfo / Plugin_Initialize / Plugin_Tick / Plugin_Shutdown / Plugin_HandleMessage。  
仅枚举目录 ./plugins，不执行。  
性能统计（p95/avg）后置实现。

---

## 14. SMART 复检指令占位与异步策略
SMART 异步线程：队列、重试 schedule=[1,5,10]、失败日志。  
disk.smart.scan：触发全盘或单盘复检（初版占位返回）。  

- 全属性刷新：smart.refreshIntervalSeconds=30
- 温度轻量刷新：smart.tempIntervalSeconds=10
- 重试 schedule：[1,5,10]；超过 maxSmartRetries 标记 stale

---

## 15. 进程控制与黑名单策略
黑名单列表（配置）：System / Registry / WinInit / smss.exe / csrss.exe / explorer.exe。  
UI 禁用 kill；后期高危操作需 ADMIN + 二次确认。

---

## 16. 配置文件 core.json 草案
```jsonc
{
  "refresh": { "cpuMs":1000, "gpuMs":2000, "memoryMs":1000, "diskMs":3000 },
  "sequence": { "maxConsecutiveOdd":5, "alarmLog":true },
  "usb": { "debounceMs":500, "smartAdjust":true },
  "privacy": { "maskBaseboardSerial":false },
  "process": { "blacklist":["System","Registry","WinInit","smss.exe","csrss.exe","explorer.exe"] },
  "plugin": { "directory":"./plugins", "tickTimeoutMs":50 },
  "trend": { "shortPoints":60, "longPoints":300, "pauseIfSnapshotFrozen":5 },
  "logging": {
    "rotateSizeMB":5,
    "errorsDatePrefix":true,
    "frontend": { "rateLimitDefault":20, "maxOverride":200 }
  },
  "temperature": {
    "enableNativeTemperature": true,
    "useSmartStorageQueryFirst": true,
    "logDiffOnceBeforeRemoval": true,
    "unsupportedValueRender": "N/A",
    "asyncSmartRetry": true,
    "smartRetryScheduleSeconds": [1,5,10],
    "maxSmartRetries": 3,
    "collectAllAmdGpus": true,
    "event": {
      "mergeEnabled": true,
      "urgentJumpThresholdC": 5
    }
  },
  "smart": {
    "trendWindow": 5,
    "wearRapidGrowthPercent": 1,
    "refreshIntervalSeconds": 30,
    "tempIntervalSeconds": 10,
    "agingWindow": 10,
    "agingThresholdScore": 50
  },
  "adaptiveRefresh": {
    "enabled": true,
    "cpuHighThresholdPercent": 15,
    "cpuRecoverThresholdPercent": 10,
    "consecutiveCount": 5
  },
  "mcp": {
    "apiVersion": "mcp-0.14",
    "temperatureChangeThreshold": 2,
    "temperatureBroadcastIntervalMs": 1000,
    "requestRateLimitPerSec": 30
  }
}
```

---

## 17. 风险与回退策略
| 风险 | 征兆 | 回退 |
|------|------|------|
| 偏移错误 | UI 数据乱码 | static_assert + offsets diff |
| 序列卡奇数 | 连续奇数报警 | 重试写 / 双缓冲 |
| 温度全 N/A | 温度视图空 | 关闭原生温度采集 |
| SMART 阻塞 | score 不更新 | 超时重启线程 |
| 日志覆盖频繁 | overrunCount 激增 | 扩容或限流调整 |
| 哈希失败频繁 | hashOk=0 重复 | 检查写入顺序 / 盐 |
| 评分跳动 | 分数忽高忽低 | 延长趋势窗口 |
| 自适应抖动 | 间隔频繁变化 | 提高 consecutiveCount |
| 结构哈希不匹配 | WARN hash mismatch | degradeMode |
| Aging 预测异常 | remainingSafeDays 巨变 | confidence=low 屏蔽提示 |

---

## 18. 验收与测试步骤总表
| 项目 | 条件 | 标准 |
|------|------|------|
| sizeof 校验 | 编译后 | ==125818 |
| 序列奇偶 | 模拟阻塞写 | 第5次奇数 WARN |
| 温度迁移差值 | 过渡首轮 | 仅一条 temp_diff |
| SMART 异步重试 | 故障盘 | 3 次重试日志 |
| 环形日志限流 | >20 条/s | [RATE_LIMITED] 标记 |
| 覆盖计数 | 压力测试 | WARN frontend_log_overrun |
| 主板字段 | 采集完成 | 字段非空 |
| USB 去抖 | 快速插拔 | 重复减少 |
| 趋势缓冲 | ≥5 分钟 | long=300 点 |
| 偏移 JSON | 启动 | 文件含 sharedmemSha256 |
| debug 程序 | 执行 | 退出码=0 |
| 自检 | 启动 | urgentJumpThresholdC 出现 |
| SMART 评分 | 有盘 | score + advice |
| 温度合并 | 多变化1秒内 | 单条 batch 事件 |
| urgent 温度 | 单跳≥5°C | 即刻 urgent=true |
| 自适应刷新 | 模拟负载 | 间隔日志变化 |
| sys.metrics | 请求 | pending 标记 |
| Aging Curve | ≥2 点 | slope 计算 |
| Event Replay | 人工文件 | quality 合理 |
| Extended 资源 | 有设备 | 返回数据或空数组+message |

### 18.1 Critical Checks
(sharedmem_size / offsets / hash / write_sequence_flip / nvapi_init / adl_init)

### 18.2 Non-Critical Checks
(smart 单盘失败 / baseboard 缺失 / trend 缺数据 / 单传感器不可用)

### 18.3 状态决策
(≥1 critical fail → fail；仅 non-critical fail → partial_warn；全通过 → ok)

### 18.4 Unit Test Plan (TODO)
- [ ] 列出需要测试的模块（sharedmem、hash、sequence、SMART refresh、温度合并）
- [ ] 编写最小断言场景（结构 size、hash 重复计算一致性）
- [ ] 添加失败路径测试（写线程阻塞 / SMART 重试）
- [ ] 以后扩展：fuzz 指令解析 / CLI 参数解析
单元测试代码后续单独实现，不在本文中。

---

## 19. 原生温度采集
范围：CPU(WMI)、GPU(NVAPI/ADL)、磁盘(SMART)。  
过渡：托管桥一次对比 → temp_diff 日志1条 → 移除桥。  
SMART 温度：异步 schedule 重试。  
无效值：-1 显示 N/A。

---

## 20. 迁移步骤
1. 新结构追加字段  
2. 移除 CRITICAL_SECTION  
3. writeSequence 奇偶协议  
4. 主板信息采集  
5. 原生温度加载 + diff  
6. 移除托管桥  
7. 环形日志缓冲  
8. 偏移 JSON + debug 程序  
9. 指令系统最小集  
10. SMART 异步线程  
11. 自检 sys.selftest  
12. 温度事件合并 + urgent  
13. SMART 寿命评分  
14. Aging Curve  
15. sys.metrics  
16. 自适应刷新  
17. LLM 接入预备  
18. MCP MVP  
19. Plugin Tick 统计  
20. Event Replay  
21. Extended Hardware 资源

---

## 21. FAQ / 常见坑提示
| 问题 | 原因 | 解决 |
|------|------|------|
| 温度大量 N/A | 驱动缺失/权限 | 安装驱动或关闭原生温度 |
| 序列长时间奇数 | 写线程卡死 | 重启写线程 / 加双缓冲 |
| SMART 温度异常 | 解析偏移错 | 校正字段索引 |
| 日志覆盖多 | 入口过量 | 调整限流或容量 |
| 哈希频繁失败 | 初始化乱序 | 统一盐和写顺序 |
| 分数突然下降 | 属性激增 | 检查 recentGrowth |
| 自适应频繁跳 | 阈值太紧 | 调整 consecutiveCount |
| Aging 预测不可信 | 点少/波动大 | confidence=low 不提醒 |
| Replay 缺数据 | 快照未写 | 启用历史定时任务 |
| Extended 空 | 无设备 | message 说明不可用 |

---

## 22. 实施批次任务分解（最新排序）
| 顺序 | 任务 | 描述 | 估时 | 依赖 | 可暂缓 | 备注 |
|------|------|------|------|------|--------|------|
| 1 | SharedMemory 扩展 + writeSequence | 基础结构与协议 | 1h | 无 | 否 | 基石 |
| 2 | 原生温度迁移 | 核心温度采集 | 1.5h | 1 | 否 | 用户可见 |
| 3 | 环形日志缓冲 | 日志结构 | 2h | 1 | 否 | 诊断基础 |
| 3a | 偏移 JSON + SHA256 + debug | 自动校验 | 1h | 1 | 否 | 尽早发现错误 |
| 4 | 指令系统最小集 | 操作入口 | 1h | 1 | 否 | 后续工具依赖 |
| 5 | SMART 异步线程 | 属性刷新 | 1.5h | 1 | 否 | 评分前置 |
| 6 | 自检 sys.selftest | 启动健康报告 | 1h | 3,3a,4,5 | 否 | 可信度 |
| 7 | 温度事件合并 + urgent | 合并与跳变推送 | 0.8h | 2 | 否 | 体验提升 |
| 8 | SMART 寿命评分 | 规则+趋势 | 1h | 5 | 否 | 健康分析 |
| 8.1 | SMART Aging Curve | 斜率预测 | 0.8h | 8 | 是 | 可后移 |
| 9 | sys.metrics | 指标聚合 | 0.8h | 2,3,5,8 | 是 | 后置 |
| 10 | 自适应刷新 | 动态间隔 | 1h | 9 或 CPU% | 是 | CPU% 未完成可暂缓 |
| 11 | LLM 接入预备 | 描述/最小化 | 0.5h | 9 | 是 | 文档占位 |
| 12 | MCP MVP | list/get/sub | 2h | 3,4,6,7,8 | 是 | 后置 |
| 13 | Plugin Tick 统计 | p95/avg | 1h | 插件框架 | 是 | 最低 |
| 14 | Event Replay 基础 | 快照+重建占位 | 1h | 6,8 | 是 | 历史分析 |
| 15 | Aging Curve 深化 | 多阈值/平滑 | 1h | 8.1 | 是 | 优化 |
| 16 | Extended Hardware 资源 | nvme/memory/cpu_features | 2h | 1,5 | 是 | 诊断增强 |

---

## 23. 后续扩展路线图
| 类别 | 扩展点 | 描述 |
|------|--------|------|
| 性能 | 双缓冲/差分写入 | 降低撕裂 |
| 分析 | stats.export/告警引擎 | 智能预警 |
| 安全 | RBAC/审计链 | 权限细粒度控制 |
| 插件 | 沙箱/WASM | 隔离与扩展 |
| 硬件 | 风扇/功耗/网络 | 更全面指标 |
| 跨平台 | Linux/macOS Provider | 统一抽象层 |
| AI | context bundle/minimal | 降 token 消耗 |
| 历史 | 快照归档/回放 | 故障复盘 |
| 签名 | 日志/结构签名 | 防篡改审计 |

---

## 24. 未来跨平台抽象占位
接口示意：
```
IHardwareProvider {
  bool Init();
  Snapshot GetSnapshot();
  TemperatureVector GetTemperatures();
  bool Refresh();
  ProviderInfo GetInfo();
}
```
Windows 实现 → 后续 Linux / macOS。  
用 capability flags 表示支持差异（如是否支持 GPU 温度、SMART）。  

---

## 25. 收尾与后续触发点
实施阶段变更 → 先改文档再写代码。  
阶段完成写里程碑日志：`INFO milestone_reached phase=<n>`。  
进入编码需明确文字指令："要代码：<模块>"。  

---

## 26. 附：偏移与结构校验章节
### 26.1 编译期校验
`static_assert(sizeof(SharedMemoryBlock)==125818,"Size mismatch")`  

### 26.2 运行期日志
`INFO sharedmem_size=<sizeof>`  
`INFO offset writeSequence=<off> baseboardManufacturer=<off>`  

### 26.3 offsets JSON 结构
```
{
  "abiVersion":"0x00010014",
  "sizeof":125818,
  "generatedTs":1697825000123,
  "sharedmemSha256":"<64hex>",
  "fields":[
    {"name":"writeSequence","offset":124792,"size":4},
    {"name":"baseboardManufacturer","offset":124836,"size":128}
  ]
}
```

### 26.4 debug 程序功能
1. 校验尺寸 & 偏移  
2. 差值展示  
3. 生成 diff JSON  
4. 写序列翻转测试  
5. 退出码：0/1/2/3/4  

### 26.5 降级模式 degradeMode
条件：尺寸或哈希不匹配 → futureReserved bit 标记 → UI/MCP 仅读基础字段。  

### 26.6 哈希差异处理
abiVersion 相同但哈希不同：`WARN sharedmem_hash_mismatch` → 自检 fail。  
abiVersion 不同且偏移文件未更新 → degradeMode + 提示升级。

### 26.7 常见偏移错误
未 pack(1) / bool 对齐差异 / 引入非 POD 类型 / 编译器差异（MSVC vs Clang）。  

---

## 27. MCP 服务器构想
资源 + 工具 + 事件统一对接模型或外部 UI。  
传输：stdio 行 JSON → 未来 WebSocket。  
资源集合：sys.cpu / sys.memory / sys.gpus / sys.disks.logical / sys.disks.physical / sys.temperatures / sys.baseboard / sys.processes / sys.usb / sys.trend.short / sys.trend.long / sys.log.config / sys.log.latest / sys.health。  
事件：logs.frontend / temperature.change（batch + urgent）。  
错误码：BAD_ARGUMENT / NOT_FOUND / PERMISSION_DENIED / RATE_LIMIT / INTERNAL_ERROR / NOT_SUPPORTED / TIMEOUT / CONFLICT。  
限速：30 req/s。  
LLM 预备：description + minimal + bundle + hint。  

---

## 28. 自检与扩展分析
### 28.1 目标
启动时生成 sys.selftest 验证核心子系统可靠性。  

### 28.2 自检项目
sharedmem_size / sharedmem_offsets / write_sequence_flip / log_ring_hash / nvapi_init / adl_init / smart_sample / baseboard_fields / trend_buffer_basic / urgentJumpThresholdC。  

### 28.3 输出示例
```
{
  "status":"partial_warn",
  "generatedTs":1697826000456,
  "urgentJumpThresholdC":5,
  "checks":[ ... ],
  "warnCount":1
}
```

### 28.4 SMART 寿命评分
规则扣分 + 趋势扣分（trendWindow=5）。  
NVMe wearPercent 获取失败 → 不扣。  

### 28.5 示例
```
{
  "diskId":"PhysicalDisk0",
  "score":74,
  "riskLevel":"medium",
  ...
}
```

### 28.6 sys.metrics
聚合指标（含 pending 字段）。  

### 28.7 性能优化
自适应刷新 & 温度事件合并 + urgent。  

### 28.8 温度事件结构
批量与 urgent 两种 JSON 格式。  

### 28.9 偏移 diff 与自检关系
哈希不匹配 → fail + degradeMode。  

### 28.10 Plugin Tick 性能统计
统计 p95/avg/max 等，占位。  

### 28.11 LLM 接入预备
description / bundle / minimal / hint / 白名单。  

### 28.12 扩展验收点
覆盖哈希、自检 urgent、趋势递增、urgent 温度、sys.metrics pending 等。  

### 28.13 顺序确认
扩展顺序已含 14~16 新阶段。  

### 28.14 SMART Aging Curve（老化曲线草图）
- 采样点：`{ts,hoursOn,score}`；条件：hoursOn 增或 score 变化幅度>1。  
- 缓存：最近 50 条。  
- trendWindow=10；斜率：`(score_last - score_first)/(hoursOn_last - hoursOn_first)`。  
- slope<0 时预测达到 score=50 的剩余小时 → remainingSafeDays。  
- 置信度：点少 / |slope| 很小 / 标准差大 → low。  
- high/medium/low 置信度规则 + agingWindow/agingThresholdScore

---

## 29. 事件回放模拟
快照文件 + 事件文件重建指定窗口状态。  
工具占位：tool:replay.build（mode=point/window）。  
质量等级：full / partial / degraded。  
初版不插值，仅拼合。  
文件命名与保留策略：  
- 快照：snapshot_YYYYMMDD_HHMMSS.json  
- 事件：events_YYYYMMDD_HHMMSS.jsonl  
- 保留：快照7天，事件30天  

---

## 30. 硬件扩展补充
新增资源：  
- sys.nvme.ext：队列深度、使用率、错误计数  
- sys.memory.channels：通道/频率/XMP/模块列表  
- sys.cpu.features：指令集、虚拟化、HT  
- sys.gpu.driver（后置）：驱动版本与推荐版本差异  
- sys.net.adapters（后置）：速率、流量、地址  
占位策略：无设备 → 空数组 + message；未知 → -1 或 null。  
无效值约定：温度=-1，百分比=-1，计数=0xFFFFFFFF，字符串空。

---

## 31. CLI 设计与实施（含 degradeMode 行为 & DATA_NOT_READY 格式）

### 31.1 目标
提供命令行工具 `tcm`：快速查看系统状态、偏移校验、刷新、SMART 与 Aging 相关信息；后续支持事件回放与硬件扩展显示。  

### 31.2 范围与非目标
- 第一批：sys-status / offsets verify / temps / refresh（可占位） / smart / smart-aging（数据不足返回码 6）。  
- 不含：交互式 REPL、复杂历史回放、日志注入（后续扩展）。

### 31.3 目录规划（tcm/）
```
tcm/
  README.md
  include/
    command_table.h
    exit_codes.h
    parsing.h
    shared_memory_reader.h
    offsets_verifier.h
    json_writer.h
  src/
    main.cpp
    command_table.cpp
    cmd_sys_status.cpp
    cmd_offsets_verify.cpp
    cmd_temps.cpp
    cmd_refresh.cpp
    cmd_smart.cpp
    cmd_smart_aging.cpp
  scripts/
    validate_offsets.ps1 (可选)
  TCM_CLI.vcxproj
  TCM_CLI.sln
```
偏移 JSON 默认读取：`docs/runtime/offsets_sharedmem_v0_14.json`（支持 `--file`）。

### 31.4 共享内存访问
默认名：`Global\TCMT_SharedMemory`  
可通过环境变量覆盖：`TCM_SHM_NAME`。  
流程：打开 → 校验尺寸 → 读 writeSequence → degradeMode 检测（若 true，sys-status 退出码=4）。  

### 31.5 命令及退出码
| 命令 | 描述 | 退出码特殊 |
|------|------|-----------|
| sys-status | 汇总 CPU/GPU/内存/温度 | degradeMode=4 |
| offsets verify | 校验尺寸/哈希/偏移 | 差异=5 文件缺失=2 |
| temps | 所有温度传感器 | degradeMode仍输出 |
| refresh | 调用 refresh.hardware | 失败=3 |
| smart --disk | SMART 评分 | 数据不足=6 |
| smart-aging --disk | Aging 曲线预测 | 数据不足=6 |

退出码集合：  
0 成功 / 1 参数错误 / 2 资源不可用 / 3 指令执行失败 / 4 degradeMode / 5 偏移校验失败 / 6 数据不足。

### 31.6 参数格式
支持 `--key value` 与 `--key=value`。  
通用参数：`--json` `--pretty` `--quiet` `--help` `--file <path>`（偏移 JSON）。  

### 31.7 输出规范
- sys-status：CPU 未实现显示 `(pending)` 而不是 -1。  
- JSON：紧凑；`--pretty` 启用缩进。  
- smart-aging 不足点时：`exitCode=6` + 文本 "Aging Curve 数据不足"。  

### 31.8 偏移校验流程
1. 读取指定或默认 offsets JSON。  
2. 计算实际 sizeof 与整块 SHA256。  
3. 对比字段偏移（使用编译期 offsetof 常量数组）。  
4. 任一不符 → 列出差异 + exitCode=5。  
5. 文件不存在 → exitCode=2。  

### 31.9 SMART / Aging 占位策略
- SMART 异步未完成：smart 命令 exitCode=6（DATA_NOT_READY）。  
- Aging 数据点 <2：smart-aging exitCode=6。  

### 31.10 JSON 库
采用 nlohmann/json（单头文件）。  
若后续性能要求提升，可考虑 RapidJSON，但初版不抽象层。  

### 31.11 环境变量
| 变量 | 描述 | 默认 |
|------|------|------|
| TCM_SHM_NAME | 共享内存名 | Global\TCMT_SharedMemory |
| TCM_MODE | local 或 mcp 模式占位 | local |
| TCM_OFFSETS_PATH | 覆盖偏移 JSON 路径 | 未设置 |

优先级：命令行 --file > TCM_OFFSETS_PATH > 默认路径。

### 31.12 验收点（CLI）
| 项目 | 条件 | 标准 |
|------|------|------|
| sys-status degrade | 模拟 degradeMode | 退出码=4 |
| offsets verify 缺文件 | 移除 JSON | exitCode=2 |
| offsets verify 差异 | 手工改偏移 | exitCode=5 |
| SMART 未就绪 | 无 SMART 数据 | exitCode=6 |
| Aging 点不足 | <2 点 | exitCode=6 |
| CPU 未实现 | 未采集 CPU% | 文本显示 (pending) |
| --json 输出 | 各命令带 --json | JSON 格式合法 |
| --pretty | 任一命令 | JSON 缩进 |
| 环境变量覆盖 | 设置 TCM_SHM_NAME | 使用新名成功 |
| refresh 指令失败 | 模拟错误 | exitCode=3 |

### 31.13 阶段划分（CLI 专属）
| 阶段 | 内容 | 备注 |
|------|------|------|
| CLI-A | sys-status + offsets verify | 基础读取与校验 |
| CLI-B | temps + refresh | 温度与刷新入口 |
| CLI-C | smart + smart-aging 占位 | 等 SMART 异步完成 |
| CLI-D | 日志注入 (可选) | 环形日志测试 |
| CLI-E | MCP 模式切换 | 待 MCP MVP |
| CLI-F | replay / hw 扩展命令 | 高级功能 |

### 31.14 与主程序协同
- 偏移 JSON 必须在主程序启动时生成。  
- CLI 的字段偏移数组与主程序结构保持同步（由公共头文件导出）。  
- 结构升级时先更新偏移 JSON 和头文件，再编译 CLI。  

### 31.15 失败与降级策略
- 共享内存不可用：退出码=2。  
- degradeMode：sys-status 退出码=4 但仍输出。  
- JSON 输出失败（极少见）：fallback 文本 + exitCode=3。  

### 31.16 未来扩展占位
命令：`tcm replay` / `tcm hw nvme` / `tcm hw memory` / `tcm hw cpu-features` / `tcm log --level INFO "..."` / `tcm trend export` / `tcm diag dump`。

### 31.17 集成 CPP-parsers 附录（使用统一解析库）

#### 31.17.1 目标
将你的仓库 [CPP-parsers] 中统一解析接口用于读取配置（如 core.json），并保留 offsets JSON 使用更复杂结构解析（nlohmann/json 直接或原生）。达到：多格式配置支持（JSON/TOML/YAML/XML/INI）无需修改 CLI 调用逻辑。

#### 31.17.2 为什么两条路径并存
| 文件类型 | 解析方式 | 原因 |
|---------|---------|------|
| core.(json|yaml|toml|ini|xml) | IConfigParser + createParser | 只需简单键值存取 |
| offsets_sharedmem_v0_14.json | 直接 nlohmann/json | 需要数组与偏移字段精确遍历 |
| 复杂结构资源导出 | nlohmann/json | 涉及对象/数组/嵌套 |
| 简单 kv 配置文件 | IConfigParser | 跨格式统一接口 |

#### 31.17.3 引入步骤（VS 2022）
1. 添加子模块（若未克隆）：  
   - `git submodule add https://github.com/dongge0210/CPP-parsers extern/CPP-parsers`  
   - 或在外部已克隆情况下直接引用其 include/src。  
2. 在解决方案中添加现有解析库头路径：  
   - 项目属性 → C/C++ → 常规 → 附加包含目录：`extern/CPP-parsers/include` `extern/CPP-parsers/src`（视实际结构）。  
3. 仅使用 JSON 时可裁剪：删除（或不编译）YAML/XML/TOML/INI 的解析实现文件，保留 JsonConfigParser。  
4. 在需要读取 core.json 的源文件中：  
   - 包含 `#include "ConfigParserFactory.h"`（它内部会包含各具体解析器头）。  
5. 调用流程（伪逻辑，不放真实代码）：  
   - `parser = createParser("core.json")` → 判空  
   - `parser->load("core.json")` → 失败则 fallback 默认配置  
   - `parser->get("refresh.cpuMs")` → 转换为整数（需要你在调用侧手动 `stoi`）  
6. 写回：  
   - 修改参数 → `parser->set("refresh.cpuMs","1200")` → `parser->save("core.json")`  
   - 注意：不保证保留原注释或格式（尤其 YAML/TOML）。  
7. 错误处理增强（建议）：  
   - 扩展接口：增加 `getOrDefault(key, defaultValue)`（在你仓库中实现后再使用）  
   - 增加 `bool has(key)`，减少 get 空字符串歧义。  

#### 31.17.4 引入步骤（VS Code + CMake 可选）
（如果你后来需要 VS Code 构建）  
1. 在顶层 CMakeLists.txt 增加：  
   - `add_subdirectory(extern/CPP-parsers)`（如果该仓库自含 CMake）  
   - 或手工 `target_include_directories(tcm PRIVATE extern/CPP-parsers/include)`  
2. 把 CLI 可执行 target 与解析库实现文件一起编译；若只要 JSON，添加 `JsonConfigParser.cpp` 与 `IConfigParser.h` 等文件。  
3. VS Code `c_cpp_properties.json` 中添加 `includePath`：`"${workspaceFolder}/extern/CPP-parsers/include"`。  

#### 31.17.5 限制与注意
| 项 | 说明 |
|----|------|
| 类型单一 | IConfigParser 当前只处理字符串，复杂类型需自己转换 |
| 无错误描述 | load 失败无法返回原因（建议扩展接口） |
| 注释/格式丢失 | save 后可能丢失原文件注释与缩进风格 |
| 线程安全 | 未声明线程安全，跨线程访问需外层锁 |
| 未测试声明 | README 标注"未经过测试"，生产前需补单测 |
| 数组/对象 | offsets JSON 中的 fields 数组无法用简单 get 处理 |

#### 31.17.6 推荐改进（后续）
- 在 CPP-parsers 增加：`virtual std::string getRaw()` 或 `virtual std::map<std::string,std::string> dumpAll()`。  
- 增加多类型接口：`getInt / getBool / getArrayKeys`。  
- 加错误获取：`std::string lastError()`。  
- 支持只读模式（避免无意 save 覆盖）。  

#### 31.17.7 验收点（集成后）
| 项目 | 条件 | 标准 |
|------|------|------|
| JSON 加载正常 | core.json 存在 | load 返回 true |
| 多格式支持 | 改名 core.yaml | createParser 返回非空 |
| 缺失文件 | rename 文件暂时不存在 | load=false → fallback 默认配置 |
| 修改保存 | set 后 save | 文件产生新内容 |
| 错误行为 | 传入不支持扩展 | createParser 返回 nullptr |
| 并发访问 | 两次快速读写 | 不崩溃（人工测试） |

#### 31.17.8 当前决策总结
- CLI 第一阶段仍用 nlohmann/json 解析复杂结构（偏移与趋势等）。  
- 简单键值配置（core.*）可使用 IConfigParser 增强可移植性与格式灵活性。  
- 不对偏移 JSON 强行套 IConfigParser，避免功能不足。  
- 后期视需要完善统一接口并逐步替换直接 JSON 访问。

---

## 32. 单位与精度规范
- 温度：0.1°C 精度 (valueC_x10)
- CPU 使用：*10 未实现为 -1
- 内存：MB（近似 1024*1024 bytes）
- 不可用数值：统一 -1 或空数组
- 时间戳：Unix ms

---

## 33. 并发与线程安全模型
- 单写多读依赖 writeSequence 奇偶
- 自检遇奇数用旧快照
- SMART 异步独立线程
- CLI 只读映射

---

## 34. 版本与兼容策略
- abiVersion 结构变化即递增
- 0xMMMMmmpp 末尾字节可对应文档序号（参考非强制）
- 引入 salt 属结构变化需 bump abiVersion

---

## 35. 历史与保留策略
- 快照每整点一次，保留 7 天
- 事件日志按天存储
- Aging 曲线点暂不持久化（后续可写 disk_age_history.json）

---

## 36. 安全与权限初步
- 角色占位：READ_SYS / CONTROL / ADMIN / HIGH_RISK
- process.kill 需 ADMIN + 二次确认
- 日志哈希链 future prevHash 计划

---

## 37. 构建与工具脚本命名
- generate_offsets.exe
- verify_sharedmem.exe
- selftest_dump.exe
- release_pack.ps1

---

## 更新总摘要
- 合并 Version9 和 Version7 的所有内容
- 新增章节：32-37（单位精度、并发模型、版本策略、历史保留、安全权限、构建工具）
- 保留 Version7 第 31.17 节的 CPP-parsers 集成详细附录
- 确认偏移校验前移、温度事件合并提前于 SMART 评分
- 自检输出包含 urgentJumpThresholdC；偏移 diff 使用整块哈希
- 增加 CLI 退出码与命令规划；集成多格式解析库的附录步骤
- 配置文件合并所有字段，确保完整性

---