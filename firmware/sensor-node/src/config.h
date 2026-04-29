/*
 * config.h — per-sensor-node tunables.
 *
 * Everything a technician might need to change at bring-up lives here.
 * Recompile-and-flash is acceptable; we do not support OTA config in v1.
 */

#ifndef SENSOR_NODE_CONFIG_H
#define SENSOR_NODE_CONFIG_H

#include <stdint.h>

/* --- Identity -------------------------------------------------------------
 * Each sensor must be flashed with a unique spot id. We set it here rather
 * than reading from NVS to keep bring-up simple; revisit if we deploy >10
 * sensors and start confusing ourselves. */
#define SPOT_LOT   "lotA"
#define SPOT_ZONE  "zone1"
#define SPOT_SPOT  "spot1"

/* --- Occupancy detection -------------------------------------------------
 * OCCUPANCY_THRESHOLD_MM: below this distance = occupied.
 * Default 1500mm (1.5m) assumes a ceiling-mounted sensor ~2.5m above the ground
 * with a car height of ~1.5m. CALIBRATE AT BRING-UP per install site. */
#define OCCUPANCY_THRESHOLD_MM  1500

/* Poll cadence. 10Hz is plenty for parking — cars don't move fast. Lower =
 * less power, but slower detection. */
#define POLL_INTERVAL_MS        100

/* Send a telemetry refresh even when occupancy does not change, so the
 * dashboard can display live distance and freshness. */
#define TELEMETRY_INTERVAL_MS   10000

/* Debounce: require this many consecutive readings agreeing before changing
 * state. Guards against momentary beams and flicker at the threshold. */
#define DEBOUNCE_COUNT          3

/* Size of the circular buffer for the 3-of-5 debounce scheme. Must be >= DEBOUNCE_COUNT. */
#define DEBOUNCE_WINDOW         5

/* --- Radio --------------------------------------------------------------- */
/* ESP-NOW peer: zone leader's STA MAC address.
 * Current bring-up C5 base MAC: 3c:dc:75:88:fa:4c. */
#define LEADER_MAC { 0x3c, 0xdc, 0x75, 0x88, 0xfa, 0x4c }

/* WiFi channel used for ESP-NOW. Must match the leader's channel.
 * Channel 1 is a safe default in North America. */
#define ESPNOW_CHANNEL 11

/* --- I2C ----------------------------------------------------------------- */
/* C3 SuperMini default I2C pins. CONFIRM against your specific board's pinout
 * at bring-up — some C3 variants silkscreen these differently. */
#define I2C_SDA_GPIO 8
#define I2C_SCL_GPIO 9
#define I2C_FREQ_HZ  400000

/* VL53L0X default I2C address. Only change if bus-sharing with another ToF. */
#define VL53L0X_ADDR 0x29

/* VL53L0X timing budget in microseconds. 33ms (33000us) is the library default
 * and a reasonable accuracy/latency tradeoff for this range. */
#define VL53L0X_TIMING_BUDGET_US 33000

/* Firmware version — bump on any protocol-visible change. */
#define FIRMWARE_VERSION 1

#endif /* SENSOR_NODE_CONFIG_H */
