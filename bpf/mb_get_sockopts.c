#include "headers/helpers.h"
#include "headers/maps.h"
#include <linux/bpf.h>
#include <linux/in.h>

#define MAX_OPS_BUFF_LENGTH 4096
#define SO_ORIGINAL_DST 80

__section("cgroup/getsockopt") int mb_get_sockopt(struct bpf_sockopt *ctx)
{
    // currently, eBPF can not deal with optlen more than 4096 bytes, so, we
    // should limit this.
    if (ctx->optlen > MAX_OPS_BUFF_LENGTH) {
        debugf("optname: %d, force set optlen to %d, original optlen %d is too "
               "high",
               ctx->optname, MAX_OPS_BUFF_LENGTH, ctx->optlen);
        ctx->optlen = MAX_OPS_BUFF_LENGTH;
    }
    // envoy will call getsockopt with SO_ORIGINAL_DST, we should rewrite it to
    // return original dst info.
    if (ctx->optname == SO_ORIGINAL_DST) {
        struct pair p = {
            .dip = ctx->sk->src_ip4,
            .dport = bpf_htons(ctx->sk->src_port),
            .sip = ctx->sk->dst_ip4,
            .sport = bpf_htons(ctx->sk->dst_port),
        };
        struct origin_info *origin =
            bpf_map_lookup_elem(&pair_original_dst, &p);
        if (origin) {
            // rewrite original_dst
            ctx->optlen = (__s32)sizeof(struct sockaddr_in);
            if ((void *)((struct sockaddr_in *)ctx->optval + 1) >
                ctx->optval_end) {
                printk("optname: %d: invalid getsockopt optval", ctx->optname);
                return 1;
            }
            ctx->retval = 0;
            struct sockaddr_in sa = {
                .sin_family = ctx->sk->family,
                .sin_addr.s_addr = origin->ip,
                .sin_port = origin->port,
            };
            *(struct sockaddr_in *)ctx->optval = sa;
        }
    }
    return 1;
}

char ____license[] __section("license") = "GPL";
int _version __section("version") = 1;
