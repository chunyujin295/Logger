<p align="center">
  <img src="./doc/img/LOGO.png" alt="LOGO" width="200">
</p>
## 1. 项目概述

### 1.1 项目简介

Logger 是一个基于 spdlog 的 C++ 日志库，支持通过 YAML 配置文件进行日志 sink 输出配置。该项目提供了简单易用的日志接口，支持多线程安全、多种日志输出方式、日志级别过滤等高级功能。

### 1.2 主要特性

- 基于 spdlog 高性能日志库
- 支持 YAML 配置文件管理日志配置
- 六个日志级别：TRACE、DEBUG、INFO、WARN、ERROR、CRITICAL
- 多种日志输出方式（控制台、文件、滚动文件、日期分割文件等）
- 支持日志回调函数
- 自动显示文件名、行号、函数名信息
- 线程安全设计
- 支持自定义日志格式

### 1.3 技术栈

- **编程语言**：C++17
- **构建系统**：CMake 3.21+
- **核心依赖**：
  - spdlog 1.16.0（日志库）
  - yaml-tool 1.1.1（YAML 配置解析）
  - yaml-cpp（YAML 底层库）

## 2. 项目结构

```
Logger/
├── 3rd/                          # 第三方库
│   └── spdlog-1.16.0/           # spdlog 日志库
├── cmake/                        # CMake 配置文件
│   ├── vendor/                  # CPM.cmake 包管理器
│   ├── 3rd.cmake                # 第三方库配置
│   ├── CPM.cmake                # CPM 包管理器
│   ├── LoggerConfig.cmake.in    # Logger 配置模板
│   └── option.cmake             # CMake 选项
├── doc/                          # 文档目录
│   ├── img/                     # 图片资源
│   └── 项目创建记录.md          # 项目创建记录
├── logger/                       # 核心日志库
│   ├── include/                 # 公共头文件
│   │   └── logger/
│   │       ├── logger.h         # 日志接口定义
│   │       ├── export.h         # 导出宏定义
│   │       └── anytostring.hpp # 类型转换工具
│   ├── private/                 # 私有实现
│   │   ├── logger_p.h           # 私有类定义
│   │   ├── logger_p.cpp         # 私有类实现
│   │   ├── count_rotating_file_mt_sink.hpp      # 按行数滚动 sink
│   │   ├── daily_count_rotating_file_sink.hpp   # 日期+行数滚动 sink
│   │   ├── daily_size_rotating_file_mt_sink.hpp # 日期+大小滚动 sink
│   │   └── id8generator.hpp      # ID 生成器
│   ├── src/                     # 源文件
│   │   └── logger.cpp           # 日志接口实现
│   └── CMakeLists.txt           # Logger 库构建配置
├── test/                         # 测试代码
│   ├── main.cpp                 # 测试主程序
│   └── CMakeLists.txt           # 测试构建配置
├── CMakeLists.txt               # 项目根构建配置
├── README.md                    # 项目说明文档
└── .gitignore                   # Git 忽略配置
```

## 3. 核心模块说明

### 3.1 日志接口模块（logger.h）

提供公共日志接口，包括：

- 日志级别枚举：LogLevel（Trace、Debug、Info、Warn、Error、Critical）
- 日志消息结构：LogMsg（包含文件名、行号、函数名、线程ID、级别、消息等）
- 日志输出方法：trace、debug、info、warn、error、critical
- 配置管理：setConfigPath
- 回调函数管理：addCallBack、removeCallBack

### 3.2 日志实现模块（logger_p.h/cpp）

采用 PIMPL 模式实现，包含：

- 单例模式确保全局唯一实例
- 配置文件加载和解析
- 日志 sink 管理
- 字符串拼接和类型转换
- 自定义 sink 实现（callback_sink）

### 3.3 自定义 Sink 模块

项目扩展了多种自定义 sink：

- **count_rotating_file_mt_sink**：按日志行数滚动的文件 sink
- **daily_count_rotating_file_sink**：按日期分割+行数滚动的文件 sink
- **daily_size_rotating_file_mt_sink**：按日期分割+大小滚动的文件 sink

### 3.4 工具模块

- **anytostring.hpp**：std::any 类型到字符串的转换
- **id8generator.hpp**：8 位 ID 生成器

## 4. 构建配置

### 4.1 CMake 选项

项目提供以下 CMake 构建选项：

- `BUILD_TEST`：是否构建测试程序（默认 ON）
- `LOGGER_INSTALL`：是否安装 Logger 及其依赖（默认 ON）

### 4.2 依赖管理

使用 CPM（CMake Package Manager）管理第三方依赖：

- **spdlog**：静态链接（SPDLOG_BUILD_SHARED OFF）
- **yaml-tool**：动态链接（YAML_TOOL_BUILD_SHARED_LIBS ON）
- **yaml-cpp**：随 yaml-tool 自动引入

### 4.3 构建命令

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake ..

# 构建
cmake --build . --config Release

# 安装（可选）
cmake --install .
```

### 4.4 平台特定配置

- **MinGW**：静态链接 GCC 运行时库，防止依赖不明确
- **Windows**：支持 Windows 特定 sink（msvc、wincolor）

## 5. 使用说明

### 5.1 基本使用

```cpp
#include <logger>

// 设置配置文件路径（可选）
Logger::setConfigPath("./myLogConfig.yml");

