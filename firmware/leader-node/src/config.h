/*
 * config.h — per-leader-node tunables.
 *
 * Everything you must set when flashing a new leader lives here.
 * No values elsewhere need editing for a normal deployment.
 */

#ifndef LEADER_NODE_CONFIG_H
#define LEADER_NODE_CONFIG_H

#include <stdint.h>

#if __has_include("config.local.h")
#include "config.local.h"
#endif

/* --- Identity ------------------------------------------------------------
 * Each leader covers one zone in one lot. Set per flashed device. */
#ifndef ZONE_LOT
#define ZONE_LOT   "lotA"
#endif
#ifndef ZONE_ID
#define ZONE_ID    "zone1"
#endif

/* --- WiFi ----------------------------------------------------------------
 * Leader connects to this AP to reach the backend.
 * Put real local credentials in ignored config.local.h. */
#ifndef WIFI_SECURITY_ENTERPRISE
#define WIFI_SECURITY_ENTERPRISE 0
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "Parth’s iPhone"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

#ifndef WIFI_EAP_IDENTITY
#define WIFI_EAP_IDENTITY "your.email@student.csulb.edu"
#endif

#ifndef WIFI_EAP_USERNAME
#define WIFI_EAP_USERNAME WIFI_EAP_IDENTITY
#endif

#ifndef WIFI_EAP_PASSWORD
#define WIFI_EAP_PASSWORD ""
#endif

#ifndef WIFI_EAP_USE_DEFAULT_CERT_BUNDLE
#define WIFI_EAP_USE_DEFAULT_CERT_BUNDLE 1
#endif

#ifndef WIFI_EAP_SERVER_DOMAIN
#define WIFI_EAP_SERVER_DOMAIN ""
#endif

/* --- Backend uplink -----------------------------------------------------
 * Compile-time switch picks the transport. iPhone hotspot clients are not
 * reliably reachable from each other over local IPv4, so MQTT uses an
 * internet broker while the laptop backend subscribes to the same broker. */
#define UPLINK_PROTOCOL_MQTT 1
/* #define UPLINK_PROTOCOL_REST 1 */

/* MQTT broker (used when UPLINK_PROTOCOL_MQTT). */
#define MQTT_BROKER_HOST "35.158.179.76"
#define MQTT_BROKER_PORT 1883

/* REST backend (used when UPLINK_PROTOCOL_REST). */
#ifndef REST_BACKEND_HOST
#define REST_BACKEND_HOST "192.0.0.2"
#endif
#ifndef REST_BACKEND_PORT
#define REST_BACKEND_PORT 8000
#endif

/* --- Radio --------------------------------------------------------------
 * Fallback ESP-NOW channel before the leader joins WiFi. Sensors use
 * channel discovery, so they will follow the hotspot's live 2.4GHz channel. */
#define ESPNOW_CHANNEL 1

/* Radio scheduler tunables (inactivity-based strategy).
 *  - Stay in ESP-NOW mode until either the aggregator requests WiFi
 *    OR ESPNOW_MAX_DWELL_MS elapses without a switch.
 *  - When in WiFi mode, stay there until the aggregator drains OR
 *    WIFI_MAX_DWELL_MS, whichever is first. */
#define ESPNOW_MAX_DWELL_MS 1000
#define WIFI_MAX_DWELL_MS   5000

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
