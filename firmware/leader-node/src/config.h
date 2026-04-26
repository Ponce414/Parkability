/*
 * config.h — per-leader-node tunables.
 *
 * Everything you must set when flashing a new leader lives here.
 * No values elsewhere need editing for a normal deployment.
 */

#ifndef LEADER_NODE_CONFIG_H
#define LEADER_NODE_CONFIG_H

#include <stdint.h>

/* --- Identity ------------------------------------------------------------
 * Each leader covers one zone in one lot. Set per flashed device. */
#define ZONE_LOT   "lotA"
#define ZONE_ID    "zone1"

/* --- WiFi ----------------------------------------------------------------
 * Leader connects to this AP to reach the backend. */
#define WIFI_SSID     "parking-lot-net"
#define WIFI_PASSWORD "change-me"

/* --- Backend uplink -----------------------------------------------------
 * Compile-time switch picks the transport. Default: MQTT. */
#define UPLINK_PROTOCOL_MQTT 1
/* #define UPLINK_PROTOCOL_REST 1 */

/* MQTT broker (used when UPLINK_PROTOCOL_MQTT). */
#define MQTT_BROKER_HOST "192.168.1.10"
#define MQTT_BROKER_PORT 1883

/* REST backend (used when UPLINK_PROTOCOL_REST). */
#define REST_BACKEND_HOST "192.168.1.10"
#define REST_BACKEND_PORT 8000

/* --- Radio --------------------------------------------------------------
 * Must match sensor-node's ESPNOW_CHANNEL. */
#define ESPNOW_CHANNEL 1

/* Radio scheduler tunables (inactivity-based strategy).
 *  - Stay in ESP-NOW mode until either the aggregator requests WiFi
 *    OR ESPNOW_MAX_DWELL_MS elapses without a switch.
 *  - When in WiFi mode, stay there until the aggregator drains OR
 *    WIFI_MAX_DWELL_MS, whichever is first. */
#define ESPNOW_MAX_DWELL_MS 2000
#define WIFI_MAX_DWELL_MS    500

/* --- Heartbeat / Bully tunables ----------------------------------------- */
#define HEARTBEAT_INTERVAL_MS  1000
#define HEARTBEAT_TIMEOUT_MS   3500   /* ~3x interval to tolerate one drop */
#define BULLY_T_OK_MS          1500   /* wait for OK after sending ELECTION */
#define BULLY_T_COORD_MS       3000   /* wait for COORDINATOR after seeing OK */

/* --- Peer table size --------------------------------------------------- */
/* Caps zone size at ~MAX_PEERS sensors + 1 backup leader.
 * ESP-NOW hard limit on this chip is ~20 unencrypted peers. */
#define MAX_PEER_TABLE 20

/* Firmware version — bump on protocol-visible changes. */
#define LEADER_FIRMWARE_VERSION 1

#endif /* LEADER_NODE_CONFIG_H */
