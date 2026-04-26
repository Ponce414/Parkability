/*
 * wifi_uplink.h — leader -> backend link.
 *
 * Two transports selected at compile time in config.h:
 *   #define UPLINK_PROTOCOL_MQTT 1   (default)
 *   #define UPLINK_PROTOCOL_REST 1
 *
 * Public interface is protocol-agnostic. WiFi connection is managed here
 * (connect on init, auto-reconnect on disconnect via WiFi event handler).
 */

#ifndef LEADER_WIFI_UPLINK_H
#define LEADER_WIFI_UPLINK_H

#include <stdint.h>
#include "messages.h"

/* Bring up WiFi STA + the chosen uplink transport. Non-blocking; the
 * connection completes asynchronously and reconnects on drop. */
void wifi_uplink_init(const char *ssid, const char *password);

/* True once WiFi is associated AND the chosen transport is ready. */
int wifi_uplink_is_ready(void);

/* Publish a sensor state change to the backend. Returns 0 on enqueue
 * success (MQTT) or 200-class HTTP response (REST); negative on error. */
int wifi_uplink_publish_update(const SensorUpdate *update,
                               const char *leader_mac_str);

/* Publish a leader-change notification — called by bully.c when this
 * node wins an election. */
int wifi_uplink_publish_leader_change(const char *zone_id,
                                      const char *new_leader_mac);

#endif /* LEADER_WIFI_UPLINK_H */
