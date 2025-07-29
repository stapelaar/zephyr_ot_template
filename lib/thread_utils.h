#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H

#include <zephyr/kernel.h>
#include <openthread/thread.h>

int thread_init(void);
int thread_get_ipv6_addr(char *addr_str);

#endif /* THREAD_UTILS_H */