#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

LOG_MODULE_REGISTER(shell_utils, LOG_LEVEL_INF);

void shell_utils_init(void)
{
    LOG_INF("Shell initialized");
}