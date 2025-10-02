use std::fmt;

use chrono::Utc;

const RESET: &str = "\x1b[0m";
const GREEN: &str = "\x1b[92m";
const RED: &str = "\x1b[91m";
const YELLOW: &str = "\x1b[93m";
const BLUE: &str = "\x1b[94m";

#[derive(Copy, Clone)]
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
    fn as_color(&self) -> &'static str {
        match self {
            LogLevel::Info => GREEN,
            LogLevel::Warning => YELLOW,
            LogLevel::Error => RED,
            LogLevel::Debug => BLUE,
        }
    }
}

pub struct Logger {
    name: Option<&'static str>,
}

impl Logger {
    pub fn new(name: Option<&'static str>) -> Self {
        Self { name }
    }

    fn log(&self, level: LogLevel, msg: &str, args: Option<&[&dyn fmt::Debug]>) -> () {
        let timestamp = Utc::now().format("%Y-%m-%d %H:%M:%S");

        let name = self.name.unwrap_or("sst-cam");

        let extras = match args {
            Some(xs) if !xs.is_empty() => format!(" {:?}", xs),
            _ => String::new(),
        };

        let lv_color = level.as_color();
        let lv_str = level.as_str();

        let formated_msg = format!("{timestamp} {lv_color}[{name}] {lv_str}{RESET}: {msg}{extras}");

        if matches!(level, LogLevel::Error) {
            eprintln!("{formated_msg}");
        } else {
            println!("{formated_msg}");
        }
    }

    pub fn info(&self, msg: &str, args: Option<&[&dyn fmt::Debug]>) {
        self.log(LogLevel::Info, msg, args);
    }
    pub fn warning(&self, msg: &str, args: Option<&[&dyn fmt::Debug]>) {
        self.log(LogLevel::Warning, msg, args);
    }
    pub fn error(&self, msg: &str, args: Option<&[&dyn fmt::Debug]>) {
        self.log(LogLevel::Error, msg, args);
    }
    pub fn debug(&self, msg: &str, args: Option<&[&dyn fmt::Debug]>) {
        self.log(LogLevel::Debug, msg, args);
    }
}
