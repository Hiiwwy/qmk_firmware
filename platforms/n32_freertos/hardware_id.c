#include "hardware_id.h"

__attribute__((weak)) hardware_id_t get_hardware_id(void) {
    hardware_id_t id = {0};
    return id;
}
