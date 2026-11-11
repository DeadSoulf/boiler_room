//! Run with `cargo run --example heating_cycle` from the crate directory.
use heating_core::{Config, Controller, Flag, Health, Inputs, Presence, Reheat, Season, Source};

fn main() {
    // Two collector loops share the first room's air sensor.
    let mut room_for_loop = [0; 14];
    room_for_loop[0] = 1; // First floor loop.
    room_for_loop[8] = 1; // First radiator loop.
    let config = Config {
        room_for_loop,
        room_on_c: 19.0,
        room_radiator_off_c: 22.0,
        room_off_c: 23.0,
        supply_target_c: 60.0,
        return_target_c: 50.0,
        return_restart_c: 45.0,
        reheat: Reheat::All,
        reheat_step_ms: 0,
        allow_away_diesel: false,
        tank_on_c: f32::NAN,
        tank_off_c: f32::NAN,
        coil_start_delta_c: f32::NAN,
        coil_stop_delta_c: f32::NAN,
    };
    let mut inputs = Inputs {
        room_c: [f32::NAN; 14],
        supply_c: f32::NAN,
        return_c: f32::NAN,
        tank_c: f32::NAN,
        coil_supply_c: f32::NAN,
        presence: Presence::Away,
        season: Season::Winter,
        diesel: Health::Unknown,
        electric: Health::Ready,
        circulation: Flag::Yes,
        plant_heat_available: Flag::Unknown,
    };
    let mut controller = Controller::default();
    println!("Пример настроек: подача 60 °C, обратка 50 °C, повторный пуск всех ТЭНов при 45 °C.");
    println!("Время   Воздух °C  Подача °C  Обратка °C  Запрос кВт  Батарея");

    // The first reduction is at 01:00. At 05:59 it is too early to shed again.
    for (seconds, room, supply, ret, expected_kw, expected_radiator) in [
        (0,   18.0, 35.0, 30.0, 9, Flag::Yes),
        (60,  19.0, 60.0, 40.0, 6, Flag::Yes),
        (359, 21.0, 61.0, 47.0, 6, Flag::Yes),
        (360, 21.0, 61.0, 48.0, 3, Flag::Yes),
        (375, 22.0, 61.0, 49.0, 3, Flag::No),
        (380, 22.0, 60.0, 50.0, 0, Flag::No),
        (600, 21.5, 55.0, 46.0, 0, Flag::No),
        (660, 21.5, 54.0, 45.0, 9, Flag::No),
    ] {
        inputs.room_c[0] = room;
        inputs.supply_c = supply;
        inputs.return_c = ret;
        let out = controller.tick(seconds * 1_000, &config, &inputs);
        assert!(out.source == Source::Electric, "источник в момент {seconds} с");
        assert!(out.loops[0] == Flag::Yes, "тёплый пол в момент {seconds} с");
        assert!(out.loops[8] == expected_radiator, "батарея в момент {seconds} с");
        let kw = out.stages * 3;
        assert_eq!(kw, expected_kw, "мощность в момент {seconds} с");
        let radiator = if out.loops[8] == Flag::Yes { "открыта" } else { "закрыта" };
        println!(
            "{:02}:{:02}   {room:>8.1}  {supply:>9.1}  {ret:>10.1}  {kw:>10}  {radiator}",
            seconds / 60, seconds % 60
        );
    }
}
