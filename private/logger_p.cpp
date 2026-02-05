#include "logger_p.h"
#include "count_rotating_file_mt_sink.hpp"
// #include "daily_dir_size_rotating_file_sink.hpp"
#include "daily_size_rotating_file_mt_sink.hpp"
#include "yamltool/yamlnode.h"
#include "yamltool/yamltool.h"

#include <QColor>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <QString>
#include <codecvt>
#include <cstdarg>
#include <cwctype>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <regex>
#include <unordered_set>

// -------------------------日志输出通道的类型-------------------------------------
// ============================================================================
// 1. 多线程安全版本 (_mt) —— 内部带锁，线程安全
// ============================================================================
const std::string SINK_TYPE_STDOUT_COLOR_SINK_MT = "stdout_color_sink_mt";// 彩色 stdout (ANSI)  // 目前启用----------------
const std::string SINK_TYPE_STDERR_COLOR_SINK_MT = "stderr_color_sink_mt";// 彩色 stderr (ANSI)
const std::string SINK_TYPE_STDOUT_SINK_MT = "stdout_sink_mt";            // 普通 stdout
const std::string SINK_TYPE_STDERR_SINK_MT = "stderr_sink_mt";            // 普通 stderr

const std::string SINK_TYPE_BASIC_FILE_SINK_MT = "basic_file_sink_mt";// 普通文件 sink // 目前启用----------------
const std::string SINK_TYPE_ROTATING_FILE_MT = "rotating_file_mt";    // 支持文件轮转 // 目前启用----------------
const std::string SINK_TYPE_DAILY_FILE_MT = "daily_file_mt";          // 每日生成新日志文件 // 目前启用----------------

const std::string SINK_TYPE_OSTREAM_MT = "ostream_mt";// 输出到 std::ostream
const std::string SINK_TYPE_DIST_MT = "dist_mt";      // 分发 sink，可组合多个 sink

// ============================================================================
// 2. 单线程版本 (_st) —— 无锁，性能高，但不适合多线程
// ============================================================================
const std::string SINK_TYPE_STDOUT_COLOR_SINK_ST = "stdout_color_sink_st";// 彩色 stdout (ANSI)
const std::string SINK_TYPE_STDERR_COLOR_SINK_ST = "stderr_color_sink_st";// 彩色 stderr (ANSI)
const std::string SINK_TYPE_STDOUT_SINK_ST = "stdout_sink_st";            // 普通 stdout
const std::string SINK_TYPE_STDERR_SINK_ST = "stderr_sink_st";            // 普通 stderr

const std::string SINK_TYPE_BASIC_FILE_SINK_ST = "basic_file_sink_st";// 普通文件 sink
const std::string SINK_TYPE_ROTATING_FILE_ST = "rotating_file_st";    // 支持文件轮转
const std::string SINK_TYPE_DAILY_FILE_ST = "daily_file_st";          // 每日生成新日志文件

const std::string SINK_TYPE_OSTREAM_ST = "ostream_st";// 输出到 std::ostream
const std::string SINK_TYPE_DIST_ST = "dist_st";      // 分发 sink，可组合多个 sink

// ============================================================================
// 3. 无 _mt/_st 区分的 sink —— 只有一个版本
// ============================================================================
const std::string SINK_TYPE_MSVC = "msvc";        // 输出到 Visual Studio 调试窗口
const std::string SINK_TYPE_WINCOLOR = "wincolor";// Windows 控制台彩色输出
const std::string SINK_TYPE_SYSLOG = "syslog";    // Linux/Unix 系统 syslog
const std::string SINK_TYPE_ANDROID = "android";  // Android logcat

const std::string SINK_TYPE_NULL = "null";// 黑洞 sink，丢弃日志

// ============================================================================
// 4. 自定义sink
// ============================================================================
const std::string SINK_TYPE_COUNT_ROTATING_FILE_MT = "count_rotating_file_mt";          // 按照日志条数进行滚动的日志sink // 目前启用----------------
const std::string SINK_TYPE_DAILY_SIZE_ROTATING_FILE_MT = "daily_size_rotating_file_mt";// 按照日志条数进行滚动的日期日志sink // 目前启用----------------
// ------------------------------------------------------------------------------

LogPrivate* LogPrivate::s_instance = nullptr;
std::mutex LogPrivate::s_mutex;

std::string LogPrivate::m_configFilePath = "./log_config.yaml";
bool LogPrivate::m_traceShowLine = false;
bool LogPrivate::m_debugShowLine = false;
bool LogPrivate::m_infoShowLine = false;
bool LogPrivate::m_warnShowLine = true;
bool LogPrivate::m_errorShowLine = true;
bool LogPrivate::m_criticalShowLine = true;

std::unordered_map<std::string, std::shared_ptr<spdlog::sinks::sink>> LogPrivate::m_callbackSinks;

ID8Generator LogPrivate::m_id8Generator;


// 创建一个回调sink类
// 不使用spdlog原生call_back_sink：只能取得日志原始内容，无法获取格式化内容
class callback_sink : public spdlog::sinks::base_sink<std::mutex>
{
public:
	using callback_t = std::function<void(const LogMsg&)>;

	explicit callback_sink(callback_t cb)
		: callback_(std::move(cb))
	{
	}

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override
	{
		// 格式化消息
		spdlog::memory_buf_t formatted;
		this->formatter_->format(msg, formatted);
		// 调用回调，传递格式化后的字符串
		if (callback_)
		{
			LogMsg logMsg;
			logMsg.fileName = msg.source.filename ? std::string(msg.source.filename) : "";
			logMsg.codeLine = std::to_string(msg.source.line);
			logMsg.funcName = msg.source.funcname ? std::string(msg.source.funcname) : "";
			logMsg.threadId = std::to_string(msg.thread_id);
			logMsg.msg = std::string(msg.payload.begin(), msg.payload.end());
			logMsg.msgFormatted = fmt::to_string(formatted);
			logMsg.level = static_cast<LogLevel>(msg.level);

			callback_(logMsg);
		}
	}

