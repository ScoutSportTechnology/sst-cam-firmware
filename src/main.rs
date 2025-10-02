mod common;
use common::logger::Logger;
fn main() {
    let log = Logger::new(Some("sst-cam"));
    log.info("Logger info message", Some(&{[&123, &123]}));
    log.error("Logger error message", None);
    log.warning("Logger warning message", None);
    log.debug("Logger debug message", None);
}
