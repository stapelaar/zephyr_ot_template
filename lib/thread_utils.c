#include <zephyr/logging/log.h>
#include <zephyr/net/openthread.h>
#include <zephyr/settings/settings.h>
#include "thread_utils.h"

LOG_MODULE_REGISTER(thread_utils, LOG_LEVEL_INF);

int thread_init(void)
{
    otInstance *ot = openthread_get_default_instance();

    // Reset NVS to clear old datasets
    settings_subsys_init();
    settings_delete("ot"); // Wis OpenThread NVS data
    LOG_INF("NVS reset for OpenThread");

    // Stel de operational dataset in vanuit config
    otOperationalDataset dataset = {
        .mActiveTimestamp = 1,
        .mPanId = CONFIG_OPENTHREAD_PANID,
        .mChannel = CONFIG_OPENTHREAD_CHANNEL,
        .mNetworkKey = {{ // Extra accolades voor array
            0x6a, 0xc2, 0x56, 0xfa, 0x94, 0x4d, 0xd2, 0x8b,
            0x7f, 0x9a, 0x64, 0x1d, 0x84, 0x49, 0xed, 0xd9
        }},
        .mExtendedPanId = {{ // Extra accolades voor array
            0x12, 0x34, 0x56, 0x78, 0x12, 0x34, 0x56, 0x78
        }},
        .mNetworkName = { .m8 = "OT-MANNAH" },
        .mComponents = {
            .mIsActiveTimestampPresent = true,
            .mIsPanIdPresent = true,
            .mIsChannelPresent = true,
            .mIsNetworkKeyPresent = true,
            .mIsExtendedPanIdPresent = true,
            .mIsNetworkNamePresent = true
        }
    };
    otError error = otDatasetSetActive(ot, &dataset);
    if (error != OT_ERROR_NONE) {
        LOG_ERR("Failed to set active dataset: %d", error);
        return -EIO;
    }
    LOG_INF("Thread dataset set: PANID=%d, Channel=%d, Name=%s",
            CONFIG_OPENTHREAD_PANID, CONFIG_OPENTHREAD_CHANNEL, "OT-MANNAH");

    // Wacht tot de node het netwerk joint
    int timeout = 30; // 30 seconden timeout
    while (timeout > 0) {
        otDeviceRole role = otThreadGetDeviceRole(ot);
        if (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER) {
            LOG_INF("Thread role: %s",
                    role == OT_DEVICE_ROLE_CHILD ? "child" : "router");
            return 0;
        }
        LOG_INF("Waiting for Thread connection... (role: %d)", role);
        k_sleep(K_SECONDS(1));
        timeout--;
    }

    LOG_ERR("Thread join timeout");
    return -ETIMEDOUT;
}

int thread_get_ipv6_addr(char *addr_str)
{
    struct net_if *iface = net_if_get_default();
    struct in6_addr *my_addr = NULL;

    for (int i = 0; i < CONFIG_NET_IF_UNICAST_IPV6_ADDR_COUNT; i++) {
        struct net_if_addr *if_addr = &iface->config.ip.ipv6->unicast[i];
        if (if_addr->is_used && if_addr->address.family == AF_INET6 &&
            if_addr->addr_state == NET_ADDR_PREFERRED) {
            my_addr = &if_addr->address.in6_addr;
            break;
        }
    }

    if (!my_addr) {
        LOG_ERR("No usable IPv6 address found.");
        return -ENOENT;
    }

    net_addr_ntop(AF_INET6, my_addr, addr_str, NET_IPV6_ADDR_LEN);
    LOG_INF("Using IPv6 address: %s", addr_str);
    return 0;
}