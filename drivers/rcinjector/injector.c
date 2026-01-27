#include <linux/version.h>
#include <linux/lsm_hooks.h>
#include <linux/security.h>
#include <linux/module.h>
#include "objsec.h"

#include "klog.h"
#include "injector.h"

#include "selinux.c"

bool injector_init_rc_hook __read_mostly = true;

static ssize_t (*orig_read)(struct file *, char __user *, size_t, loff_t *);
static ssize_t (*orig_read_iter)(struct kiocb *, struct iov_iter *);
static struct file_operations fops_proxy;
static ssize_t injector_rc_pos = 0;
const size_t injector_rc_len = sizeof(INJECTOR_RC) - 1;

// https://cs.android.com/android/platform/superproject/main/+/main:system/core/init/parser.cpp;l=144;drc=61197364367c9e404c7da6900658f1b16c42d0da
// https://cs.android.com/android/platform/superproject/main/+/main:system/libbase/file.cpp;l=241-243;drc=61197364367c9e404c7da6900658f1b16c42d0da
// The system will read init.rc file until EOF, whenever read() returns 0,
// so we begin append injector rc when we meet EOF.

static ssize_t read_proxy(struct file *file, char __user *buf, size_t count,
                          loff_t *pos)
{
    ssize_t ret = 0;
    size_t append_count;
    if (injector_rc_pos && injector_rc_pos < injector_rc_len)
        goto append_injector_rc;

    ret = orig_read(file, buf, count, pos);
    if (ret != 0 || injector_rc_pos >= injector_rc_len) {
        return ret;
    } else {
        pr_info("read_proxy: orig read finished, start append rc\n");
    }
append_injector_rc:
    append_count = injector_rc_len - injector_rc_pos;
    if (append_count > count - ret)
        append_count = count - ret;
    // copy_to_user returns the number of not copied
    if (copy_to_user(buf + ret, INJECTOR_RC + injector_rc_pos, append_count)) {
        pr_info("read_proxy: append error, totally appended %zd\n", injector_rc_pos);
    } else {
        pr_info("read_proxy: append %zd\n", append_count);

        injector_rc_pos += append_count;
        if (injector_rc_pos == injector_rc_len) {
            pr_info("read_proxy: append done\n");
        }
        ret += append_count;
    }

    return ret;
}

static ssize_t read_iter_proxy(struct kiocb *iocb, struct iov_iter *to)
{
    ssize_t ret = 0;
    size_t append_count;
    if (injector_rc_pos && injector_rc_pos < injector_rc_len)
        goto append_injector_rc;

    ret = orig_read_iter(iocb, to);
    if (ret != 0 || injector_rc_pos >= injector_rc_len) {
        return ret;
    } else {
        pr_info("read_iter_proxy: orig read finished, start append rc\n");
    }
append_injector_rc:
    // copy_to_iter returns the number of copied bytes
    append_count =
        copy_to_iter(INJECTOR_RC + injector_rc_pos, injector_rc_len - injector_rc_pos, to);
    if (!append_count) {
        pr_info("read_iter_proxy: append error, totally appended %zd\n",
                injector_rc_pos);
    } else {
        pr_info("read_iter_proxy: append %zd\n", append_count);

        injector_rc_pos += append_count;
        if (injector_rc_pos == injector_rc_len) {
            pr_info("read_iter_proxy: append done\n");
        }
        ret += append_count;
    }
    return ret;
}

static bool is_atrace_rc(struct file *fp)
{
    const char *short_name = NULL;
    char path[256];
    char *dpath = NULL;

    if (strcmp(current->comm, "init")) {
        // we are only interest in `init` process
        return false;
    }

    if (!d_is_reg(fp->f_path.dentry)) {
        return false;
    }

    short_name = fp->f_path.dentry->d_name.name;
    if (strcmp(short_name, "atrace.rc")) {
        // we are only interest `init.rc` file name file
        return false;
    }
    
    dpath = d_path(&fp->f_path, path, sizeof(path));

    if (IS_ERR(dpath)) {
        return false;
    }

    if (!!strcmp(dpath, "/system/etc/init/atrace.rc")) {
        return false;
    }

    return true;
}

// NOTE: https://github.com/tiann/KernelSU/commit/df640917d11dd0eff1b34ea53ec3c0dc49667002
// - added 260110, seems needed for A16 QPR 3

typedef enum {
    STAT_NATIVE, // struct stat
    STAT_COMPAT, // struct compat_stat
    STAT_STAT64 // struct stat64 // 32-bit uses this
} stat_type_t;

