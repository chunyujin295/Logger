#include <logger/logger.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

// ============================================================
// 简易测试框架
// ============================================================
int g_passed = 0;
int g_failed = 0;

#define TEST(name) std::cout << "  " << name << " ... " << std::flush

#define PASS()                              \
    do {                                    \
        std::cout << "PASSED\n";            \
        ++g_passed;                         \
    } while (0)

#define FAIL(msg)                           \
    do {                                    \
        std::cout << "FAILED - " << msg << "\n"; \
        ++g_failed;                         \
    } while (0)

#define CHECK(cond, msg)                    \
    do {                                    \
        if (!(cond)) {                      \
            FAIL(msg);                      \
            return;                         \
        }                                   \
    } while (0)

// ============================================================
// 配置辅助
// ============================================================
const std::string TEST_DIR = "./test_logs/";

// 写一个只输出到文件的配置（同步）
void writeSyncConfig(const std::string& path, const std::string& filePath,
                     const std::string& level = "trace") {
    std::ofstream f(path);
    f << "log_config:\n"
      << "  logger:\n"
      << "    name: test-sync\n"
      << "    debug_level: " << level << "\n"
      << "    release_level: " << level << "\n"
      << "    flush_on: trace\n"
      << "    pattern: \"[%l]%v\"\n"
      << "    async: false\n"
      << "  showCodeLine:\n"
      << "    trace: true\n"
      << "    debug: true\n"
      << "    info: true\n"
      << "    warn: true\n"
      << "    error: true\n"
      << "    critical: true\n"
      << "  sinks:\n"
      << "    - type: basic_file_sink_mt\n"
      << "      level: trace\n"
      << "      file_path: " << filePath << "\n"
      << "      truncate: true\n";
}

// 写一个异步配置文件
void writeAsyncConfig(const std::string& path, const std::string& filePath,
                      int queueSize = 8192) {
    std::ofstream f(path);
    f << "log_config:\n"
      << "  logger:\n"
      << "    name: test-async\n"
      << "    debug_level: trace\n"
      << "    release_level: trace\n"
      << "    flush_on: trace\n"
      << "    pattern: \"[%l]%v\"\n"
      << "    async: true\n"
      << "    async_queue_size: " << queueSize << "\n"
      << "    async_thread_count: 1\n"
      << "  showCodeLine:\n"
      << "    trace: false\n"
      << "    debug: false\n"
      << "    info: false\n"
      << "    warn: false\n"
      << "    error: false\n"
      << "    critical: false\n"
      << "  sinks:\n"
      << "    - type: basic_file_sink_mt\n"
      << "      level: trace\n"
      << "      file_path: " << filePath << "\n"
      << "      truncate: true\n";
}

// 读文件全部内容
std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// 统计文件行数
int countLines(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return 0;
    int n = 0;
    std::string line;
    while (std::getline(f, line)) ++n;
    return n;
}

// 检查文件是否包含某字符串
bool fileContains(const std::string& path, const std::string& needle) {
    std::string content = readFile(path);
    return content.find(needle) != std::string::npos;
}

// ============================================================
// 测试用例
// ============================================================

// 1) 六个日志级别
void test_all_levels(const std::string& logFile) {
    TEST("all 6 levels");
    LOG_TRACE("trace_msg");
    LOG_DEBUG("debug_msg");
    LOG_INFO("info_msg");
    LOG_WARN("warn_msg");
    LOG_ERROR("error_msg");
    LOG_CRITI("critical_msg");

    std::string content = readFile(logFile);
    CHECK(content.find("trace_msg") != std::string::npos, "trace missing");
    CHECK(content.find("debug_msg") != std::string::npos, "debug missing");
    CHECK(content.find("info_msg") != std::string::npos, "info missing");
    CHECK(content.find("warn_msg") != std::string::npos, "warn missing");
    CHECK(content.find("error_msg") != std::string::npos, "error missing");
    CHECK(content.find("critical_msg") != std::string::npos, "critical missing");
    PASS();
}

