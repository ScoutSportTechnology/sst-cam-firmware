#include "logger.hpp"

#include <iostream>

class Logger {
    std::optional<std::string> name_;
    static constexpr const char* level_str(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:
                return "DEBUG";
            case LogLevel::INFO:
                return "INFO";
            case LogLevel::WARNING:
                return "WARNING";
            case LogLevel::ERROR:
                return "ERROR";
            default:
                return "";
        }
    }
    static constexpr const char* level_color(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:
                return BLUE;
            case LogLevel::INFO:
                return GREEN;
            case LogLevel::WARNING:
                return YELLOW;
            case LogLevel::ERROR:
                return RED;
            default:
                return RESET;
        }
    }

    template <typename... Args>
    void log(LogLevel lvl, std::string_view msg, Args&&... args) const {
        std::cout << level_color(lvl) << level_str(lvl) << RESET << ": ";
        if (name_) std::cout << "[" << *name_ << "] ";
        std::cout << msg;
        ((std::cout << " " << std::forward<Args>(args)), ...);
        std::cout << std::endl;
    }

   public:
    template <typename... Args>
    void debug(std::string_view msg, Args&&... args) const { log(LogLevel::DEBUG, msg, std::forward<Args>(args)...); }
    template <typename... Args>
    void info(std::string_view msg, Args&&... args) const { log(LogLevel::INFO, msg, std::forward<Args>(args)...); }
    template <typename... Args>
    void warning(std::string_view msg, Args&&... args) const { log(LogLevel::WARNING, msg, std::forward<Args>(args)...); }
    template <typename... Args>
    void error(std::string_view msg, Args&&... args) const { log(LogLevel::ERROR, msg, std::forward<Args>(args)...); }
};