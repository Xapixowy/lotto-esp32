#pragma once

// Copy to Secrets.h. Leave display-check mode enabled for the first upload.
#define DISPLAY_CHECK_ONLY true
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define BACKEND_URL "https://lotto.example.com/api/results"
#define API_TOKEN "YOUR_DEVICE_TOKEN"
#define UI_LANGUAGE "pl"

// Root CA certificate that validates your VPS HTTPS certificate.
// Obtain the root from your certificate authority, not the server's leaf certificate.
static const char BACKEND_ROOT_CA[] = R"PEM(
-----BEGIN CERTIFICATE-----
PASTE_YOUR_CA_ROOT_CERTIFICATE_HERE
-----END CERTIFICATE-----
)PEM";
