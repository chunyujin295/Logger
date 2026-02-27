<div><center><img src="./doc/img/LOGO_CUTED.png" alt="LOGO_CUTED" style="zoom:20%;" /></center></div>
这是一个基于spdlog的日志库，支持通过yaml配置文件进行日志sink输出配置
# 日志使用文档

## 头文件

`logger.h`

## 接口说明

见头文件注释

## 使用示例

```c++
#include <logger>

std::vector<int> testFunction()
{
    Logger::setConfigPath("./myLogConfig.yml");// 可选，手动修改日志配置位置
    // 否则默认读取可执行文件所在路径下默认配置，配置不存在，自动生成
    
	LOG_INFO("testFunction started!");// 打印程序启动信息日志

	std::vector<int> vec(5);
	for (int i = 0; i < vec.size(); ++i)
	{
		vec.at(i) = i * i;
		LOG_DEBUG("myFunction: std::vector<int> vec(" , i, ") = ", i*i);
	}
	try
	{
		vec.at(10) = 42;// 抛出异常 
	}
	catch (const std::out_of_range& e)
	{
		LOG_ERROR(e.what());// 输出越界信息 
		//处理异常
	}
}
```

目前日志支持涵盖基本内建类型，若需要扩充类型，修改`logger_p.cpp/anyToString`方法。

## 日志级别说明及建议使用场景

| 级别         | 说明                                                         | 典型使用场景                          |
| ------------ | ------------------------------------------------------------ | ------------------------------------- |
| **TRACE**    | 最详细的日志信息，用于追踪程序执行路径（如函数调用、循环变量变化） | 深度调试时启用，生产环境通常关闭      |
| **DEBUG**    | 开发阶段的调试信息（如变量值、中间结果）                     | 验证非核心逻辑，仅在开发/测试环境启用 |
| **INFO**     | 系统运行关键节点（如服务启动、业务操作完成）                 | 生产环境默认级别，用于监控健康状态    |
| **WARN**     | 潜在异常或非预期行为（不影响系统运行）                       | 提示需人工检查但无需立即处理的场景    |
| **ERROR**    | 可恢复的系统错误（如业务逻辑异常、外部依赖失败）             | 触发告警机制，要求开发人员排查        |
| **CRITICAL** | 致命错误导致系统崩溃（需立即停止运行）                       | 内存溢出、核心服务崩溃等紧急场景      |

## 日志过滤级别

日志系统有两个重要对象：

- 日志记录器logger
- 附加器sink。日志对外输出的对象，包括控制台、日志文件、流、Qt控件等。

我们的日志采用单日志记录器logger对应多附加器sink，一条日志可以同时输出到多个对象中。

可以通过配置文件对日志和sink设置过滤级别。当一条日志消息发出，首先会被日志记录器进行第一级过滤，当日志消息级别不低于记录器过滤级别，则允许通过，否则会被拦截。通过日志过滤的日志，在输出到sink之前，会进入到每个sink的第二级过滤，同样是当日志消息级别不低于sink记录器过滤级别，才允许通过，打印到最终的输出对象上。

应用实例：可以设置多个sink，每个sink设置不同的过滤级别，将不同级别的日志保存或输出到不同的位置。

## 滚动相关日志文件说明

spdlog关于滚动日志的设计理念：

1. 永远有一个当前正在写入的日志，不带后缀，假设名称为`stem.log`。
2. 当`stem.log`内容达到滚动标准后，修改日志文件名为`stem.1.log`，作为历史日志文件，按照此规定依次累加。

> 不带后缀的 `xxx.log` 表示“正在写的现在”，
>  带 `.1/.2/...` 后缀的表示“已经封存的过去”。

## 配置文件

配置文件默认生成于可执行文件同级目录下，若不存在，自动创建，用户可进行自定义修改内容。

当用户手动调用`setConfigPath`，则前往指定路径下读取配置文件。

## 配置文件使用说明

下面是默认生成的配置文件：

