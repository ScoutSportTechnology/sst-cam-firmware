#include <optional>
#include <string>
#include <string_view>
#pragma once

#ifndef LOGGER_HPP
#define LOGGER_HPP

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
};

inline constexpr const char* RESET = "\033[0m";
inline constexpr const char* RED = "\033[31m";
inline constexpr const char* GREEN = "\033[32m";
inline constexpr const char* YELLOW = "\033[33m";
inline constexpr const char* BLUE = "\033[34m";

class Logger {
   private:
    std::optional<std::string> name_;
    static constexpr const char* level_str(LogLevel level);
    static constexpr const char* level_color(LogLevel level);
    template <typename... Args>
    void log(LogLevel lvl, std::string_view msg, Args&&... args) const;

   public:
    template <typename... Args>
    void debug(std::string_view msg, Args&&... args) const;
    template <typename... Args>
    void info(std::string_view msg, Args&&... args) const;
    template <typename... Args>
    void warning(std::string_view msg, Args&&... args) const;
    template <typename... Args>
    void error(std::string_view msg, Args&&... args) const;
};

#endif