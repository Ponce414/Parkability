# Hardware decisions

## Sensor: VL53L0X time-of-flight on I2C

Picked over PIR, ultrasonic, IR-break-beam.

| | VL53L0X | PIR | Ultrasonic | IR break-beam |
|---|---|---|---|---|
| Detects stationary cars | yes | no (motion only) | yes | yes |
| Range up to ~2m | yes | n/a | yes | yes |
| Lighting-immune | yes | yes | yes | weather-sensitive |
| Bus | I2C | digital | trig/echo | digital |
| Cost | ~$5 | ~$2 | ~$3 | ~$3 |

**Why VL53L0X:** detects stationary cars (PIR doesn't), simple I2C wiring,
small enough to mount overhead, immune to ambient light. The 2m max range is
fine for ceiling-mount in a typical garage.

## Sensor MCU: ESP32-C3 SuperMini

Compact, low-cost, has WiFi+BLE+ESP-NOW, and runs on PlatformIO with esp-idf
out of the box. Single radio is fine for sensors because they only ever
speak ESP-NOW.

## Leader MCU: ESP32-C3 DevKit (tentative)

Same chip family as sensors so we share `messages.h` / `lamport.c` / etc.
Ideally this would be a C5 or C6 (dual-radio, no time-sharing required) but
those weren't reliably available — so the C3 lives with a *single-radio*
constraint that the radio scheduler module exists to manage.

## Known hardware quirks

- ESP32-C3 ↔ older ESP32 ESP-NOW has been reported as one-way flaky. Bring-up
  test: explicitly send both directions and confirm receipt.
- VL53L0X factory address is 0x29; if we ever want to share an I2C bus we'd
  need to use the XSHUT pin to reassign one at boot. Not relevant for v1.
- C3 SuperMini I2C pin assignments vary between board batches. Current
  VL53L0X wiring is SDA=GPIO4 and SCL=GPIO5; sensor firmware auto-probes
  common alternate pairs at boot.

## Future: deferred decisions

- Final leader chip: revisit C5/C6 once reliably stocked.
- Spot ID encoding: still strings on the wire; keep the option open in
  `spot_id.c` to switch to a 4-byte packed form if we need it.