```yaml
logger:
  name: default-log # 日志对象名称
  debug_level: trace # debug模式下日志过滤级别，仅输出高于此级别的日志。off为关闭日志输出
  release_level: info
  flush_on: trace # 高于此级别的日志，进行立即刷新写入
  pattern: "[%Y-%m-%d %H:%M:%S.%e][%n][%^%l%$][thread %t]%v" # 输出格式化字符串
  # [时间][日志名称][日志级别][线程号][日志内容]
  # 注意，代码层面，在日志内容中，增加了打印日志所在文件、方法名、行号
showCodeLine: # 打印下面级别的日志时，是否显示所在文件、方法、行号
  trace: false
  debug: false
  info: false
  warn: true
  error: true
  critical: true
sinks: # 日志输出器，一条日志可以配置发送往多个输出位置
  - type: stdout_color_sink_mt # 输出器类型，控制台色彩打印
    level: trace # 输出其过滤级别，高于此级别的日志才会被打印
        
  - type: basic_file_sink_mt # 普通日志
  	level: trace
    file_path: ./logs/basic_file_sink_mt.log
    truncate: false
    
  - type: rotating_file_mt # size滚动日志，具有容量限制，超出自动分页
    level: trace
    file_path: ./logs/rotating_file_mt.log # 日志输出位置
    max_size: 3072 # 单位KB
    max_files: 5 # 允许存在的最大历史日志数量。必须填写非0正整数，且无法设置为无限大，可以用超大数来模拟。
    rotate_on_open: false # 滚动日志在程序启动时是否就进行一次滚动，false为追加写入

    
  - type: daily_file_mt # 日期分割日志，每天按照给定时间点进行划分新日志文件
    level: trace
    file_path: ./logs/daily_file_mt.log
    rotation_hour: 0 # 每日0：0进行分割
    rotation_min: 0
    max_days: 7 # 最大留存的历史文件天数，第8天就将第1天删除。0表示无意义，不可设置。
    truncate: false # 程序启动时，是否清空原有内容

  - type: count_rotating_file_mt # count滚动日志，具有行数限制，超出自动分页
    level: trace
    file_path: ./logs/count_rotating_file_mt.log # 日志输出位置
    max_count: 100000 # 最大行数，超出分页
    max_files: 5 # 允许存在的最大历史日志数量。必须填写非0正整数，且无法设置为无限大，可以用超大数来模拟。
    rotate_on_open: false # 滚动日志在程序启动时是否就进行一次滚动，false为追加写入
    strict_count_on_open: true # 追加写入的时候，是否先计算一下当前文件的行数，决定是否立即进行滚动
  
  - type: daily_size_rotating_file_mt # 按日期分割 + 单日按大小滚动的 sink
    level: trace
    root_dir: ./logs # 日志文件存放根目录
    name: "前缀{date}后缀"
    # 日志文件名模板（不是最终文件名）
	# 生成规则：<name> + "." + 序号 + ".log"
	# 其中 {date} 会按 date_name_format 替换为当天日期
	# 例：date_name_format = yyyy-MM-dd, name = begin-{date}-end 
	# 当天第一个文件 -> begin-2026-02-05-end.log
	# 超过 max_size 后滚动 -> begin-2026-02-05-end.1.log, begin-2026-02-05-end.2.log ...
	# 第二天自动创建 -> begin-2026-02-06-end.log
	# 只影响文件命名，不影响日志过滤级别
    date_name_format: yyyy-MM-dd # name中{date}对应的日期格式
    rotation_hour: 0 # 每日0：0进行分割
    rotation_min: 0
    max_size: 3072 # 单位KB
    max_files: 10 # 允许存在的最大历史日志数量。必须填写非0正整数，且无法设置为无限大，可以用超大数来模拟。
    rotate_on_open: false # 滚动日志在程序启动时是否就进行一次滚动，false为追加写入
```

## 修改配置文件读取接口

```cpp
	/**
	 * 手动修改读取配置文件的路径
	 * 若不调用此方法修改，默认读取位置为可执行程序所在路径下
	 * @param configFilePath 配置文件路径
	 * @param isDeleteOldConfig 是否删除旧的配置文件
	 */
	static void setConfigPath(const std::string& configFilePath, bool isDeleteOldConfig = true);
```

日志工具提供了修改配置文件读取位置的方法，建议在进入程序入口前进行调用设置。