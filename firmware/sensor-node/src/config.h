/*
 * config.h — per-sensor-node tunables.
 *
 * Everything a technician might need to change at bring-up lives here.
 * Recompile-and-flash is acceptable; we do not support OTA config in v1.
 */

#ifndef SENSOR_NODE_CONFIG_H
#define SENSOR_NODE_CONFIG_H

#include <stdint.h>

#if __has_include("config.local.h")
#include "config.local.h"
#endif

/* --- Identity -------------------------------------------------------------
 * Each sensor must be flashed with a unique spot id. We set it here rather
 * than reading from NVS to keep bring-up simple; revisit if we deploy >10
 * sensors and start confusing ourselves. */
#ifndef SPOT_LOT
#define SPOT_LOT   "lotA"
#endif
#ifndef SPOT_ZONE
#define SPOT_ZONE  "zone1"
#endif
#ifndef SPOT_SPOT
#define SPOT_SPOT  "spot1"
#endif

/* --- Occupancy detection -------------------------------------------------
 * OCCUPANCY_THRESHOLD_MM: this distance or below = occupied; above it = available. */
#define OCCUPANCY_THRESHOLD_MM  30

/* Poll cadence. 10Hz is plenty for parking — cars don't move fast. Lower =
 * less power, but slower detection. */
#define POLL_INTERVAL_MS        100

/* Send a telemetry refresh even when occupancy does not change, so the
 * dashboard can display live distance and freshness. */
#define TELEMETRY_INTERVAL_MS   1000

/* Distance calibration/filtering.
 * DISTANCE_OFFSET_MM lets us correct a fixed mounting bias after measuring a
 * spot with a tape measure. Invalid/out-of-range readings are ignored before
 * debounce so VL53L0X error values such as ~8190mm do not reach the website. */
#ifndef DISTANCE_OFFSET_MM
#define DISTANCE_OFFSET_MM      0
#endif
#ifndef DISTANCE_MIN_VALID_MM
#define DISTANCE_MIN_VALID_MM   1
#endif
#ifndef DISTANCE_MAX_VALID_MM
#define DISTANCE_MAX_VALID_MM   3000
#endif
#ifndef DISTANCE_FILTER_WINDOW
#define DISTANCE_FILTER_WINDOW  5
#endif
#ifndef DISTANCE_JUMP_TOLERANCE_MM
#define DISTANCE_JUMP_TOLERANCE_MM 80
#endif
#ifndef DISTANCE_JUMP_CONFIRM_COUNT
#define DISTANCE_JUMP_CONFIRM_COUNT 4
#endif

/* Debounce: require this many readings in the debounce window to agree before
 * changing state. Guards against momentary bad ToF readings and flicker. */
#define DEBOUNCE_COUNT          10

/* Size of the circular buffer for the debounce scheme. Must be >= DEBOUNCE_COUNT. */
#define DEBOUNCE_WINDOW         10

/* --- Radio --------------------------------------------------------------- */
/* ESP-NOW peer: zone leader's STA MAC address.
 * Current bring-up C5 base MAC: 3c:dc:75:85:a8:d0. */
#ifndef LEADER_MAC
#if defined(LEADER_MAC_0) && defined(LEADER_MAC_1) && defined(LEADER_MAC_2) && \
    defined(LEADER_MAC_3) && defined(LEADER_MAC_4) && defined(LEADER_MAC_5)
#define LEADER_MAC { LEADER_MAC_0, LEADER_MAC_1, LEADER_MAC_2, LEADER_MAC_3, LEADER_MAC_4, LEADER_MAC_5 }
#else
#define LEADER_MAC { 0x3c, 0xdc, 0x75, 0x85, 0xa8, 0xd0 }
#endif
#endif

/* WiFi channel used for ESP-NOW.
 * 0 enables sensor-side discovery across channels 1-11, which handles iPhone
 * hotspot channel changes between bring-up sessions. */
#ifndef ESPNOW_CHANNEL
#define ESPNOW_CHANNEL 0
#endif

#ifndef REGISTER_RETRY_MS
#define REGISTER_RETRY_MS 1000
#endif
#ifndef SENSOR_UPDATE_SEND_ATTEMPTS
#define SENSOR_UPDATE_SEND_ATTEMPTS 5
#endif
#ifndef SENSOR_UPDATE_SEND_RETRY_MS
#define SENSOR_UPDATE_SEND_RETRY_MS 250
#endif
#ifndef SENSOR_SEND_FAILURES_BEFORE_DISCOVERY
#define SENSOR_SEND_FAILURES_BEFORE_DISCOVERY 25
#endif

/* --- I2C ----------------------------------------------------------------- */
/* C3 SuperMini I2C pins used by the current VL53L0X wiring. Auto-detect below
 * still probes common alternate pairs so bring-up survives board variations. */
#ifndef I2C_SDA_GPIO
#define I2C_SDA_GPIO 4
#endif
#ifndef I2C_SCL_GPIO
#define I2C_SCL_GPIO 5
#endif
#ifndef I2C_FREQ_HZ
#define I2C_FREQ_HZ  100000
#endif
#ifndef I2C_AUTODETECT_PINS
#define I2C_AUTODETECT_PINS 1
#endif

/* VL53L0X default I2C address. Only change if bus-sharing with another ToF. */
#ifndef VL53L0X_ADDR
#define VL53L0X_ADDR 0x29
#endif

/* VL53L0X timing budget in microseconds. 33ms (33000us) is the library default
 * and a reasonable accuracy/latency tradeoff for this range. */
#define VL53L0X_TIMING_BUDGET_US 33000

/* Firmware version — bump on any protocol-visible change. */
#define FIRMWARE_VERSION 1

#endif /* SENSOR_NODE_CONFIG_H */