	void flush_() override// 此方法必须override
	{
		// 可选实现
	}

private:
	callback_t callback_;
};


void LogPrivate::setConfigPath(const std::string& configFilePath, bool isDeleteOldConfig)
{
	std::string oldConfigPath = m_configFilePath;
	try
	{
		getInstance()->loadConfigFile(configFilePath);
		if (getInstance()->getLogger()->level() == spdlog::level::off)// 日志级别设置失败，为off
		{
			std::cout << "[LogPrivate] 日志级别为off" << std::endl;
		}
	}
	catch (const spdlog::spdlog_ex& ex)// 捕获读取配置文件过程中遇到的异常
	{
		std::cout << "[LogPrivate] Log initialization error: " << ex.what() << std::endl;
		getInstance()->loadDefaultConfig(configFilePath);// 采用默认配置
	}
	if (isDeleteOldConfig)
	{
		getInstance()->deleteOldConfig(oldConfigPath);
	}
}

void LogPrivate::trace(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList)
{
	std::string msg;
	msg.clear();
	msg = linkString(fileName, fileLine, function, msgList);
	if (msgList.size() == 1)
	{
		if (msg.empty())
		{
			getInstance()->getLogger()->trace("[{}:{}][{}] 日志内容为空", fileName, fileLine, function);
			return;
		}
	}
	else if (msg.empty())
	{
		getInstance()->getLogger()->trace("[{}:{}][{}] 日志打印失败，数据类型转换错误", fileName, fileLine, function);
		return;
	}
	if (!m_traceShowLine)
	{
		getInstance()->getLogger()->trace("{}", msg);
		return;
	}
	getInstance()->getLogger()->trace("[{}:{}][{}]{}", fileName, fileLine, function, msg);
}

void LogPrivate::debug(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList)
{
	std::string msg;
	msg.clear();
	msg = linkString(fileName, fileLine, function, msgList);
	if (msgList.size() == 1)
	{
		if (msg.empty())
		{
			getInstance()->getLogger()->debug("[{}:{}][{}] 日志内容为空", fileName, fileLine, function);
			return;
		}
	}
	else if (msg.empty())
	{
		getInstance()->getLogger()->debug("[{}:{}][{}] 日志打印失败，数据类型转换错误", fileName, fileLine, function);
		return;
	}
	if (!m_debugShowLine)
	{
		getInstance()->getLogger()->debug("{}", msg);
		return;
	}
	getInstance()->getLogger()->debug("[{}:{}][{}]{}", fileName, fileLine, function, msg);
}

void LogPrivate::info(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList)
{
	std::string msg;
	msg.clear();
	msg = linkString(fileName, fileLine, function, msgList);
	if (msgList.size() == 1)
	{
		if (msg.empty())
		{
			getInstance()->getLogger()->info("[{}:{}][{}] 日志内容为空", fileName, fileLine, function);
			return;
		}
	}
	else if (msg.empty())
	{
		getInstance()->getLogger()->info("[{}:{}][{}] 日志打印失败，数据类型转换错误", fileName, fileLine, function);
		return;
	}

	if (!m_infoShowLine)
	{
		getInstance()->getLogger()->info("{}", msg);
		return;
	}
	getInstance()->getLogger()->info("[{}:{}][{}]{}", fileName, fileLine, function, msg);
}

void LogPrivate::warn(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList)
{
	std::string msg;
	msg.clear();
	msg = linkString(fileName, fileLine, function, msgList);
	if (msgList.size() == 1)
	{
		if (msg.empty())
		{
			getInstance()->getLogger()->warn("[{}:{}][{}] 日志内容为空", fileName, fileLine, function);
			return;
		}
	}
	else if (msg.empty())
	{
		getInstance()->getLogger()->warn("[{}:{}][{}] 日志打印失败，数据类型转换错误", fileName, fileLine, function);
		return;
	}
	if (!m_warnShowLine)
	{
		getInstance()->getLogger()->warn("{}", msg);
		return;
	}
	getInstance()->getLogger()->warn("[{}:{}][{}]{}", fileName, fileLine, function, msg);
}

void LogPrivate::error(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList)
{
	std::string msg;
	msg.clear();
	msg = linkString(fileName, fileLine, function, msgList);
	if (msgList.size() == 1)
	{
		if (msg.empty())
		{
			getInstance()->getLogger()->error("[{}:{}][{}] 日志内容为空", fileName, fileLine, function);
			return;
		}
	}
	else if (msg.empty())
	{
		getInstance()->getLogger()->error("[{}:{}][{}] 日志打印失败，数据类型转换错误", fileName, fileLine, function);
		return;
	}
	if (!m_errorShowLine)
	{
		getInstance()->getLogger()->error("{}", msg);
		return;
	}
	getInstance()->getLogger()->error("[{}:{}][{}]{}", fileName, fileLine, function, msg);
}

