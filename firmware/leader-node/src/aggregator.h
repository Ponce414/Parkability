/*
 * aggregator.h — collects sensor updates and pushes them to the backend.
 *
 * Receives SensorUpdate and Register from espnow_handler.c. Buffers them
 * briefly (to amortize WiFi-mode windows with the radio scheduler) then
 * hands them to wifi_uplink.c.
 *
 * Batching/throttling policy: TODO — see aggregator.c.
 */

#ifndef LEADER_AGGREGATOR_H
#define LEADER_AGGREGATOR_H

#include "messages.h"

void aggregator_init(void);

void aggregator_on_sensor_update(const SensorUpdate *msg);
void aggregator_on_register(const Register *msg);

/* Called by the main loop periodically — gives the aggregator a chance to
 * flush its buffer when the radio is in WiFi mode. */
void aggregator_tick(void);

#endif /* LEADER_AGGREGATOR_H */
