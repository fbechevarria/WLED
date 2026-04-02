#include "usermod_v2_improv_ble.h"

#if defined(USERMOD_IMPROV_BLE) && defined(ESP32)
static UsermodImprovBLE improv_ble;
REGISTER_USERMOD(improv_ble);
#endif
