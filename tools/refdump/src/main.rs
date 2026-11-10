//! Prints the Rust core's playback plan of a MIDI file as "microseconds bytes…",
//! the same lines as `XGPlay <file> DUMP`; `refdump --xws2mid in.xws out.mid`
//! writes the core's .XWS -> MIDI export (export_named) for the C comparison.
fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() == 4 && args[1] == "--xws2mid" {
        let data = std::fs::read(&args[2]).expect("read");
        std::fs::write(&args[3], xgworks_core::export_named(&data)).expect("write");
        return;
    }
    let path = args.get(1).expect("refdump <file.mid>");
    let data = std::fs::read(path).expect("read");
    for (ms, m) in xgworks_core::playback_plan(&data) {
        let hex: Vec<String> = m.iter().map(|b| format!("{b:02X}")).collect();
        println!("{} {}", (ms * 1000.0).round() as u64, hex.join(" "));
    }
}