void LogPrivate::critical(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList)
{
	std::string msg;
	msg.clear();
	msg = linkString(fileName, fileLine, function, msgList);
	if (msgList.size() == 1)
	{
		if (msg.empty())
		{
			getInstance()->getLogger()->critical("[{}:{}][{}] 日志内容为空", fileName, fileLine, function);
			return;
		}
	}
	else if (msg.empty())
	{
		getInstance()->getLogger()->critical("[{}:{}][{}] 日志打印失败，数据类型转换错误", fileName, fileLine, function);
		return;
	}
	if (!m_criticalShowLine)
	{
		getInstance()->getLogger()->critical("{}", msg);
		return;
	}
	getInstance()->getLogger()->critical("[{}:{}][{}]{}", fileName, fileLine, function, msg);
}

void LogPrivate::setStreamOutPut(std::ostringstream& stream, bool flush, LogLevel level)
{
	auto streamSink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream, flush);
	spdlog::level::level_enum spdlogLevel = spdlog::level::trace;
	switch (level)
	{
	case LogLevel::Trace:
		spdlogLevel = spdlog::level::trace;
		break;
	case LogLevel::Debug:
		spdlogLevel = spdlog::level::debug;
		break;
	case LogLevel::Info:
		spdlogLevel = spdlog::level::info;
		break;
	case LogLevel::Warn:
		spdlogLevel = spdlog::level::warn;
		break;
	case LogLevel::Error:
		spdlogLevel = spdlog::level::err;
		break;
	case LogLevel::Critical:
		spdlogLevel = spdlog::level::critical;
		break;
	defalut:
		break;
	}

	streamSink->set_level(spdlogLevel);
	getInstance()->getLogger()->sinks().push_back(streamSink);
}


std::string LogPrivate::addCallBackSink(const std::function<void(const LogMsg& logMsg)>& logCallBack, LogLevel level)
{
	auto logger = getInstance()->getLogger();
	if (!logger)
		return {};

	auto sinkId = m_id8Generator();

	// 如果相同ID的sink已经存在，则先删除旧的
	auto it = m_callbackSinks.find(sinkId);
	if (it != m_callbackSinks.end())
	{
		logger->sinks().erase(std::remove(logger->sinks().begin(), logger->sinks().end(), it->second),
							  logger->sinks().end());
		m_callbackSinks.erase(it);
		std::cout << "[LogPrivate] 覆盖原有回调 sink: " << sinkId << std::endl;
	}

	auto cbSink = std::make_shared<callback_sink>([logCallBack](const LogMsg& logMsg) {
		logCallBack(logMsg);
	});

	cbSink->set_level(static_cast<spdlog::level::level_enum>(level));
	logger->sinks().push_back(cbSink);
	m_callbackSinks[sinkId] = cbSink;

	std::cout << "[LogPrivate] 添加回调 sink: " << sinkId << std::endl;

	return sinkId;
}
void LogPrivate::removeCallBackSink(const std::string& sinkId)
{
	auto logger = getInstance()->getLogger();
	if (!logger)
		return;

	auto it = m_callbackSinks.find(sinkId);
	if (it != m_callbackSinks.end())
	{
		logger->sinks().erase(std::remove(logger->sinks().begin(), logger->sinks().end(), it->second),
							  logger->sinks().end());
		m_callbackSinks.erase(it);

		std::cout << "[LogPrivate] 移除回调 sink: " << sinkId << std::endl;
	}
	else
	{
		std::cout << "[LogPrivate] 未找到回调 sink: " << sinkId << std::endl;
	}
}

LogPrivate::LogPrivate()
{
	// 设置全局错误处理程序
	spdlog::set_error_handler(
			[](const std::string& msg) {
				std::cout << "[LogPrivate] Log initialization failed: " << msg << std::endl;
			});

	m_configFilePath = "./log_config.yaml";
	try
	{
		this->loadConfigFile(m_configFilePath);           //默认配置文件存在于可执行程序所在路径
		if (this->m_logger->level() == spdlog::level::off)// 日志级别设置失败，为off
		{
			std::cout << "[LogPrivate] 日志级别为off" << std::endl;
		}
	}
	catch (const spdlog::spdlog_ex& ex)// 捕获读取配置文件过程中遇到的异常
	{
		std::cout << "[LogPrivate] Log initialization error: " << ex.what() << std::endl;
		this->loadDefaultConfig(m_configFilePath);// 采用默认配置
	}
}

LogPrivate* LogPrivate::getInstance()
{
	// DCL双检锁创建单例
	if (s_instance == nullptr)
	{
		std::lock_guard<std::mutex> lock(s_mutex);// 确保多线程创建单例时线程安全
		if (s_instance == nullptr)
		{
			s_instance = new LogPrivate();
		}
	}
	// 初始化异步线程池
	// spdlog::init_thread_pool(8192, 1);
	return s_instance;
}

std::shared_ptr<spdlog::logger> LogPrivate::getLogger()
{
	return m_logger;
}