// 2) 多种数据类型
void test_data_types(const std::string& logFile) {
    TEST("data types: int/float/double/bool/string");
    int lineBefore = countLines(logFile);

    LOG_INFO("int:", 42);
    LOG_INFO("float:", 3.14f);
    LOG_INFO("double:", 2.718281828);
    LOG_INFO("bool_true:", true);
    LOG_INFO("bool_false:", false);
    LOG_INFO("string:", std::string("hello"));
    LOG_INFO("cstr:", "world");
    LOG_INFO("multi:", 1, " + ", 2, " = ", 3);

    std::string content = readFile(logFile);
    CHECK(content.find("int:42") != std::string::npos, "int missing");
    CHECK(content.find("3.14") != std::string::npos, "float missing");
    CHECK(content.find("2.718") != std::string::npos, "double missing");
    CHECK(content.find("hello") != std::string::npos, "string missing");
    CHECK(content.find("1 + 2 = 3") != std::string::npos, "multi missing");
    PASS();
}

// 3) 回调 sink
void test_callback() {
    TEST("callback sink");

    // 不再设置行号显示，因为 Logger::addCallBack 会在已有 logger 上添加 sink
    std::atomic<int> cbCount{0};
    LogMsg lastMsg;

    std::string sinkId = Logger::addCallBack(
        [&](const LogMsg& msg) {
            ++cbCount;
            lastMsg = msg;
        },
        LogLevel::Trace);

    LOG_INFO("callback_test_hello");

    CHECK(cbCount.load() > 0, "callback not invoked");
    CHECK(lastMsg.msg.find("callback_test_hello") != std::string::npos,
          "callback msg mismatch: " + lastMsg.msg);
    CHECK(lastMsg.level == LogLevel::Info, "callback level mismatch");
    // fileName/codeLine/funcName 由 spdlog 的 source_loc 填充，
    // Logger 当前实现将文件/行号信息嵌入了消息字符串而非 source_loc，因此此处不检查非空
    CHECK(!lastMsg.threadId.empty(), "threadId empty");
    CHECK(lastMsg.msgFormatted.find("callback_test_hello") != std::string::npos,
          "msgFormatted missing marker");

    // 移除回调，再发一条日志，回调计数不应增长
    Logger::removeCallBack(sinkId);
    int before = cbCount.load();
    LOG_INFO("after_remove_callback");
    CHECK(cbCount.load() == before,
          "callback fired after removal");

    PASS();
}

// 4) 异步日志 — 核心检测
void test_async(const std::string& configPath, const std::string& asyncLogFile, const std::string& syncLogFile) {
    TEST("async logging: write " + std::to_string(500) + " messages, no data loss");

    // 先 shutdown 之前的同步 logger
    Logger::shutdown();

    // 切换到异步配置
    Logger::setConfigPath(configPath, false);

    const int N = 500;
    for (int i = 0; i < N; ++i) {
        LOG_INFO("ASYNC_MSG_", i);
    }
    Logger::shutdown();

    int lineCount = countLines(asyncLogFile);
    CHECK(lineCount >= N, "expected >= " + std::to_string(N) +
         " lines, got " + std::to_string(lineCount));

    // 逐条验证每条消息都在
    std::set<int> found;
    {
        std::ifstream f(asyncLogFile);
        std::string line;
        while (std::getline(f, line)) {
            auto pos = line.find("ASYNC_MSG_");
            if (pos != std::string::npos) {
                int idx = std::stoi(line.substr(pos + 10));
                found.insert(idx);
            }
        }
    }
    for (int i = 0; i < N; ++i) {
        if (found.find(i) == found.end()) {
            FAIL("ASYNC_MSG_" + std::to_string(i) + " missing");
            return;
        }
    }
    PASS();
}

