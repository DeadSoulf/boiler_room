//! Fixed-width interface shared with the C++ adapter. Numeric tags from C
//! are decoded before entering the state machine; a foreign byte is never
//! treated as a Rust enum discriminant.
use core::ffi::c_void;
use core::mem::{align_of, size_of};

use super::{Config, Controller, Flag, Health, Inputs, Output, Presence, Reheat, Season, LOOPS, ROOMS};

#[repr(C)]
pub struct RawConfig {
    pub(super) room_for_loop: [u8; LOOPS],
    pub(super) allow_away_diesel: u8,
    pub(super) reheat: u8,
    pub(super) room_on_c: f32,
    pub(super) room_radiator_off_c: f32,
    pub(super) room_off_c: f32,
    pub(super) supply_target_c: f32,
    pub(super) return_target_c: f32,
    pub(super) return_restart_c: f32,
    pub(super) tank_on_c: f32,
    pub(super) tank_off_c: f32,
    pub(super) coil_start_delta_c: f32,
    pub(super) coil_stop_delta_c: f32,
    pub(super) reheat_step_ms: u32,
}

#[repr(C)]
pub struct RawInputs {
    room_c: [f32; ROOMS],
    supply_c: f32,
    return_c: f32,
    tank_c: f32,
    coil_supply_c: f32,
    presence: u8,
    season: u8,
    diesel: u8,
    electric: u8,
    circulation: u8,
    plant_heat_available: u8,
}

#[repr(C)]
pub struct RawOutput {
    loops: [u8; LOOPS],
    demand: u8,
    alarm: u8,
    tank_power: u8,
    coil_pump: u8,
    source: u8,
    stages: i8,
}

const _: () = {
    assert!(size_of::<Controller>() <= 128 && align_of::<Controller>() <= 4);
    assert!(!core::mem::needs_drop::<Controller>());
    assert!(size_of::<RawConfig>() == 60 && align_of::<RawConfig>() == 4);
    assert!(size_of::<RawInputs>() == 80 && align_of::<RawInputs>() == 4);
    assert!(size_of::<RawOutput>() == 20 && align_of::<RawOutput>() == 1);
};

impl From<&RawConfig> for Config {
    fn from(raw: &RawConfig) -> Self {
        Self {
            room_for_loop: raw.room_for_loop,
            allow_away_diesel: raw.allow_away_diesel == 1,
            reheat: match raw.reheat {
                1 => Reheat::All,
                2 => Reheat::OneByOne,
                _ => Reheat::Unset,
            },
            room_on_c: raw.room_on_c,
            room_radiator_off_c: raw.room_radiator_off_c,
            room_off_c: raw.room_off_c,
            supply_target_c: raw.supply_target_c,
            return_target_c: raw.return_target_c,
            return_restart_c: raw.return_restart_c,
            tank_on_c: raw.tank_on_c,
            tank_off_c: raw.tank_off_c,
            coil_start_delta_c: raw.coil_start_delta_c,
            coil_stop_delta_c: raw.coil_stop_delta_c,
            reheat_step_ms: raw.reheat_step_ms,
        }
    }
}

fn health(byte: u8) -> Health {
    match byte {
        1 => Health::Ready,
        2 => Health::Fault,
        _ => Health::Unknown,
    }
}

fn flag(byte: u8) -> Flag {
    match byte {
        1 => Flag::No,
        2 => Flag::Yes,
        _ => Flag::Unknown,
    }
}

impl From<&RawInputs> for Inputs {
    fn from(raw: &RawInputs) -> Self {
        Self {
            room_c: raw.room_c,
            supply_c: raw.supply_c,
            return_c: raw.return_c,
            tank_c: raw.tank_c,
            coil_supply_c: raw.coil_supply_c,
            presence: match raw.presence {
                1 => Presence::Home,
                2 => Presence::Away,
                _ => Presence::Unknown,
            },
            season: match raw.season {
                1 => Season::Summer,
                2 => Season::Winter,
                _ => Season::Unknown,
            },
            diesel: health(raw.diesel),
            electric: health(raw.electric),
            circulation: flag(raw.circulation),
            plant_heat_available: flag(raw.plant_heat_available),
        }
    }
}

impl From<Output> for RawOutput {
    fn from(out: Output) -> Self {
        Self {
            loops: out.loops.map(|flag| flag as u8),
            demand: out.demand as u8,
            alarm: out.alarm as u8,
            tank_power: out.tank_power as u8,
            coil_pump: out.coil_pump as u8,
            source: out.source as u8,
            stages: out.stages,
        }
    }
}

/// Initialize or reset an instance in caller-owned storage.
///
/// # Safety
/// `state` must point to 128 writable bytes, aligned to at least 4 bytes.
/// The caller must serialize all accesses to this instance.
#[no_mangle]
pub unsafe extern "C" fn boiler_heating_init(state: *mut c_void) {
    // SAFETY: the caller provides exclusive storage with the size and alignment checked above.
    unsafe { state.cast::<Controller>().write(Controller::default()) };
}

/// Advance one controller from the current settings and fresh inputs.
///
/// # Safety
/// All pointers must be nonnull, aligned, and refer to non-overlapping objects of
/// the stated sizes for this call. The controller must have been initialized by
/// `boiler_heating_init` and be accessed exclusively. Input fields must be
/// initialized and the output storage must be writable.
#[no_mangle]
pub unsafe extern "C" fn boiler_heating_tick(
    state: *mut c_void,
    now: u32,
    config: *const RawConfig,
    input: *const RawInputs,
    output: *mut RawOutput,
) {
    // SAFETY: controller lifetime, exclusive access and input validity are the caller's contract.
    let (controller, config, input) = unsafe { (&mut *state.cast::<Controller>(), &*config, &*input) };
    let decision = controller.tick(now, &Config::from(config), &Inputs::from(input));
    // SAFETY: the output contains only bytes and the caller supplied non-overlapping writable storage.
    unsafe { output.write(decision.into()) };
}

/// Encode and validate one record for the v1 NVS settings key.
///
/// # Safety
/// All pointers must be valid, aligned and non-overlapping. `output` must
/// have space for one 56-byte record. It is written only on success.
#[no_mangle]
pub unsafe extern "C" fn boiler_heating_settings_encode(
    config: *const RawConfig,
    season: u8,
    revision: u32,
    output: *mut super::settings::Record,
) -> u8 {
    // SAFETY: the caller supplies a valid initialized RawConfig.
    if let Some(record) = super::settings::encode(unsafe { &*config }, season, revision) {
        // SAFETY: the caller provides exclusive space for the entire record.
        unsafe { output.write(record) };
        1
    } else {
        0
    }
}

/// Decode a saved v1 NVS record. Failed reads leave all outputs intact.
///
/// # Safety
/// `record` must point to 56 initialized bytes. All four pointers must be
/// valid, aligned and non-overlapping, with exclusive access to the outputs.
#[no_mangle]
pub unsafe extern "C" fn boiler_heating_settings_decode(
    record: *const super::settings::Record,
    config: *mut RawConfig,
    season: *mut u8,
    revision: *mut u32,
) -> u8 {
    // SAFETY: the caller guarantees a readable record of the exact size.
    let Some(decoded) = super::settings::decode(unsafe { &*record }) else { return 0 };
    // SAFETY: all output locations are writable and distinct from each other and the input.
    unsafe {
        config.write(decoded.config);
        season.write(decoded.season);
        revision.write(decoded.revision);
    }
    1
}
