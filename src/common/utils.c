#include "utils.h"
#include "log.h"

static int is_little_endian(void) {
    // big endian：0x12, 0x34; small endian：0x34, 0x12
    uint16_t v = 0x1234;
    uint8_t* b = (uint8_t*)&v;
    return b[0] == 0x34;
}


net_err_t utils_init(void) {
    log_info(LOG_UTILS, "init tools.");

    // the endian of the host and the settings must be the same.
    int host_endian = is_little_endian();
    log_info(LOG_UTILS, "host endian: %d", host_endian);
    if (host_endian  != NET_ENDIAN_LITTLE) {
        log_error(LOG_UTILS, "check endian faild.");
        return NET_ERR_SYS;
    }
    log_info(LOG_UTILS, "done.");
    return NET_OK;
}