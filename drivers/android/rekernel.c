#include <linux/rekernel.h>

int rekernel_netlink_unit = NETLINK_REKERNEL_MIN;
struct sock *rekernel_netlink = NULL;
struct netlink_kernel_cfg rekernel_cfg = { 
    .input = netlink_rcv_msg,
};