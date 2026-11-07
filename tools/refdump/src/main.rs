//! Prints the Rust core's playback plan of a MIDI file as "microseconds bytes…",
//! the same lines as `XGPlay <file> DUMP`.
fn main() {
    let path = std::env::args().nth(1).expect("refdump <file.mid>");
    let data = std::fs::read(&path).expect("read");
    for (ms, m) in xgworks_core::playback_plan(&data) {
        let hex: Vec<String> = m.iter().map(|b| format!("{b:02X}")).collect();
        println!("{} {}", (ms * 1000.0).round() as u64, hex.join(" "));
    }
}
