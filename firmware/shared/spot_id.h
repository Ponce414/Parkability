/*
 * spot_id.h — hierarchical spot identifiers.
 *
 * Format: "lot/zone/spot", e.g., "lotA/zone2/spot17".
 * See concepts/04-naming-service-discovery/README.md for rationale.
 *
 * Encoding tradeoff (deferred decision — currently string-based):
 *   - String form is human-readable in logs and the DB, costs ~24 bytes on wire.
 *   - Packed bitfield (e.g., 8 bits lot + 12 bits zone + 12 bits spot = 4 bytes)
 *     is tighter but requires a lookup table everywhere ids are displayed.
 *   We use strings for now and keep this module as the single place to change
 *   if we flip to bitfields later.
 */

#ifndef PARKING_SPOT_ID_H
#define PARKING_SPOT_ID_H

#include <stdint.h>
#include "messages.h"   /* for SPOT_ID_MAX_LEN */

/* Build a spot id string into `out`. Returns number of bytes written (excluding
 * null terminator), or -1 if it would overflow SPOT_ID_MAX_LEN.
 * lot/zone/spot are caller-provided strings; caller enforces their own length. */
int spot_id_format(char *out, const char *lot, const char *zone, const char *spot);

/* Parse a spot id of the form "lot/zone/spot" into three heap-free outputs.
 * Each output buffer must hold at least `max_component_len` bytes.
 * Returns 0 on success, -1 on malformed input. */
int spot_id_parse(const char *id,
                  char *lot_out,   uint8_t lot_cap,
                  char *zone_out,  uint8_t zone_cap,
                  char *spot_out,  uint8_t spot_cap);

/* True if `id` matches the `lot/zone/*` prefix — used by leaders to decide
 * whether an incoming Register belongs to their zone. */
int spot_id_belongs_to_zone(const char *id, const char *lot, const char *zone);

#endif /* PARKING_SPOT_ID_H */
