#include "suspend.h"
#include "wait.h"

void suspend_power_down(void) {
    suspend_power_down_quantum();
    wait_ms(17);
}

void suspend_wakeup_init(void) {
    suspend_wakeup_init_quantum();
}
