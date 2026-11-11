//! Reference outputs of the Rust core (XGWorks NextGen) for the C tests:
//!   refdump <file.mid>                      the playback plan, "microseconds bytes…"
//!   refdump --xws2mid in.xws out.mid        the .XWS -> MIDI export (export_named)
//!   refdump --job <name> in.mid out.mid     a Job on every track over the whole song
use xgworks_core::jobs::{self, CtrlKind, Modify, QntzOpts, Scope};
use xgworks_core::listev::TimeMap;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.len() == 4 && args[1] == "--xws2mid" {
        let data = std::fs::read(&args[2]).expect("read");
        std::fs::write(&args[3], xgworks_core::export_named(&data)).expect("write");
        return;
    }
    if args.len() == 4 && args[1] == "--toxws" {
        let data = std::fs::read(&args[2]).expect("read");
        let (tracks, ppqn) = xgworks_core::edit_model(&data);
        std::fs::write(&args[3], xgworks_core::write_xws(&tracks, ppqn)).expect("write");
        return;
    }
    if args.len() == 5 && args[1] == "--job" {
        let data = std::fs::read(&args[3]).expect("read");
        let (mut tracks, ppqn) = xgworks_core::edit_model(&data);
        let sc = Scope { spans: (0..tracks.len()).map(|t| (t, 0, u32::MAX)).collect(), note_lo: 0, note_hi: 127 };
        let tm = TimeMap::new(&tracks, ppqn);
        let bar3 = tm.from_mbc(3, 1, 0);
        let bar5 = tm.from_mbc(5, 1, 0);
        match args[2].as_str() {
            "transpose" => { jobs::transpose(&mut tracks, &sc, 5); }
            "velocity" => { jobs::velocity(&mut tracks, &sc, Modify::Relative { rate: 80, offset: 10 }); }
            "gate" => { jobs::gate_time(&mut tracks, &sc, Modify::Absolute(100)); }
            "shift" => { jobs::shift_clock(&mut tracks, &sc, 37); }
            "sort" => jobs::sort_chord(&mut tracks, &sc, true),
            "separate" => { jobs::sort_chord(&mut tracks, &sc, false); jobs::separate_chord(&mut tracks, &sc, 10) }
            "control" => { jobs::modify_control(&mut tracks, &sc, CtrlKind::Cc(7, 11), Modify::Relative { rate: 50, offset: 0 }); }
            "bend" => { jobs::modify_control(&mut tracks, &sc, CtrlKind::PitchBend, Modify::Relative { rate: 50, offset: 100 }); }
            "thin" => { jobs::thin_out(&mut tracks, &sc, CtrlKind::Cc(7, 7)); }
            "quantize" => {
                let o = QntzOpts { grid: 2, strength: 80, sens: 100, swing: 60, gate: 90, vel: 110 };
                jobs::quantize(&mut tracks, &sc, o, ppqn as u32, |t| (tm.from_mbc(tm.to_mbc(t).0, 1, 0), tm.bar_len(t)));
            }
            "stretch" => { jobs::stretch_time(&mut tracks, &sc, 150, 10); }
            "create" => {
                let c = tracks.iter().position(|t| t.conductor).unwrap_or(0);
                jobs::create_measure(&mut tracks, c, ppqn as u32, bar3, 3, 4, 2, (4, 4));
            }
            "delete" => jobs::delete_measure(&mut tracks, bar3, bar5),
            "chord" => { jobs::transpose_chord(&mut tracks, 0, u32::MAX, 3); }
            j => panic!("no job {j}"),
        }
        std::fs::write(&args[4], xgworks_core::write_mid(&tracks, ppqn)).expect("write");
        return;
    }
    let path = args.get(1).expect("refdump <file.mid>");
    let data = std::fs::read(path).expect("read");
    for (ms, m) in xgworks_core::playback_plan(&data) {
        let hex: Vec<String> = m.iter().map(|b| format!("{b:02X}")).collect();
        println!("{} {}", (ms * 1000.0).round() as u64, hex.join(" "));
    }
}
