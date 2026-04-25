#include "spot_id.h"

#include <string.h>
#include <stdio.h>

int spot_id_format(char *out, const char *lot, const char *zone, const char *spot)
{
    int n = snprintf(out, SPOT_ID_MAX_LEN, "%s/%s/%s", lot, zone, spot);
    if (n < 0 || n >= SPOT_ID_MAX_LEN) {
        return -1;
    }
    return n;
}

int spot_id_parse(const char *id,
                  char *lot_out,   uint8_t lot_cap,
                  char *zone_out,  uint8_t zone_cap,
                  char *spot_out,  uint8_t spot_cap)
{
    const char *first = strchr(id, '/');
    if (!first) return -1;
    const char *second = strchr(first + 1, '/');
    if (!second) return -1;

    size_t lot_len  = (size_t)(first - id);
    size_t zone_len = (size_t)(second - first - 1);
    size_t spot_len = strlen(second + 1);

    if (lot_len == 0 || zone_len == 0 || spot_len == 0) return -1;
    if (lot_len  >= lot_cap)  return -1;
    if (zone_len >= zone_cap) return -1;
    if (spot_len >= spot_cap) return -1;

    memcpy(lot_out,  id,         lot_len);  lot_out[lot_len]   = '\0';
    memcpy(zone_out, first + 1,  zone_len); zone_out[zone_len] = '\0';
    memcpy(spot_out, second + 1, spot_len); spot_out[spot_len] = '\0';
    return 0;
}

int spot_id_belongs_to_zone(const char *id, const char *lot, const char *zone)
{
    char prefix[SPOT_ID_MAX_LEN];
    int n = snprintf(prefix, sizeof(prefix), "%s/%s/", lot, zone);
    if (n < 0 || n >= (int)sizeof(prefix)) return 0;
    return strncmp(id, prefix, (size_t)n) == 0;
}