void LogPrivate::loadConfigFile(const std::string& configFilePath)
{
	this->m_logger.reset();//重新设置日志

	mozi::YamlNode rootNode;
	if (!mozi::YamlTool::loadFile(rootNode, configFilePath))
	{
		throw spdlog::spdlog_ex("加载日志yaml配置文件失败，路径：" + std::filesystem::absolute(configFilePath).string());
	}
	mozi::YamlNode logConfigNode = mozi::YamlTool::getNode(rootNode, "log_config");
	if (!logConfigNode.isDefined() || logConfigNode.isNull())
	{
		throw spdlog::spdlog_ex("日志yaml配置文件中, <LogConfig> 节点未定义或为空");
	}
	mozi::YamlNode loggerNode = mozi::YamlTool::getNode(logConfigNode, "logger");
	if (!loggerNode.isDefined() || loggerNode.isNull())
	{
		throw spdlog::spdlog_ex("日志yaml配置文件中, <Logger> 节点未定义或为空");
	}

	// deleteOldConfig(m_configFilePath);

	m_configFilePath = configFilePath;
	// 获取logger名称
	auto loggerName = mozi::YamlTool::getDef<std::string>(loggerNode, "name", "default-logger");

	// 获取DEBUG模式下logger过滤级别
	auto debugLevelStr = mozi::YamlTool::getDef<std::string>(loggerNode, "debug_level", "trace");
	spdlog::level::level_enum debugLevel = spdlog::level::from_str(debugLevelStr);

	// 获取RELEASE模式下logger过滤级别
	auto releaseLevelStr = mozi::YamlTool::getDef<std::string>(loggerNode, "release_level", "info");
	spdlog::level::level_enum releaseLevel = spdlog::level::from_str(releaseLevelStr);

	// 获取flush_on级别
	auto flushOnStr = mozi::YamlTool::getDef<std::string>(loggerNode, "flush_on", "trace");
	spdlog::level::level_enum flushOn = spdlog::level::from_str(flushOnStr);

	// 获取输出格式
	auto logPatternStr = mozi::YamlTool::getDef<std::string>(loggerNode, "pattern",
															 "[%Y-%m-%d %H:%M:%S.%e][%n][%^%l%$][thread %t]%v");

	// 获取各级别日志是否按照输出格式输出
	mozi::YamlNode showCodeLineNode = mozi::YamlTool::getNode(logConfigNode, "showCodeLine");
	if (showCodeLineNode.isDefined() && !showCodeLineNode.isNull())
	{
		m_traceShowLine = mozi::YamlTool::getDef<bool>(showCodeLineNode, "trace", false);
		m_debugShowLine = mozi::YamlTool::getDef<bool>(showCodeLineNode, "debug", false);
		m_infoShowLine = mozi::YamlTool::getDef<bool>(showCodeLineNode, "info", false);
		m_warnShowLine = mozi::YamlTool::getDef<bool>(showCodeLineNode, "warn", true);
		m_errorShowLine = mozi::YamlTool::getDef<bool>(showCodeLineNode, "error", true);
		m_criticalShowLine = mozi::YamlTool::getDef<bool>(showCodeLineNode, "critical", true);
	}

	mozi::YamlNode sinksNode = mozi::YamlTool::getNode(logConfigNode, "sinks");
	std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks;

	if (!sinksNode.isDefined() || sinksNode.isNull() || !sinksNode.isSequence())
	{
		throw spdlog::spdlog_ex("在日志yaml配置文件中, <Sinks> 节点未定义、为空，或不是序列");
	}
	else
	{
		if (sinksNode.size() == 0)
		{
			auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();// 创建控制台sink
			consoleSink->set_pattern(logPatternStr);
			this->m_logger = std::make_shared<spdlog::logger>("console", consoleSink);
			std::cout << "[LogPrivate] LogPrivate not set Sink, used default: console!" << std::endl;
		}
		else
		{
			for (std::size_t i = 0; i < sinksNode.size(); ++i)
			{
				mozi::YamlNode sinkNode = mozi::YamlTool::getSequenceNode(sinksNode, i);
				if (!sinkNode.isDefined() || sinkNode.isNull())
				{
					std::cout << "[LogPrivate] sinkNode is not exist, index: " + std::to_string(i);
					continue;
				}
				else
				{
					// auto name = Config::YamlTool::getDef<std::string>(sinkNode, "name", "");
					auto type = mozi::YamlTool::getDef<std::string>(sinkNode, "type", "");
					auto sinkLevel = spdlog::level::from_str(mozi::YamlTool::getDef<std::string>(sinkNode, "level", "trace"));

					// 注意，spdlog默认支持两种sink：多线程mt和单线程st，mt虽然性能比st低，但多线程安全，因此默认使用mt，不再使用st
					if (type == SINK_TYPE_STDOUT_COLOR_SINK_MT)// 控制台sink
					{
						auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
						consoleSink->set_level(sinkLevel);
						sinks.push_back(consoleSink);
					}

					else if (type == SINK_TYPE_DAILY_FILE_MT)// 日期分割文件sink
					{
						auto filePath = mozi::YamlTool::getDef<std::string>(sinkNode, "file_path", "");
						if (filePath.empty())
						{
							std::cout << "[LogPrivate] file_path is empty, index: " + std::to_string(i);
							continue;
						}
						int rotationHour = mozi::YamlTool::getDef<int>(sinkNode, "rotation_hour", 0);
						int rotationMin = mozi::YamlTool::getDef<int>(sinkNode, "rotation_min", 0);

						int maxDays = mozi::YamlTool::getDef<int>(sinkNode, "max_days", 0);
						auto truncate = mozi::YamlTool::getDef<bool>(sinkNode, "truncate", false);// 是否清空截断，false则下次打开追加写入

						auto fileSink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(filePath, rotationHour, rotationMin, truncate, maxDays);
						fileSink->set_level(sinkLevel);
						sinks.push_back(fileSink);
					}
					else if (type == SINK_TYPE_ROTATING_FILE_MT)// 滚动文件sink
					{
						auto filePath = mozi::YamlTool::getDef<std::string>(sinkNode, "file_path", "");
						if (filePath.empty())
						{
							std::cout << "[LogPrivate] file_path is empty, index: " + std::to_string(i);
							continue;
						}
						int maxSize = mozi::YamlTool::getDef<int>(sinkNode, "max_size", 52428800) * 8 * 1024;// 单位bit -> KB
						int maxFiles = mozi::YamlTool::getDef<int>(sinkNode, "max_files", 10);
						auto rotateOnOpen = mozi::YamlTool::getDef<bool>(sinkNode, "rotate_on_open", false);// 是否在 logger 初始化时就立刻进行一次滚动
						auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(filePath, maxSize, maxFiles, rotateOnOpen);
						fileSink->set_level(sinkLevel);
						sinks.push_back(fileSink);
					}
					else if (type == SINK_TYPE_BASIC_FILE_SINK_MT)
					{

						auto filePath = mozi::YamlTool::getDef<std::string>(sinkNode, "file_path", "");
						if (filePath.empty())
						{
							std::cout << "[LogPrivate] file_path is empty, index: " + std::to_string(i);
							continue;
						}

						auto truncate = mozi::YamlTool::getDef<bool>(sinkNode, "truncate", false);// 是否清空截断，false则下次打开追加写入
						auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filePath, truncate);
						fileSink->set_level(sinkLevel);
						sinks.push_back(fileSink);
					}
					else if (type == SINK_TYPE_COUNT_ROTATING_FILE_MT)// 按行数滚动的日志文件sink
					{
						auto filePath = mozi::YamlTool::getDef<std::string>(sinkNode, "file_path", "");
						if (filePath.empty())
						{
							std::cout << "[LogPrivate] file_path is empty, index: " + std::to_string(i);
							continue;
						}
						int maxCount = mozi::YamlTool::getDef<int>(sinkNode, "max_count", 100000);
						int maxFiles = mozi::YamlTool::getDef<int>(sinkNode, "max_files", 10);
						auto rotateOnOpen = mozi::YamlTool::getDef<bool>(sinkNode, "rotate_on_open", false);          // 是否在 logger 初始化时就立刻进行一次滚动
						bool strictCountOnOpen = mozi::YamlTool::getDef<bool>(sinkNode, "strict_count_on_open", true);// 追加写入的时候，是否先计算一下当前文件的行数，决定是否立即进行滚动
						auto fileSink = std::make_shared<CustomSink::count_rotating_file_mt<std::mutex>>(filePath, maxCount, maxFiles, rotateOnOpen, strictCountOnOpen);
						fileSink->set_level(sinkLevel);
						sinks.push_back(fileSink);
					}
					else if (type == SINK_TYPE_DAILY_SIZE_ROTATING_FILE_MT)
					{
						auto rootDir = mozi::YamlTool::getDef<std::string>(sinkNode, "root_dir", "");
						if (rootDir.empty())
						{
							std::cout << "[LogPrivate] file_path is empty, index: " + std::to_string(i);
							continue;
						}
						auto name = mozi::YamlTool::getDef<std::string>(sinkNode, "name", "{date}");
						auto dateNameFormat = mozi::YamlTool::getDef<std::string>(sinkNode, "date_name_format", "yyyy-MM-dd");
						int rotationHour = mozi::YamlTool::getDef<int>(sinkNode, "rotation_hour", 0);
						int rotationMin = mozi::YamlTool::getDef<int>(sinkNode, "rotation_min", 0);
						int maxSize = mozi::YamlTool::getDef<int>(sinkNode, "max_size", 52428800) * 8 * 1024;// 单位bit -> KB
						int maxFiles = mozi::YamlTool::getDef<int>(sinkNode, "max_files", 10);
						auto rotateOnOpen = mozi::YamlTool::getDef<bool>(sinkNode, "rotate_on_open", false);// 是否在 logger 初始化时就立刻进行一次滚动
						auto fileSink = std::make_shared<CustomSink::daily_size_rotating_file_mt<std::mutex>>(rootDir,
																											  name,
																											  dateNameFormat,
																											  rotationHour,
																											  rotationMin,
																											  maxSize,
																											  maxFiles,
																											  rotateOnOpen);
						fileSink->set_level(sinkLevel);
						sinks.push_back(fileSink);
					}
					else
					{
						std::cout << "[LogPrivate] sink type is not supported now, index: " + std::to_string(i) << ", type: " << type << std::endl;
					}
				}
			}
		}
		this->m_logger = std::make_shared<spdlog::logger>(loggerName);
		for (const auto& sink: sinks)
		{
			this->m_logger->sinks().push_back(sink);
		}
	}