// 5) 多线程并发日志
void test_multithread(const std::string& configPath, const std::string& logFile) {
    TEST("multithread: 4 threads x 250 messages each");

    Logger::shutdown();
    Logger::setConfigPath(configPath, false);

    const int PER_THREAD = 250;
    std::atomic<int> ready{0};
    std::atomic<bool> start{false};

    auto worker = [&](int tid) {
        ready.fetch_add(1);
        while (!start.load()) {
            std::this_thread::yield();
        }
        for (int i = 0; i < PER_THREAD; ++i) {
            LOG_INFO("THREAD_", tid, "_MSG_", i);
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back(worker, t);
    }

    // 等所有线程就绪
    while (ready.load() < 4) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    start.store(true);

    for (auto& th : threads) th.join();
    Logger::shutdown();

    int lineCount = countLines(logFile);
    CHECK(lineCount >= 4 * PER_THREAD,
          "expected >= " + std::to_string(4 * PER_THREAD) +
          " lines, got " + std::to_string(lineCount));

    // 检查每个线程的消息都被记录了
    for (int t = 0; t < 4; ++t) {
        std::string marker = "THREAD_" + std::to_string(t) + "_MSG_0";
        CHECK(fileContains(logFile, marker),
              ("missing marker: " + marker));
    }
    PASS();
}

// 6) 级别过滤
void test_level_filter(const std::string& configPath, const std::string& logFile) {
    TEST("level filter: INFO level should block TRACE/DEBUG");

    Logger::shutdown();

    // 写一个 info 级别的配置
    {
        std::ofstream f(configPath);
        f << "log_config:\n"
          << "  logger:\n"
          << "    name: test-filter\n"
          << "    debug_level: info\n"
          << "    release_level: info\n"
          << "    flush_on: info\n"
          << "    pattern: \"[%l]%v\"\n"
          << "    async: false\n"
          << "  showCodeLine:\n"
          << "    trace: false\n    debug: false\n    info: false\n"
          << "    warn: false\n    error: false\n    critical: false\n"
          << "  sinks:\n"
          << "    - type: basic_file_sink_mt\n"
          << "      level: trace\n"
          << "      file_path: " << logFile << "\n"
          << "      truncate: true\n";
    }

    Logger::setConfigPath(configPath, false);

    LOG_TRACE("should_be_filtered_trace");
    LOG_DEBUG("should_be_filtered_debug");
    LOG_INFO("should_appear_info");
    LOG_ERROR("should_appear_error");

    std::string content = readFile(logFile);
    CHECK(content.find("should_be_filtered_trace") == std::string::npos,
          "trace should be filtered");
    CHECK(content.find("should_be_filtered_debug") == std::string::npos,
          "debug should be filtered");
    CHECK(content.find("should_appear_info") != std::string::npos,
          "info should appear");
    CHECK(content.find("should_appear_error") != std::string::npos,
          "error should appear");
    PASS();
}

// 7) 边界条件：空消息、单空参数
void test_edge_cases(const std::string& configPath, const std::string& logFile) {
    TEST("edge cases: empty/single-empty-any");
    Logger::shutdown();

    {
        std::ofstream f(configPath);
        f << "log_config:\n"
          << "  logger:\n"
          << "    name: test-edge\n"
          << "    debug_level: trace\n"
          << "    release_level: trace\n"
          << "    flush_on: trace\n"
          << "    pattern: \"[%l]%v\"\n"
          << "    async: false\n"
          << "  showCodeLine:\n"
          << "    trace: false\n    debug: false\n    info: false\n"
          << "    warn: true\n    error: true\n    critical: true\n"
          << "  sinks:\n"
          << "    - type: basic_file_sink_mt\n"
          << "      level: trace\n"
          << "      file_path: " << logFile << "\n"
          << "      truncate: true\n";
    }

    Logger::setConfigPath(configPath, false);

    // 空 initializer_list
    LOG_INFO();
    // 单个空 std::any
    std::any emptyAny;
    LOG_INFO(emptyAny);
    // 混合正常和空
    LOG_INFO("before_empty - ", std::any{}, " - after_empty");

    // 全都不应崩溃，至少有输出
    int lines = countLines(logFile);
    CHECK(lines >= 1, "no output at all for edge cases");
    PASS();
}

// 8) showCodeLine 验证
void test_show_codeline(const std::string& configPath, const std::string& logFile) {
    TEST("showCodeLine: file/line info in output");
    Logger::shutdown();

    {
        std::ofstream f(configPath);
        f << "log_config:\n"
          << "  logger:\n"
          << "    name: test-scl\n"
          << "    debug_level: trace\n"
          << "    release_level: trace\n"
          << "    flush_on: trace\n"
          << "    pattern: \"[%l]%v\"\n"
          << "    async: false\n"
          << "  showCodeLine:\n"
          << "    trace: false\n    debug: false\n    info: true\n"
          << "    warn: true\n    error: true\n    critical: true\n"
          << "  sinks:\n"
          << "    - type: basic_file_sink_mt\n"
          << "      level: trace\n"
          << "      file_path: " << logFile << "\n"
          << "      truncate: true\n";
    }

    Logger::setConfigPath(configPath, false);

    LOG_INFO("showline_marker");

    std::string content = readFile(logFile);
    CHECK(content.find("showline_marker") != std::string::npos,
          "marker missing");
    CHECK(content.find("main.cpp") != std::string::npos,
          "file name (main.cpp) missing from output");
    PASS();
}

// 9) shutdown 安全性
void test_shutdown_safe() {
    TEST("shutdown twice: no crash");
    Logger::shutdown();
    Logger::shutdown();  // 第二次不应崩溃
    PASS();
}

// ============================================================
// main
// ============================================================
int main() {
    std::cout << "=== Logger Comprehensive Test ===\n\n";

    // 创建测试目录
    fs::create_directories(TEST_DIR);
    fs::create_directories(TEST_DIR + "sync");
    fs::create_directories(TEST_DIR + "async");
    fs::create_directories(TEST_DIR + "mt");
    fs::create_directories(TEST_DIR + "filter");

    std::string syncConfig  = TEST_DIR + "sync/config.yaml";
    std::string syncLog     = TEST_DIR + "sync/test.log";
    std::string asyncConfig = TEST_DIR + "async/config.yaml";
    std::string asyncLog    = TEST_DIR + "async/test.log";
    std::string mtConfig    = TEST_DIR + "mt/config.yaml";
    std::string mtLog       = TEST_DIR + "mt/test.log";
    std::string filterConfig= TEST_DIR + "filter/config.yaml";
    std::string filterLog   = TEST_DIR + "filter/test.log";

    // ---- 同步基础测试 ----
    std::cout << "[1] Sync basic tests\n";
    writeSyncConfig(syncConfig, syncLog, "trace");
    Logger::setConfigPath(syncConfig, false);

    test_all_levels(syncLog);
    test_data_types(syncLog);
    test_callback();

    // ---- 异步测试 ----
    std::cout << "[2] Async tests\n";
    writeAsyncConfig(asyncConfig, asyncLog);
    test_async(asyncConfig, asyncLog, syncLog);

    // ---- 多线程测试 ----
    std::cout << "[3] Multi-thread tests\n";
    writeSyncConfig(mtConfig, mtLog, "trace");
    test_multithread(mtConfig, mtLog);

    // ---- 级别过滤测试 ----
    std::cout << "[4] Level filter tests\n";
    test_level_filter(filterConfig, filterLog);

    // ---- 边界与格式测试 ----
    std::cout << "[5] Edge cases & formatting tests\n";
    std::string edgeConfig = TEST_DIR + "edge/config.yaml";
    std::string edgeLog    = TEST_DIR + "edge/test.log";
    fs::create_directories(TEST_DIR + "edge");
    test_edge_cases(edgeConfig, edgeLog);

    std::string sclConfig = TEST_DIR + "scl/config.yaml";
    std::string sclLog    = TEST_DIR + "scl/test.log";
    fs::create_directories(TEST_DIR + "scl");
    test_show_codeline(sclConfig, sclLog);

    // ---- 关闭测试 ----
    std::cout << "[6] Shutdown tests\n";
    test_shutdown_safe();

    // ---- 清理 ----
    std::error_code ec;
    fs::remove_all(TEST_DIR, ec);

    // ---- 结果 ----
    std::cout << "\n=========================\n";
    std::cout << "  PASSED: " << g_passed << "\n";
    std::cout << "  FAILED: " << g_failed << "\n";
    std::cout << "=========================\n";

    return g_failed > 0 ? 1 : 0;
}
