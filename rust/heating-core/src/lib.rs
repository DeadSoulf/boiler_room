//! Stateful calculation of heating requests. The caller supplies fresh measurements
//! and owns each controller instance. This crate does not perform hardware I/O.
#![cfg_attr(target_os = "none", no_std)]

mod ffi;

const ROOMS: usize = 14;
const LOOPS: usize = 14;
const FLOOR_LOOPS: usize = 8;
const FLOOR_MASK: u16 = (1 << FLOOR_LOOPS) - 1;
const SHED_INTERVAL_MS: u32 = 5 * 60 * 1_000;

// ESP-IDF provides abort() and the board's configured panic/reset handling.
#[cfg(target_os = "none")]
#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! {
    extern "C" {
        fn abort() -> !;
    }
    // SAFETY: ESP-IDF's C abort() takes no arguments and never returns.
    unsafe { abort() }
}

#[derive(Clone, Copy, Default, PartialEq, Eq)]
#[repr(u8)]
enum Flag {
    #[default]
    Unknown = 0,
    No = 1,
    Yes = 2,
}

#[derive(Clone, Copy, Default, PartialEq, Eq)]
enum Presence {
    #[default]
    Unknown,
    Home,
    Away,
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Season {
    Unknown,
    Summer,
    Winter,
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Health {
    Unknown,
    Ready,
    Fault,
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum Reheat {
    Unset,
    All,
    OneByOne,
}

#[derive(Clone, Copy, Default, PartialEq, Eq)]
#[repr(u8)]
enum Source {
    #[default]
    Unknown = 0,
    None = 1,
    Diesel = 2,
    Electric = 3,
}

struct Config {
    room_for_loop: [u8; LOOPS],
    room_on_c: f32,
    room_radiator_off_c: f32,
    room_off_c: f32,
    supply_target_c: f32,
    return_target_c: f32,
    return_restart_c: f32,
    tank_on_c: f32,
    tank_off_c: f32,
    coil_start_delta_c: f32,
    coil_stop_delta_c: f32,
    allow_away_diesel: bool,
    reheat: Reheat,
    reheat_step_ms: u32,
}

struct Inputs {
    room_c: [f32; ROOMS],
    supply_c: f32,
    return_c: f32,
    tank_c: f32,
    coil_supply_c: f32,
    presence: Presence,
    season: Season,
    diesel: Health,
    electric: Health,
    circulation: Flag,
    plant_heat_available: Flag,
}

struct Output {
    loops: [Flag; LOOPS],
    demand: Flag,
    alarm: Flag,
    tank_power: Flag,
    coil_pump: Flag,
    source: Source,
    stages: i8,
}

#[derive(Clone, Copy, Default)]
struct Room {
    loops: u16,
    floor: Flag,
    radiator: Flag,
}

impl Room {
    fn set_both(&mut self, state: Flag) {
        self.floor = state;
        self.radiator = state;
    }
}

#[derive(Default)]
struct ElectricStages {
    active: i8,
    return_satisfied: bool,
    last_shed: Option<u32>,
    reheat_start: Option<u32>,
}

impl ElectricStages {
    fn idle(&mut self) {
        self.active = 0;
        self.last_shed = None;
        self.reheat_start = None;
    }

    fn tick(&mut self, now: u32, c: &Config, input: &Inputs, source: Source) -> i8 {
        let return_known = input.return_c.is_finite() && c.return_target_c.is_finite();
        if return_known && input.return_c >= c.return_target_c {
            self.return_satisfied = true;
            self.idle();
            return 0;
        }
        if matches!(source, Source::None | Source::Diesel) {
            self.idle();
            return 0;
        }
        if source != Source::Electric || input.circulation != Flag::Yes || !return_known
            || !input.supply_c.is_finite() || !c.supply_target_c.is_finite()
        {
            // Restart a running shed interval, preserving the existing stage count.
            if self.last_shed.is_some() {
                self.last_shed = Some(now);
            }
            self.reheat_start = None;
            return if input.circulation == Flag::No { 0 } else { -1 };
        }

        let hot_supply = input.supply_c >= c.supply_target_c;
        let has_restart = ordered(c.return_restart_c, c.return_target_c);
        let low_return = has_restart && input.return_c <= c.return_restart_c;
        let timed_rise = c.reheat == Reheat::OneByOne
            && (1..=i32::MAX as u32).contains(&c.reheat_step_ms);
        if self.active == 0 {
            if hot_supply || (has_restart && !low_return) {
                return 0;
            }
            if self.return_satisfied {
                if !has_restart || (c.reheat != Reheat::All && !timed_rise) {
                    return -1;
                }
                self.active = if timed_rise { 1 } else { 3 };
                self.return_satisfied = false;
                self.reheat_start = timed_rise.then_some(now);
            } else {
                self.active = 3;
            }
        } else if hot_supply {
            self.reheat_start = None;
            let shed_due = self.last_shed.is_none_or(|t| now.wrapping_sub(t) >= SHED_INTERVAL_MS);
            if self.active > 1 && shed_due {
                self.active -= 1;
                self.last_shed = Some(now);
            }
        } else if self.active < 3 && low_return && c.reheat == Reheat::All {
            self.active = 3;
            self.reheat_start = None;
        } else if self.active < 3 && low_return && timed_rise {
            let start = self.reheat_start.get_or_insert(now);
            if now.wrapping_sub(*start) >= c.reheat_step_ms {
                self.active += 1;
                self.reheat_start = Some(now);
            }
        } else {
            self.reheat_start = None;
        }
        self.active
    }
}

struct Tank {
    thresholds: [f32; 4],
    demand: Flag,
    coil_hot: Flag,
}

impl Default for Tank {
    fn default() -> Self {
        Self {
            thresholds: [f32::NAN; 4],
            demand: Flag::Unknown,
            coil_hot: Flag::Unknown,
        }
    }
}

impl Tank {
    fn pump(&mut self, c: &Config, input: &Inputs, power: Flag) -> Flag {
        let thresholds = [c.tank_on_c, c.tank_off_c, c.coil_start_delta_c, c.coil_stop_delta_c];
        if self.thresholds != thresholds || power != Flag::Yes || input.season != Season::Winter {
            self.demand = Flag::Unknown;
            self.coil_hot = Flag::Unknown;
        }
        self.thresholds = thresholds;
        if power == Flag::No || input.season == Season::Summer {
            return Flag::No;
        }
        if power == Flag::Unknown || input.season != Season::Winter {
            return Flag::Unknown;
        }
        if !ordered(c.tank_on_c, c.tank_off_c) || !input.tank_c.is_finite() {
            self.demand = Flag::Unknown;
        } else if input.tank_c <= c.tank_on_c {
            self.demand = Flag::Yes;
        } else if input.tank_c >= c.tank_off_c {
            self.demand = Flag::No;
        }
        if input.plant_heat_available != Flag::Yes || !ordered(c.coil_stop_delta_c, c.coil_start_delta_c)
            || c.coil_stop_delta_c < 0.0 || !input.coil_supply_c.is_finite() || !input.tank_c.is_finite()
        {
            self.coil_hot = Flag::Unknown;
        } else {
            let delta = input.coil_supply_c - input.tank_c;
            if delta >= c.coil_start_delta_c {
                self.coil_hot = Flag::Yes;
            } else if delta <= c.coil_stop_delta_c {
                self.coil_hot = Flag::No;
            }
        }
        if input.plant_heat_available == Flag::No || self.demand == Flag::No || self.coil_hot == Flag::No {
            Flag::No
        } else if self.demand == Flag::Yes && self.coil_hot == Flag::Yes {
            Flag::Yes
        } else {
            Flag::Unknown
        }
    }
}

fn ordered(low: f32, high: f32) -> bool {
    low.is_finite() && high.is_finite() && low < high
}

#[derive(Default)]
struct Controller {
    rooms: [Room; ROOMS],
    last_presence: Presence,
    on_backup: bool,
    electric: ElectricStages,
    tank: Tank,
}

impl Controller {
    fn tick(&mut self, now: u32, c: &Config, input: &Inputs) -> Output {
        let (loops, demand) = self.evaluate_rooms(c, input);
        let (source, alarm) = self.choose_source(c, input, demand);
        let tank_power = match input.presence {
            Presence::Home => Flag::Yes,
            Presence::Away => Flag::No,
            Presence::Unknown => Flag::Unknown,
        };
        Output {
            loops,
            demand,
            alarm,
            source,
            stages: self.electric.tick(now, c, input, source),
            tank_power,
            coil_pump: self.tank.pump(c, input, tank_power),
        }
    }

    fn evaluate_rooms(&mut self, c: &Config, input: &Inputs) -> ([Flag; LOOPS], Flag) {
        let mut masks = [0u16; ROOMS];
        let mut invalid_map = false;
        let mut assigned = false;
        for (i, &id) in c.room_for_loop.iter().enumerate() {
            if id == 0 {
                continue;
            }
            if let Some(mask) = masks.get_mut(usize::from(id) - 1) {
                *mask |= 1 << i;
                assigned = true;
            } else {
                invalid_map = true;
            }
        }
        let valid_thresholds = ordered(c.room_on_c, c.room_radiator_off_c)
            && ordered(c.room_radiator_off_c, c.room_off_c);
        for ((room, mask), &temperature) in self.rooms.iter_mut().zip(masks).zip(&input.room_c) {
            if room.loops != mask {
                *room = Room { loops: mask, ..Room::default() };
            }
            if input.season == Season::Summer {
                room.set_both(Flag::No);
            } else if input.season != Season::Winter || !valid_thresholds || !temperature.is_finite() {
                room.set_both(Flag::Unknown);
            } else if temperature >= c.room_off_c {
                room.set_both(Flag::No);
            } else if temperature <= c.room_on_c {
                room.set_both(Flag::Yes);
            } else if mask & FLOOR_MASK != 0 && mask >> FLOOR_LOOPS != 0
                && temperature >= c.room_radiator_off_c
            {
                room.radiator = Flag::No;
            }
        }

        let mut loops = [Flag::Unknown; LOOPS];
        let mut any_on = false;
        let mut any_unknown = false;
        for (i, &id) in c.room_for_loop.iter().enumerate() {
            if id == 0 {
                continue;
            }
            if let Some(room) = self.rooms.get(usize::from(id) - 1) {
                loops[i] = if i < FLOOR_LOOPS { room.floor } else { room.radiator };
                any_on |= loops[i] == Flag::Yes;
                any_unknown |= loops[i] == Flag::Unknown;
            }
        }
        let demand = if input.season == Season::Summer {
            Flag::No
        } else if input.season != Season::Winter || !assigned || invalid_map {
            Flag::Unknown
        } else if any_on {
            Flag::Yes
        } else if any_unknown {
            Flag::Unknown
        } else {
            Flag::No
        };
        (loops, demand)
    }

    fn choose_source(&mut self, c: &Config, input: &Inputs, demand: Flag) -> (Source, Flag) {
        if input.presence != self.last_presence || demand == Flag::No {
            self.on_backup = false;
        }
        self.last_presence = input.presence;
        let (primary, backup, primary_source, backup_source) = match input.presence {
            Presence::Home => (input.diesel, input.electric, Source::Diesel, Source::Electric),
            Presence::Away => (input.electric, input.diesel, Source::Electric, Source::Diesel),
            Presence::Unknown => {
                let source = if demand == Flag::No { Source::None } else { Source::Unknown };
                return (source, Flag::Unknown);
            }
        };
        let alarm = match primary {
            Health::Unknown => Flag::Unknown,
            Health::Fault => Flag::Yes,
            Health::Ready => Flag::No,
        };
        if demand == Flag::No {
            return (Source::None, alarm);
        }
        if demand != Flag::Yes {
            return (Source::Unknown, alarm);
        }
        if !self.on_backup {
            match primary {
                Health::Ready => return (primary_source, alarm),
                Health::Unknown => return (Source::Unknown, alarm),
                Health::Fault => {}
            }
        }
        if input.presence == Presence::Away && !c.allow_away_diesel {
            self.on_backup = false;
            return (Source::None, alarm);
        }
        match backup {
            Health::Fault => (Source::None, Flag::Yes),
            Health::Unknown => (Source::Unknown, alarm),
            Health::Ready => {
                self.on_backup = true;
                (backup_source, alarm)
            }
        }
    }
}
