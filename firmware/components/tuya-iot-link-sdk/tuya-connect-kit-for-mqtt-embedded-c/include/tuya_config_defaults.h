#ifndef TUYA_CONFIG_DEFAULTS_H_
#define TUYA_CONFIG_DEFAULTS_H_

/**
 * @brief The buffer pre-allocated during activation, 
 * the more function points, the larger the buffer needed.
 * 
 */
#ifndef ACTIVATE_BUFFER_LENGTH
    #define ACTIVATE_BUFFER_LENGTH (8192U)
#endif

/**
 * @brief ACTIVATE_KV_BUFFER_MAX is the maximum length of the buffer
 * 
 */
#ifndef ACTIVATE_KV_BUFFER_MAX
    #define ACTIVATE_KV_BUFFER_MAX (512U)
#endif

/**
 * @brief MQTT socket recv blocking time.
 * 
 */
#ifndef MQTT_RECV_BLOCK_TIME_MS
    #define MQTT_RECV_BLOCK_TIME_MS (2000U)
#endif

/**
 * @brief MQTT keep alive period.
 * 
 */
#ifndef MQTT_KEEPALIVE_INTERVALIN
    #define MQTT_KEEPALIVE_INTERVALIN (120)
#endif

/**
 * @brief Defaults auto check upgrade interval.
 * 
 */
#ifndef AUTO_UPGRADE_CHECK_INTERVAL
    #define AUTO_UPGRADE_CHECK_INTERVAL (1000U*60*60*24) // 24 hours
#endif

/**
 * @brief The maximum number of retries for connecting to server.
 */
#ifndef MQTT_CONNECT_RETRY_MAX_ATTEMPTS
    #define MQTT_CONNECT_RETRY_MAX_ATTEMPTS (0xffffffff)
#endif

/**
 * @brief The maximum back-off delay (in milliseconds) for retrying connection to server.
 */
#ifndef MQTT_CONNECT_RETRY_MAX_DELAY_MS
    #define MQTT_CONNECT_RETRY_MAX_DELAY_MS (8000U)
#endif

/**
 * @brief The base back-off delay (in milliseconds) to use for connection retry attempts.
 */
#ifndef MQTT_CONNECT_RETRY_MIN_DELAY_MS
    #define MQTT_CONNECT_RETRY_MIN_DELAY_MS (1000U)
#endif

/**
 * @brief MQTT BIND TLS timeout config.
 */
#ifndef MQTT_BIND_TIMEOUT_MS_DEFAULT
    #define MQTT_BIND_TIMEOUT_MS_DEFAULT (5000U)
#endif

/**
 * @brief HTTP TLS timeout config.
 */
#ifndef HTTP_TIMEOUT_MS_DEFAULT
    #define HTTP_TIMEOUT_MS_DEFAULT (5000U)
#endif

/**
 * @brief HTTP TLS timeout config.
 */
#ifndef MATOP_TIMEOUT_MS_DEFAULT
    #define MATOP_TIMEOUT_MS_DEFAULT (8000U)
#endif

#endif /* ifndef TUYA_CONFIG_DEFAULTS_H_ */
