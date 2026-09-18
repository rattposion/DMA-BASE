#pragma once

#ifdef ERROR
    #undef ERROR
    enum LogLevel {
        INFO,
        WARNING,
        ERROR,
    };
#endif

namespace AnsiColours {
    constexpr std::string_view radicalRed = "\x1b[38;2;242;46;89m";
    constexpr std::string_view outerSpace = "\x1b[38;2;70;66;84m";
    constexpr std::string_view royalOrange = "\x1b[38;2;245;152;71m";
    constexpr std::string_view independence = "\x1b[38;2;81;77;97m";
    constexpr std::string_view malachiteGreen = "\x1b[38;2;71;245;135m";
    constexpr std::string_view reset = "\x1b[0m";
}

class Log {
public:
    Log() {
        HANDLE stdOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdOutputHandle) {
            DWORD currentConsoleMode = 0;
            if (GetConsoleMode(stdOutputHandle, &currentConsoleMode)) {
                currentConsoleMode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(stdOutputHandle, currentConsoleMode);
            }
        }
    }

    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;

    template<typename... Args>
    void log(const std::format_string<Args...> fmt, Args&&... args) {
        logInternal(LogLevel::INFO, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void log(LogLevel logLevel, const std::format_string<Args...> fmt, Args&&... args) {
        logInternal(logLevel, fmt, std::forward<Args>(args)...);
    }

private:
    std::mutex m_logMutex;

    struct LogLevelInfo {
        std::string_view info;
        std::string_view colour;
    };

    LogLevelInfo getLogLevelInfo(LogLevel logLevel) {
        switch (logLevel) {
            case (LogLevel::INFO): {
                return {
                    "INFO",
                    AnsiColours::malachiteGreen
                };
            }
            case (LogLevel::WARNING): {
                return {
                    "WARNING",
                    AnsiColours::royalOrange
                };
            }
            case (LogLevel::ERROR): {
                return {
                    "ERROR",
                    AnsiColours::radicalRed
                };
            }
        }
        return { "", "" };
    }

    std::string getCurrentTime() {
        auto const now = std::chrono::system_clock::now();
        return std::format("{:%F %T}", now);
    }

    template<typename... Args>
    void logInternal(LogLevel logLevel, const std::format_string<Args...> fmt, Args&&... args) {
        std::lock_guard<std::mutex> lock(m_logMutex);

        const auto currentTime = getCurrentTime();
        const std::string formattedMessage = std::vformat(fmt.get(), std::make_format_args(args...));

        const auto logLevelInfo = getLogLevelInfo(logLevel);
        const auto paddedLogLevelInfo = std::format("{:<7}", logLevelInfo.info);

        std::println(
            "{}[{}{}{}]{} {}{}{} {}", // [TIME] Level Message
            AnsiColours::outerSpace,
            AnsiColours::independence,
            currentTime,
            AnsiColours::outerSpace,
            AnsiColours::reset,
            logLevelInfo.colour,
            paddedLogLevelInfo,
            AnsiColours::reset,
            formattedMessage
        );
    }
};

extern Log Logger;