static __always_inline void injector_common_newfstat_ret(unsigned long fd_long,
                                                    void **statbuf_ptr,
                                                    const int type)
{
    struct file *file = NULL;
    uintptr_t statbuf_ptr_local = NULL;
    void __user *statbuf = NULL;
    void __user *st_size_ptr = NULL;
    long size, new_size;
    size_t len;


    if (!injector_init_rc_hook) {
        return;
    }

    if (!is_init(get_current_cred()))
        return;

    file = fget(fd_long);
    if (!file)
        return;

    if (!is_atrace_rc(file)) {
        fput(file);
        return;
    }
    fput(file);

    pr_info("%s: stat atrace.rc \n", __func__);

    statbuf_ptr_local = (uintptr_t) * (void **)statbuf_ptr;
    statbuf = (void __user *)statbuf_ptr_local;
    if (!statbuf)
        return;

    st_size_ptr = statbuf + offsetof(struct stat, st_size);
    len = sizeof(long);

#if defined(__ARCH_WANT_STAT64) || defined(__ARCH_WANT_COMPAT_STAT64)
    if (type) {
        st_size_ptr = statbuf + offsetof(struct stat64, st_size);
        len = sizeof(long long);
    }
#endif

    if (copy_from_user(&size, st_size_ptr, len)) {
        pr_info("%s: read statbuf 0x%lx failed \n", __func__,
                (unsigned long)st_size_ptr);
        return;
    }

    new_size = size + injector_rc_len;
    pr_info("%s: adding injector_rc_len: %ld -> %ld \n", __func__, size, new_size);

    if (!copy_to_user(st_size_ptr, &new_size, len))
        pr_info("%s: added injector_rc_len \n", __func__);
    else
        pr_info("%s: add injector_rc_len failed: statbuf 0x%lx \n", __func__,
                (unsigned long)st_size_ptr);

    return;
}

void injector_handle_newfstat_ret(unsigned int *fd, struct stat __user **statbuf_ptr)
{
    unsigned long fd_long = (unsigned long)*fd;

    // native
    injector_common_newfstat_ret(fd_long, (void **)statbuf_ptr, STAT_NATIVE);
}

#if defined(__ARCH_WANT_STAT64) || defined(__ARCH_WANT_COMPAT_STAT64)
void injector_handle_fstat64_ret(unsigned long *fd,
                            struct stat64 __user **statbuf_ptr)
{
    unsigned long fd_long = (unsigned long)*fd;

    // 32-bit call uses this!
    injector_common_newfstat_ret(fd_long, (void **)statbuf_ptr, STAT_STAT64);
}
#endif

static void stop_init_rc_hook(void)
{
    injector_init_rc_hook = false;
    pr_info("stop init_rc_hook!\n");
}

static void injector_handle_initrc(struct file *file)
{
    static bool rc_hooked = false;

    if (!file) {
        return;
    }

    if (!is_init(get_current_cred()))
        return;

    if (!is_atrace_rc(file)) {
        return;
    }

    // we only process the first read
    if (rc_hooked) {
        // we don't need these kprobe, unregister it!
        stop_init_rc_hook();
        return;
    }
    rc_hooked = true;

    // now we can sure that the init process is reading
    // `/system/etc/init/atrace.rc`

    pr_info("read atrace.rc, comm: %s, rc_count: %zu\n", current->comm,
            injector_rc_len);

    // Now we need to proxy the read and modify the result!
    // But, we can not modify the file_operations directly, because it's in read-only memory.
    // We just replace the whole file_operations with a proxy one.
    memcpy(&fops_proxy, file->f_op, sizeof(struct file_operations));
    orig_read = file->f_op->read;
    if (orig_read) {
        fops_proxy.read = read_proxy;
    }
    orig_read_iter = file->f_op->read_iter;
    if (orig_read_iter) {
        fops_proxy.read_iter = read_iter_proxy;
    }
    // replace the file_operations
    file->f_op = &fops_proxy;
}

static int injector_file_permission(struct file *file, int mask)
{
    if (!injector_init_rc_hook)
        return 0;

    injector_handle_initrc(file);

    return 0;
}

static struct security_hook_list injector_hooks[] = {
    LSM_HOOK_INIT(file_permission, injector_file_permission),
};

int __init injector_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
    security_add_hooks(injector_hooks, ARRAY_SIZE(injector_hooks), "injector");
#else
    // https://elixir.bootlin.com/linux/v4.10.17/source/include/linux/lsm_hooks.h#L1892
    security_add_hooks(injector_hooks, ARRAY_SIZE(injector_hooks));
#endif
    return 0;
}

void injector_exit(void)
{

}

module_init(injector_init);
module_exit(injector_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AlexLiuDev233");
MODULE_DESCRIPTION("Inject Custom RC File to Android System");