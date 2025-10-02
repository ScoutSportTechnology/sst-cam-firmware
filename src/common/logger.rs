use std::fmt;

const RESET: &str = "\x1b[0m";
const GREEN: &str = "\x1b[92m";
const RED: &str = "\x1b[91m";
const YELLOW: &str = "\x1b[93m";
const BLUE: &str = "\x1b[94m";

enum LogLevel {
    Info,
    Warning,
    Error,
    Debug,
}

impl LogLevel {
    fn as_str(&self) -> &'static str {
        match self {
            LogLevel::Info => "INFO",
            LogLevel::Warning => "WARNING",
            LogLevel::Error => "ERROR",
            LogLevel::Debug => "DEBUG",
        }
    }
    fn color(&self) -> &'static str {
        match self {
            LogLevel::Info => GREEN,
            LogLevel::Warning => YELLOW,
            LogLevel::Error => RED,
            LogLevel::Debug => BLUE,
        }
    }
}

pub struct Logger {
    name: Option<String>,
}

impl Logger {
    pub fn new(name: Option<String>) -> Self {
        Self {
            name,
        }
    }

    fn format_msg(&self, level: LogLevel, msg: &str, args: &[impl fmt::Display]) -> String {
        let args_str = if args.is_empty() {
            "".to_string()
        } else {
            let joined = args
                .iter()
                .map(|a| a.to_string())
                .collect::<Vec<_>>()
                .join(" ");
            format!(" {}", joined)
        };

        let name_str = self
            .name
            .as_ref()
            .map(|n| format!("[{}] ", n))
            .unwrap_or_default();

        format!(
            "{}{}{}: {}{}",
            level.color(),
            level.as_str(),
            RESET,
            name_str,
            msg
        ) + &args_str
    }

    fn log(&self, level: LogLevel, msg: &str, args: &[impl fmt::Display]) {
        println!("{}", self.format_msg(level, msg, args));
    }

    pub fn info(&self, msg: &str, args: &[impl fmt::Display]) {
        self.log(LogLevel::Info, msg, args);
    }

    pub fn error(&self, msg: &str, args: &[impl fmt::Display]) {
        self.log(LogLevel::Error, msg, args);
    }

    pub fn warning(&self, msg: &str, args: &[impl fmt::Display]) {
        self.log(LogLevel::Warning, msg, args);
    }

    pub fn debug(&self, msg: &str, args: &[impl fmt::Display]) {
        self.log(LogLevel::Debug, msg, args);
    }
}