#ifdef DEBUG
	this->m_logger->set_level(debugLevel);
#else//release模式下，提升日志级别，或关闭日志输出
	this->m_logger->set_level(releaseLevel);
#endif
	this->m_logger->flush_on(flushOn);
	this->m_logger->set_pattern(logPatternStr);

	std::cout << "[LogPrivate] 日志配置文件加载成功，配置文件路径：" << std::filesystem::absolute(configFilePath) << std::endl;
}

void LogPrivate::loadDefaultConfig(const std::string& configFilePath)
{
	// 创建日志及设置名称
	this->m_logger = std::make_shared<spdlog::logger>("log-default");
	// 设置日志级别
#ifdef MZ_LOG_DEBUG//release模式下，提升日志级别，或关闭日志输出
	this->m_logger->set_level(spdlog::level::trace);
#else
	this->m_logger->set_level(spdlog::level::warn);
#endif
	// 设置日志格式
	this->m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e][%n][%^%l%$][thread %t]%v");

	// 设置日志输出是否显示行号
	m_traceShowLine = false;
	m_debugShowLine = false;
	m_infoShowLine = false;
	m_warnShowLine = true;
	m_errorShowLine = true;
	m_criticalShowLine = true;

	//组织配置文件所需的参数并写入配置文件
	std::string loggerName = "default-log";
	std::string debugLevel = "trace";
	std::string releaseLevel = "info";
	std::string flushOn = "trace";
	std::string logPatternStr = "[%Y-%m-%d %H:%M:%S.%e][%n][%^%l%$][thread %t]%v";

	std::string traceShowLine = "false";
	std::string debugShowLine = "false";
	std::string infoShowLine = "false";
	std::string warnShowLine = "true";
	std::string errorShowLine = "true";
	std::string criticalShowLine = "true";

	std::string stdoutColorSinkType = SINK_TYPE_STDOUT_COLOR_SINK_MT;
	std::string stdoutColorSinkLevel = "trace";

	std::string basicSinkType = SINK_TYPE_BASIC_FILE_SINK_MT;
	std::string basicSinkLevel = "trace";
	std::string basicSinkFilePath = "./logs/basic_file_sink_mt.log";
	std::string basicSinkTruncate = "false";

	std::string rotatingSinkType = SINK_TYPE_ROTATING_FILE_MT;
	std::string rotatingSinkLevel = "trace";
	std::string rotatingSinkFilePath = "./logs/rotating_file_mt.log";
	std::string rotatingSinkMaxSize = "10240";
	std::string rotatingSinkMaxFiles = "5";
	std::string rotatingSinkRotateOnOpen = "false";

	std::string dailySinkType = SINK_TYPE_DAILY_FILE_MT;
	std::string dailySinkLevel = "trace";
	std::string dailySinkFilePath = "./logs/daily_file_mt.log";
	std::string dailySinkRotationHour = "0";
	std::string dailySinkRotationMin = "0";
	std::string dailySinkMaxDays = "7";
	std::string dailySinkTruncate = "false";

	std::string countRotatingSinkType = SINK_TYPE_COUNT_ROTATING_FILE_MT;
	std::string countRotatingSinkLevel = "trace";
	std::string countRotatingSinkFilePath = "./logs/count_rotating_file_mt.log";
	std::string countRotatingSinkMaxCount = "100000";
	std::string countRotatingSinkMaxFiles = "5";
	std::string countRotatingSinkRotateOnOpen = "false";
	std::string countRotatingSinkStrictCountOnOpen = "true";

	std::string dailySizeRotatingSinkType = SINK_TYPE_DAILY_SIZE_ROTATING_FILE_MT;
	std::string dailySizeRotatingSinkLevel = "trace";
	std::string dailySizeRotatingSinkRootDir = "./logs";
	std::string dailySizeRotatingSinkName = "{date}";
	std::string dailySizeRotatingSinkDateNameFormat = "yyyy-MM-dd";
	std::string dailySizeRotatingSinkRotationHour = "0";
	std::string dailySizeRotatingSinkRotationMin = "0";
	std::string dailySizeRotatingSinkMaxSize = "10240";
	std::string dailySizeRotatingSinkMaxFiles = "5";
	std::string dailySizeRotatingSinkRotateOnOpen = "false";

	mozi::YamlNode rootNode;
	mozi::YamlNode logConfigNode;
	mozi::YamlNode loggerNode;
	mozi::YamlNode showCodeLineNode;
	mozi::YamlNode sinksNode;

	mozi::YamlTool::setDef<std::string>(loggerNode, "name", loggerName);
	mozi::YamlTool::setDef<std::string>(loggerNode, "debug_level", debugLevel);
	mozi::YamlTool::setDef<std::string>(loggerNode, "release_level", releaseLevel);
	mozi::YamlTool::setDef<std::string>(loggerNode, "flush_on", flushOn);
	mozi::YamlTool::setDef<std::string>(loggerNode, "pattern", logPatternStr);

	mozi::YamlTool::setDef<std::string>(showCodeLineNode, "trace", traceShowLine);
	mozi::YamlTool::setDef<std::string>(showCodeLineNode, "debug", debugShowLine);
	mozi::YamlTool::setDef<std::string>(showCodeLineNode, "info", infoShowLine);
	mozi::YamlTool::setDef<std::string>(showCodeLineNode, "warn", warnShowLine);
	mozi::YamlTool::setDef<std::string>(showCodeLineNode, "error", errorShowLine);
	mozi::YamlTool::setDef<std::string>(showCodeLineNode, "critical", criticalShowLine);

	mozi::YamlNode stdoutColorNode;
	mozi::YamlTool::setDef<std::string>(stdoutColorNode, "type", stdoutColorSinkType);
	mozi::YamlTool::setDef<std::string>(stdoutColorNode, "level", stdoutColorSinkLevel);
	mozi::YamlTool::pushBack(sinksNode, stdoutColorNode);

	mozi::YamlNode basicNode;
	mozi::YamlTool::setDef<std::string>(basicNode, "type", basicSinkType);
	mozi::YamlTool::setDef<std::string>(basicNode, "level", basicSinkLevel);
	mozi::YamlTool::setDef<std::string>(basicNode, "file_path", basicSinkFilePath);
	mozi::YamlTool::setDef<std::string>(basicNode, "truncate", basicSinkTruncate);
	mozi::YamlTool::pushBack(sinksNode, basicNode);

	mozi::YamlNode rotatingNode;
	mozi::YamlTool::setDef<std::string>(rotatingNode, "type", rotatingSinkType);
	mozi::YamlTool::setDef<std::string>(rotatingNode, "level", rotatingSinkLevel);
	mozi::YamlTool::setDef<std::string>(rotatingNode, "file_path", rotatingSinkFilePath);
	mozi::YamlTool::setDef<std::string>(rotatingNode, "max_size", rotatingSinkMaxSize);
	mozi::YamlTool::setDef<std::string>(rotatingNode, "max_files", rotatingSinkMaxFiles);
	mozi::YamlTool::setDef<std::string>(rotatingNode, "rotate_on_open", rotatingSinkRotateOnOpen);
	mozi::YamlTool::pushBack(sinksNode, rotatingNode);

	mozi::YamlNode dailyNode;
	mozi::YamlTool::setDef<std::string>(dailyNode, "type", dailySinkType);
	mozi::YamlTool::setDef<std::string>(dailyNode, "level", dailySinkLevel);
	mozi::YamlTool::setDef<std::string>(dailyNode, "file_path", dailySinkFilePath);
	mozi::YamlTool::setDef<std::string>(dailyNode, "rotation_hour", dailySinkRotationHour);
	mozi::YamlTool::setDef<std::string>(dailyNode, "rotation_min", dailySinkRotationMin);
	mozi::YamlTool::setDef<std::string>(dailyNode, "max_days", dailySinkMaxDays);
	mozi::YamlTool::setDef<std::string>(dailyNode, "truncate", dailySinkTruncate);
	mozi::YamlTool::pushBack(sinksNode, dailyNode);

	mozi::YamlNode countRotatingNode;
	mozi::YamlTool::setDef<std::string>(countRotatingNode, "type", countRotatingSinkType);
	mozi::YamlTool::setDef<std::string>(countRotatingNode, "level", countRotatingSinkLevel);
	mozi::YamlTool::setDef<std::string>(countRotatingNode, "file_path", countRotatingSinkFilePath);
	mozi::YamlTool::setDef<std::string>(countRotatingNode, "max_count", countRotatingSinkMaxCount);
	mozi::YamlTool::setDef<std::string>(countRotatingNode, "max_files", countRotatingSinkMaxFiles);
	mozi::YamlTool::setDef<std::string>(countRotatingNode, "rotate_on_open", countRotatingSinkRotateOnOpen);
	mozi::YamlTool::setDef<std::string>(countRotatingNode, "strict_count_on_open", countRotatingSinkStrictCountOnOpen);
	mozi::YamlTool::pushBack(sinksNode, countRotatingNode);

	mozi::YamlNode dailySizeRotatingNode;
	mozi::YamlTool::setDef<std::string>(dailySizeRotatingNode, "type", dailySizeRotatingSinkType);
	mozi::YamlTool::setDef<std::string>(dailySizeRotatingNode, "level", dailySizeRotatingSinkLevel);
	mozi::YamlTool::setDef<std::string>(dailySizeRotatingNode, "root_dir", dailySizeRotatingSinkRootDir);
	mozi::YamlTool::setDef<std::string>(dailySizeRotatingNode, "name", dailySizeRotatingSinkName);
	mozi::YamlTool::setDef<std::string>(dailySizeRotatingNode, "date_name_format", dailySizeRotatingSinkDateNameFormat);
	mozi::YamlTool::setDef<std::string>(dailySizeRotatingNode, "rotation_hour", dailySizeRotatingSinkRotationHour);
	mozi::YamlTool::setDef<std::string>(dailySizeRotatingNode, "rotation_min", dailySizeRotatingSinkRotationMin);
	mozi::YamlTool::setDef<std::string>(dailySizeRotatingNode, "max_size", dailySizeRotatingSinkMaxSize);
	mozi::YamlTool::setDef<std::string>(dailySizeRotatingNode, "max_files", dailySizeRotatingSinkMaxFiles);
	mozi::YamlTool::setDef<std::string>(dailySizeRotatingNode, "rotate_on_open", dailySizeRotatingSinkRotateOnOpen);
	mozi::YamlTool::pushBack(sinksNode, dailySizeRotatingNode);

	mozi::YamlTool::addNode(logConfigNode, "logger", loggerNode);
	mozi::YamlTool::addNode(logConfigNode, "showCodeLine", showCodeLineNode);
	mozi::YamlTool::addNode(logConfigNode, "sinks", sinksNode);

	mozi::YamlTool::addNode(rootNode, "log_config", logConfigNode);

	try
	{
		mozi::YamlTool::saveAsFile(rootNode, configFilePath);
		m_configFilePath = configFilePath;
		std::cout << "[LogPrivate] 默认日志配置文件完成，配置文件路径：" << std::filesystem::absolute(m_configFilePath) << std::endl;
		this->loadConfigFile(m_configFilePath);
	}
	catch (std::exception& e)
	{
		std::cout << "[LogPrivate] " << e.what();
	}
}
void LogPrivate::deleteOldConfig(const std::string& configFilePath)
{
	if (!configFilePath.empty())// 删除原来的配置文件
	{
		if (std::filesystem::exists(configFilePath))
		{
			if (std::filesystem::remove(configFilePath))
			{
				std::cout << "[LogPrivate] 旧的日志配置文件删除成功: " << std::filesystem::absolute(configFilePath) << std::endl;
			}
			else
			{
				std::cout << "[LogPrivate] 旧的日志配置文件删除失败: " << std::filesystem::absolute(configFilePath) << std::endl;
			}
		}
		else
		{
			std::cout << "[LogPrivate] 旧的日志配置文件不存在: " << std::filesystem::absolute(configFilePath) << std::endl;
		}
	}
}

