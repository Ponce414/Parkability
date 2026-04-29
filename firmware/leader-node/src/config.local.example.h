/*
 * Copy these values into config.local.h on your laptop and fill them in.
 * config.local.h is ignored by git so your SSO password is not tracked.
 */

#define WIFI_SECURITY_ENTERPRISE 1
#define WIFI_SSID "eduroam"
#define WIFI_EAP_IDENTITY "your.email@student.csulb.edu"
#define WIFI_EAP_USERNAME WIFI_EAP_IDENTITY
#define WIFI_EAP_PASSWORD "your-sso-password"

/* Use your laptop's current WiFi IP when running the backend locally. */
#define REST_BACKEND_HOST "192.0.0.2"
