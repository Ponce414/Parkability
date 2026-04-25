/*
 * wifi_uplink.h — leader -> backend link.
 *
 * Supports two protocols selected at compile time:
 *   -DUPLINK_PROTOCOL_MQTT (default)
 *   -DUPLINK_PROTOCOL_REST
 *
 * The public interface here is protocol-agnostic. Implementation decides
 * whether to publish to an MQTT topic or POST to a REST endpoint.
 */

#ifndef LEADER_WIFI_UPLINK_H
#define LEADER_WIFI_UPLINK_H

#include <stdint.h>
#include "messages.h"

/* Compile-time protocol switch. */
#ifndef UPLINK_PROTOCOL_MQTT
#ifndef UPLINK_PROTOCOL_REST
#define UPLINK_PROTOCOL_MQTT 1
#endif
#endif

void wifi_uplink_init(const char *ssid, const char *password,
                      const char *backend_host, uint16_t backend_port);

/* Publish a sensor state change to the backend. Returns 0 on success,
 * negative on queue/network error. Caller must be in WiFi mode; if the
 * radio scheduler has ESP-NOW right now, this will queue or drop depending
 * on policy (currently: drop + bump stat). */
int wifi_uplink_publish_update(const SensorUpdate *update, const char *leader_mac_str);

/* Publish a leader-change notification — called by bully.c when this node
 * wins an election. */
int wifi_uplink_publish_leader_change(const char *zone_id, const char *new_leader_mac);

#endif /* LEADER_WIFI_UPLINK_H */
