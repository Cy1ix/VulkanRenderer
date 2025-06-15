#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/async.h>
#include <spdlog/fmt/ostr.h>

enum class LogLevel
{
	INFO = spdlog::level::info,
	WARN = spdlog::level::warn,
	ERR = spdlog::level::err,
};

class Logger {
public:

	static Logger& getInstance() {
		static Logger instance;
		return instance;
	}

	bool init(const std::string& loggerName = "logger",
		const std::string& logFile = "logs/app.log",
		LogLevel logLevel = LogLevel::INFO,
		size_t maxFileSize = 10,
		size_t maxFiles = 5,
		bool consoleOutput = true,
		bool asyncMode = true) {
		try {
			if (asyncMode) {
				spdlog::init_thread_pool(8192, 1);
			}

			std::vector<spdlog::sink_ptr> sinks;

			auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
				logFile, maxFileSize * 1024 * 1024, maxFiles);
			sinks.push_back(rotating_sink);

			if (consoleOutput) {
				auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
				console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%s:%#] %v");
				sinks.push_back(console_sink);
			}

			if (asyncMode) {
				m_logger = std::make_shared<spdlog::async_logger>(
					loggerName, sinks.begin(), sinks.end(),
					spdlog::thread_pool(), spdlog::async_overflow_policy::block);
			}
			else {
				m_logger = std::make_shared<spdlog::logger>(loggerName, sinks.begin(), sinks.end());
			}

			m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%s:%#] %v");

			m_logger->set_level(static_cast<spdlog::level::level_enum>(logLevel));

			m_logger->flush_on(spdlog::level::trace);

			spdlog::set_default_logger(m_logger);

			m_logger->info("Logger Initailized");
			return true;
		}
		catch (const spdlog::spdlog_ex& ex) {
			std::cerr << "Logger Initailize fail: " << ex.what() << std::endl;
			return false;
		}
	}

	void setLevel(LogLevel level) {
		if (m_logger) {
			m_logger->set_level(static_cast<spdlog::level::level_enum>(level));
		}
	}

	std::shared_ptr<spdlog::logger> getLogger() const {
		return m_logger;
	}

	void shutdown() {
		if (m_logger) {
			m_logger->info("Logger shutdown");
			spdlog::shutdown();
		}
	}

private:
	Logger() = default;
	Logger(const Logger&) = delete;
	Logger(Logger&&) = delete;
	Logger& operator=(const Logger&) = delete;
	Logger& operator=(Logger&&) = delete;

	std::shared_ptr<spdlog::logger> m_logger;
};


#define LOGI(...) SPDLOG_LOGGER_INFO(Logger::getInstance().getLogger(), __VA_ARGS__)
#define LOGW(...) SPDLOG_LOGGER_WARN(Logger::getInstance().getLogger(), __VA_ARGS__)
#define LOGE(...) SPDLOG_LOGGER_ERROR(Logger::getInstance().getLogger(), __VA_ARGS__)
