//! Persistent v1 layout shared with the Arduino/ESP32-P4 firmware since
//! v0.11.0. All fields are little endian, independent of host alignment.
use crate::ffi::RawConfig;
use crate::ROOMS;

pub type Record = [u8; 56];
const FORMAT: u32 = 0x4854_0101;
const ABSENT: i16 = i16::MIN;
const CHECKSUM_OFFSET: usize = 52;
const MAX_STEP_MS: u32 = 86_400_000;

pub(super) struct Decoded {
    pub config: RawConfig,
    pub season: u8,
    pub revision: u32,
}

fn checksum(bytes: &[u8]) -> u32 {
    bytes.iter().fold(2_166_136_261, |hash, &byte| {
        (hash ^ u32::from(byte)).wrapping_mul(16_777_619)
    })
}

fn temperature_word(index: usize, value: f32) -> Option<i16> {
    if index >= 3 && value.is_nan() {
        return Some(ABSENT);
    }
    let range = if index < 8 { -55.0..=125.0 } else { 0.0..=180.0 };
    if !range.contains(&value) {
        return None;
    }
    // The v1 writer uses C lroundf (ties away from zero). The bounds above
    // keep every candidate in i16; only values within 0.0001 C of a tenth pass.
    let scaled = value * 10.0;
    let word = if scaled < 0.0 { scaled - 0.5 } else { scaled + 0.5 } as i16;
    ((value - f32::from(word) / 10.0).abs() <= 0.0001).then_some(word)
}

pub(super) fn encode(c: &RawConfig, season: u8, revision: u32) -> Option<Record> {
    if season > 2 || c.reheat > 2 || c.allow_away_diesel > 1
        || c.reheat_step_ms > MAX_STEP_MS || (c.reheat == 2 && c.reheat_step_ms == 0)
        || c.room_for_loop.iter().any(|&id| usize::from(id) > ROOMS)
    {
        return None;
    }
    // Optional thresholds are NaN until configured. The ordered comparisons
    // below require the pair to be ordered only when both ends are present.
    if !(c.room_on_c < c.room_radiator_off_c && c.room_radiator_off_c < c.room_off_c)
        || c.return_restart_c >= c.return_target_c || c.tank_on_c >= c.tank_off_c
        || c.coil_start_delta_c <= c.coil_stop_delta_c
    {
        return None;
    }
    let mut record = [0; 56];
    record[..4].copy_from_slice(&FORMAT.to_le_bytes());
    record[4..8].copy_from_slice(&revision.to_le_bytes());
    record[8..12].copy_from_slice(&c.reheat_step_ms.to_le_bytes());
    let temps = [
        c.room_on_c, c.room_radiator_off_c, c.room_off_c, c.supply_target_c, c.return_target_c,
        c.return_restart_c, c.tank_on_c, c.tank_off_c, c.coil_start_delta_c, c.coil_stop_delta_c,
    ];
    for (index, (dst, value)) in record[12..32].chunks_exact_mut(2).zip(temps).enumerate() {
        dst.copy_from_slice(&temperature_word(index, value)?.to_le_bytes());
    }
    record[32..46].copy_from_slice(&c.room_for_loop);
    record[46] = season;
    record[47] = c.reheat;
    record[48] = c.allow_away_diesel;
    let hash = checksum(&record[..CHECKSUM_OFFSET]);
    record[CHECKSUM_OFFSET..].copy_from_slice(&hash.to_le_bytes());
    Some(record)
}

pub(super) fn decode(record: &Record) -> Option<Decoded> {
    let revision = u32::from_le_bytes(record[4..8].try_into().ok()?);
    if record[..4] != FORMAT.to_le_bytes() || revision == 0 {
        return None;
    }
    let mut temps = [0.0; 10];
    for (value, src) in temps.iter_mut().zip(record[12..32].chunks_exact(2)) {
        let word = i16::from_le_bytes([src[0], src[1]]);
        *value = if word == ABSENT { f32::NAN } else { f32::from(word) / 10.0 };
    }
    let config = RawConfig {
        room_for_loop: record[32..46].try_into().ok()?,
        allow_away_diesel: record[48],
        reheat: record[47],
        room_on_c: temps[0],
        room_radiator_off_c: temps[1],
        room_off_c: temps[2],
        supply_target_c: temps[3],
        return_target_c: temps[4],
        return_restart_c: temps[5],
        tank_on_c: temps[6],
        tank_off_c: temps[7],
        coil_start_delta_c: temps[8],
        coil_stop_delta_c: temps[9],
        reheat_step_ms: u32::from_le_bytes(record[8..12].try_into().ok()?),
    };
    let season = record[46];
    // Re-encoding checks field validity, all reserved bytes and the checksum.
    if encode(&config, season, revision)? != *record {
        return None;
    }
    Some(Decoded { config, season, revision })
}