std::string LogPrivate::getItemValue(const std::string& itemValue, const std::string& itemName,
									 const std::string& defaultValue)
{
	if (itemValue.empty())
	{
		std::cout << "[LogPrivate] " + itemName + " not set! Used default" + defaultValue + "!" << std::endl;
		return defaultValue;
	}

	return itemValue;
}

bool LogPrivate::checkSinkFilePath(const std::string& sinkType, const std::string& filePath)
{
	if (!filePath.empty())
	{
		return true;
	}
	std::cout << "[LogPrivate] sink " + sinkType + "filePath is not set, jumped this sink config!" << std::endl;
	return false;
}


std::string LogPrivate::linkString(const char* fileName, int fileLine, const char* function, const std::initializer_list<std::any>& msgList)
{
	if (msgList.size() == 0)
	{
		return {};
	}

	std::string linkStr;
	linkStr.clear();
	for (const auto& msg: msgList)
	{
		std::string msgStr = anyToString(fileName, fileLine, function, msg);
		// if (msgStr.empty())
		// {
		// 	return {};
		// }
		linkStr += msgStr;
	}
	return linkStr;
}
std::string LogPrivate::floatingNumToString(double value)
{
	if (value == 0.0 || value == -0.0)
	{
		return "0";
	}

	std::ostringstream out;
	int prec = std::numeric_limits<double>::digits10;
	std::string res;
	double tmp = value;
	while (prec >= 7)
	{
		out.clear();
		out.str(std::string());
		out.precision(prec);//覆盖默认精度
		out << tmp;
		std::string str = out.str();//从流中取出字符串
		res = str;
		tmp = std::stod(str);
		prec -= 1;
	}

	std::replace(res.begin(), res.end(), 'e', 'E');
	return res;
}
std::string LogPrivate::anyToString(const char* fileName, int fileLine, const char* function, const std::any& data)
{

	if (!data.has_value())
	{
		return {};
	}
	if (data.type() != typeid(nullptr))
	{
		if (data.type() == typeid(std::string))
		{
			return std::any_cast<std::string>(data);
		}
		if (data.type() == typeid(std::string&))
		{
			return std::any_cast<std::string>(data);
		}
		if (data.type() == typeid(const std::string&))
		{
			return std::any_cast<std::string>(data);
		}
		if (data.type() == typeid(const char*))// 添加对 const char* 的支持
		{
			return std::any_cast<const char*>(data);// 直接返回字符串
		}
		if (data.type() == typeid(long))// 添加对 const char* 的支持
		{
			return std::to_string(std::any_cast<long>(data));
		}
		if (data.type() == typeid(long long))// 添加对 const char* 的支持
		{
			return std::to_string(std::any_cast<long long>(data));
		}
		if (data.type() == typeid(char*))
		{
			return std::any_cast<char*>(data);
		}
		if (data.type() == typeid(int))
		{
			return std::to_string(std::any_cast<int>(data));
		}
		if (data.type() == typeid(unsigned int))
		{
			return std::to_string(std::any_cast<unsigned int>(data));
		}
		if (data.type() == typeid(float))
		{
			return std::to_string(std::any_cast<float>(data));
		}
		if (data.type() == typeid(double))
		{
			return floatingNumToString(std::any_cast<double>(data));
		}
		if (data.type() == typeid(std::wstring))
		{
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
			std::string utf8String = converter.to_bytes(std::any_cast<std::wstring>(data));
			return utf8String;
		}
		if (data.type() == typeid(wchar_t*))
		{
			std::wstring wStr = std::any_cast<wchar_t*>(data);
			std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
			std::string utf8String = converter.to_bytes(wStr);
			return utf8String;
		}
		if (data.type() == typeid(const wchar_t*))
		{
			std::wstring wStr = std::any_cast<const wchar_t*>(data);
			std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
			std::string utf8String = converter.to_bytes(wStr);
			return utf8String;
		}
		if (data.type() == typeid(size_t))
		{
			return std::to_string(std::any_cast<size_t>(data));
		}
		if (data.type() == typeid(bool))
		{
			if (std::any_cast<bool>(data)) return "true";
			return "false";
		}
		if (data.type() == typeid(uint64_t))
		{
			return std::to_string(std::any_cast<uint64_t>(data));
		}
		if (data.type() == typeid(QString))
		{
			return std::any_cast<QString>(data).toStdString();
		}
		if (data.type() == typeid(QColor))
		{
			QColor color = std::any_cast<QColor>(data);
			return QString("rgba(%1, %2, %3,%4)")
					.arg(color.red())
					.arg(color.green())
					.arg(color.blue())
					.arg(color.alpha())
					.toStdString();
			;
		}
		if (data.type() == typeid(QStringList))
		{
			auto strList = std::any_cast<QStringList>(data).toStdList();
			if (strList.empty())
			{
				return {};
			}

			std::string str = "(";
			for (QString i: strList)
			{
				str += "\"" + i.toStdString() + "\",";
			}
			str.replace(str.end() - 1, str.end(), "");
			str += ")";
			return str;
		}
		if (data.type() == typeid(QByteArray))
		{
			return std::any_cast<QByteArray>(data).toStdString();
		}
		if (data.type() == typeid(qint64))
		{
			return std::to_string(std::any_cast<qint64>(data));
		}
		if (data.type() == typeid(QPoint))
		{
			std::string str = "(" + std::to_string(std::any_cast<QPoint>(data).x()) + "," + std::to_string(std::any_cast<QPoint>(data).y()) + ")";
			return str;
		}
		if (data.type() == typeid(QPointF))
		{
			std::string str = "(" + floatingNumToString(std::any_cast<QPointF>(data).x()) + "," + floatingNumToString(std::any_cast<QPointF>(data).y()) + ")";
			return str;
		}

		getInstance()->getLogger()->warn("[{}:{}][{}] anythingToString 类型转换失败", fileName, fileLine, function);// 如果不为空且没有匹配上类型
		return {};
	}
	getInstance()->getLogger()->warn("[{}:{}][{}] anythingToString 入参为空", fileName, fileLine, function);// 如果不为空且没有匹配上类型
	return {};
}