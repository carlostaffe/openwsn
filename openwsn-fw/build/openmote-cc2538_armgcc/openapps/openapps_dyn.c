#include "opendefs.h"

#include "opencoap.h"
#include "cjoin.h"

void openapps_init(void) {
   opencoap_init();
   cjoin_init();
}