// 使用日志宏
LOG_INFO("Application started!");
LOG_DEBUG("Variable value: ", value);
LOG_WARN("Warning message");
LOG_ERROR("Error occurred: ", error_msg);
```

### 5.2 日志级别说明

| 级别     | 说明                 | 典型使用场景                     |
| -------- | -------------------- | -------------------------------- |
| TRACE    | 最详细的日志信息     | 深度调试，生产环境通常关闭       |
| DEBUG    | 开发阶段的调试信息   | 验证非核心逻辑                   |
| INFO     | 系统运行关键节点     | 生产环境默认级别，监控健康状态   |
| WARN     | 潜在异常或非预期行为 | 提示需人工检查但无需立即处理     |
| ERROR    | 可恢复的系统错误     | 触发告警机制，要求开发人员排查   |
| CRITICAL | 致命错误导致系统崩溃 | 内存溢出、核心服务崩溃等紧急场景 |

### 5.3 日志过滤机制

日志系统采用两级过滤：

1. **Logger 级过滤**：全局日志级别过滤
2. **Sink 级过滤**：每个 sink 独立的级别过滤

一条日志消息必须同时通过两级过滤才会被输出。

### 5.4 配置文件说明

默认配置文件位置：可执行文件同级目录下的 `log_config.yaml`

#### 配置文件结构

```yaml
logger:
  name: default-log              # 日志对象名称
  debug_level: trace             # Debug 模式过滤级别
  release_level: info            # Release 模式过滤级别
  flush_on: trace               # 立即刷新级别
  pattern: "[%Y-%m-%d %H:%M:%S.%e][%n][%^%l%$][thread %t]%v"

showCodeLine:                   # 是否显示代码位置信息
  trace: false
  debug: false
  info: false
  warn: true
  error: true
  critical: true

sinks:                          # 日志输出器列表
  - type: stdout_color_sink_mt  # 控制台彩色输出
    level: trace
  
  - type: basic_file_sink_mt    # 普通文件输出
    level: trace
    file_path: ./logs/basic_file_sink_mt.log
    truncate: false
  
  - type: rotating_file_mt      # 按大小滚动文件
    level: trace
    file_path: ./logs/rotating_file_mt.log
    max_size: 3072              # 单位 KB
    max_files: 5
    rotate_on_open: false
  
  - type: daily_file_mt         # 按日期分割文件
    level: trace
    file_path: ./logs/daily_file_mt.log
    rotation_hour: 0
    rotation_min: 0
    max_days: 7
    truncate: false
  
  - type: count_rotating_file_mt  # 按行数滚动文件
    level: trace
    file_path: ./logs/count_rotating_file_mt.log
    max_count: 100000
    max_files: 5
    rotate_on_open: false
    strict_count_on_open: true
  
  - type: daily_size_rotating_file_mt  # 日期+大小滚动
    level: trace
    root_dir: ./logs
    name: "前缀{date}后缀"
    date_name_format: yyyy-MM-dd
    rotation_hour: 0
    rotation_min: 0
    max_size: 3072
    max_files: 10
    rotate_on_open: false
```

### 5.5 滚动日志说明

spdlog 滚动日志设计理念：

1. 永远有一个当前正在写入的日志，不带后缀（如 `stem.log`）
2. 当达到滚动标准后，重命名为 `stem.1.log`，依次累加

> 不带后缀的 `xxx.log` 表示"正在写的现在"，带 `.1/.2/...` 后缀的表示"已经封存的过去"。

### 5.6 回调函数使用

```cpp
// 添加回调函数
std::string sinkId = Logger::addCallBack(
    [](const LogMsg& logMsg) {
        std::cout << "Callback: " << logMsg.msg << std::endl;
    },
    LogLevel::Info
);

// 移除回调函数
Logger::removeCallBack(sinkId);
```

## 6. 开发指南

### 6.1 添加新的日志类型支持

如需支持新的日志类型，修改 `logger_p.cpp` 中的 `anyToString` 方法。

### 6.2 添加新的 Sink 类型

1. 在 `logger/private/` 目录下创建新的 sink 头文件
2. 继承 `spdlog::sinks::base_sink<std::mutex>` 或其他基类
3. 在 `logger_p.cpp` 中添加 sink 类型常量
4. 在配置加载逻辑中添加对应的 sink 创建代码

### 6.3 代码规范

- 使用 PIMPL 模式隐藏实现细节
- 所有公共接口使用静态方法
- 使用宏定义简化日志调用
- 保持线程安全，使用互斥锁保护共享资源

### 6.4 测试

项目包含测试程序，可通过以下命令运行：

```bash
cd build
ctest
```

## 7. 安装和部署

### 7.1 安装内容

安装时包含以下内容：

- Logger 库（动态库）
- yaml-tool 库（动态库）
- yaml-cpp 库（动态库）
- spdlog 库（静态库）
- 所有相关头文件
- 测试程序（如果启用）

### 7.2 安装命令

```bash
cmake --install . --prefix /usr/local
```

### 7.3 集成到其他项目

```cmake
find_package(Logger REQUIRED)

target_link_libraries(your_target PRIVATE Logger::Logger)
```

## 8. 注意事项

1. **配置文件**：默认配置文件会自动生成，用户可自定义修改
2. **线程安全**：所有 sink 都使用多线程安全版本（_mt 后缀）
3. **性能考虑**：生产环境建议将日志级别设置为 INFO 或更高
4. **日志滚动**：滚动日志文件数量必须为非零正整数
5. **路径检查**：日志文件路径不存在时会自动创建

## 9. 版本历史

- **v1.0.0**：初始版本
  - 基于 spdlog 1.16.0
  - 支持 YAML 配置文件
  - 支持多种 sink 类型
  - 支持日志回调函数

## 10. 许可证

本项目遵循 spdlog 的许可证（MIT License）。

## 11. 联系方式

- 作者：chenyujin@mozihealthcare.cn
- 项目地址：[待补充]

## 12. 参考资料

- [spdlog 官方文档](https://github.com/gabime/spdlog)
- [yaml-tool 项目](https://github.com/chunyujin295/yaml-tool)
- [CPM 包管理器](https://github.com/cpm-cmake/CPM)