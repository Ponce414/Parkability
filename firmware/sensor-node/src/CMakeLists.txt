idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        driver
        esp_event
        esp_netif
        esp_wifi
        esp_driver_i2c
        nvs_flash
        shared
)

if(DEFINED SENSOR_SPOT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE SPOT_SPOT=\"${SENSOR_SPOT}\")
endif()

if(DEFINED SENSOR_DISTANCE_OFFSET_MM)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE DISTANCE_OFFSET_MM=${SENSOR_DISTANCE_OFFSET_MM})
endif()

if(DEFINED SENSOR_DISTANCE_MAX_VALID_MM)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE DISTANCE_MAX_VALID_MM=${SENSOR_DISTANCE_MAX_VALID_MM})
endif()

if(DEFINED SENSOR_LEADER_MAC_0 AND DEFINED SENSOR_LEADER_MAC_1 AND
   DEFINED SENSOR_LEADER_MAC_2 AND DEFINED SENSOR_LEADER_MAC_3 AND
   DEFINED SENSOR_LEADER_MAC_4 AND DEFINED SENSOR_LEADER_MAC_5)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        LEADER_MAC_0=${SENSOR_LEADER_MAC_0}
        LEADER_MAC_1=${SENSOR_LEADER_MAC_1}
        LEADER_MAC_2=${SENSOR_LEADER_MAC_2}
        LEADER_MAC_3=${SENSOR_LEADER_MAC_3}
        LEADER_MAC_4=${SENSOR_LEADER_MAC_4}
        LEADER_MAC_5=${SENSOR_LEADER_MAC_5}
    )
endif()
