#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 14, 0)
struct lsm_context {
    char *context;
    u32 len;
};

static int __security_secid_to_secctx(u32 secid, struct lsm_context *cp)
{
    return security_secid_to_secctx(secid, &cp->context, &cp->len);
}
static void __security_release_secctx(struct lsm_context *cp)
{
    security_release_secctx(cp->context, cp->len);
}
#else
#define __security_secid_to_secctx security_secid_to_secctx
#define __security_release_secctx security_release_secctx
#endif

#define INIT_CONTEXT "u:r:init:s0"

static u32 cached_init_sid __read_mostly = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 18, 0)
static inline bool is_sid_match_tsec(const struct task_security_struct *tsec,
                                     u32 cached_sid,
                                     const char *fallback_context)
#else
static inline bool is_sid_match_tsec(const struct cred_security_struct *tsec,
                                     u32 cached_sid,
                                     const char *fallback_context)
#endif
{
    struct lsm_context ctx;
    bool result;

    // Fast path: use cached SID if available
    if (likely(cached_sid != 0)) {
        return tsec->sid == cached_sid;
    }

    // Slow path fallback: string comparison (only before cache is initialized)
    if (__security_secid_to_secctx(tsec->sid, &ctx)) {
        return false;
    }
    result = strncmp(fallback_context, ctx.context, ctx.len) == 0;
    __security_release_secctx(&ctx);

    return result;
}

/*
 * Fast path: compare task's SID directly against cached value.
 * Falls back to string comparison if cache is not initialized.
 */
static inline bool is_sid_match(const struct cred *cred, u32 cached_sid,
                                const char *fallback_context)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 18, 0)
    const struct task_security_struct *tsec;
#else
    const struct cred_security_struct *tsec;
#endif

    if (!cred) {
        return false;
    }
    tsec = selinux_cred(cred);
    if (!tsec) {
        return false;
    }

    return is_sid_match_tsec(tsec, cached_sid, fallback_context);
}

static bool is_init(const struct cred *cred)
{
    return is_sid_match(cred, cached_init_sid, INIT_CONTEXT);
}