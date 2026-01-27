#include <linux/version.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/printk.h>
#include <linux/namei.h>
#include <linux/list.h>
#include <linux/init_task.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/fdtable.h>
#include <linux/statfs.h>
#include <linux/random.h>
#include <linux/susfs.h>
#include <linux/thread_info.h>

#define KSU_INSTALL_MAGIC1 0xDEADBEEF
#ifdef CONFIG_SUSFS_ENABLE_LOG
bool susfs_is_log_enabled __read_mostly = true;
#define SUSFS_LOGI(fmt, ...) if (susfs_is_log_enabled) pr_info("susfs:[%u][%d][%s] " fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)
#define SUSFS_LOGE(fmt, ...) if (susfs_is_log_enabled) pr_err("susfs:[%u][%d][%s]" fmt, current_uid().val, current->pid, __func__, ##__VA_ARGS__)
#else
#define SUSFS_LOGI(fmt, ...) 
#define SUSFS_LOGE(fmt, ...) 
#endif

bool susfs_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str++ != *prefix++)
            return false;
    }
    return true;
}

static inline bool susfs_is_current_proc_umounted(void) {
	return test_ti_thread_flag(&current->thread_info, TIF_PROC_UMOUNTED);
}

static inline void susfs_set_current_proc_umounted(void) {
	set_ti_thread_flag(&current->thread_info, TIF_PROC_UMOUNTED);
}

extern bool ksu_uid_should_umount(uid_t uid);
extern bool is_zygote(const struct cred *cred);
#ifdef CONFIG_SUSFS_SUS_PATH
extern void susfs_run_sus_path_loop(uid_t uid);
#endif // #ifdef CONFIG_SUSFS_SUS_PATH

static inline bool is_zygote_isolated_service_uid(uid_t uid)
{
    uid %= 100000;
    return (uid >= 99000 && uid < 100000);
}

static inline bool is_zygote_normal_app_uid(uid_t uid)
{
    uid %= 100000;
    return (uid >= 10000 && uid < 19999);
}

#ifdef CONFIG_SUSFS_SUS_PATH
static DEFINE_SPINLOCK(susfs_spin_lock_sus_path);
static LIST_HEAD(LH_SUS_PATH_LOOP);
static LIST_HEAD(LH_SUS_PATH_ANDROID_DATA);
static LIST_HEAD(LH_SUS_PATH_SDCARD);
static struct st_external_dir android_data_path = {0};
static struct st_external_dir sdcard_path = {0};
const struct qstr susfs_fake_qstr_name = QSTR_INIT("..5.u.S", 7); // used to re-test the dcache lookup, make sure you don't have file named like this!!

void susfs_set_i_state_on_external_dir(void __user **user_info) {
	struct path path;
	struct inode *inode = NULL;
	static struct st_external_dir info = {0};

	if (copy_from_user(&info, (struct st_external_dir __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	info.err = kern_path(info.target_pathname, LOOKUP_FOLLOW, &path);
	if (info.err) {
		SUSFS_LOGE("Failed opening file '%s'\n", info.target_pathname);
		goto out_copy_to_user;
	}

	inode = d_inode(path.dentry);
	if (!inode) {
		info.err = -EINVAL;
		goto out_path_put_path;
	}
	
	if (info.cmd == CMD_SUSFS_SET_ANDROID_DATA_ROOT_PATH) {
		spin_lock(&inode->i_lock);
		set_bit(AS_FLAGS_ANDROID_DATA_ROOT_DIR, &inode->i_mapping->flags);
		spin_unlock(&inode->i_lock);
		strncpy(android_data_path.target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME-1);
		android_data_path.is_inited = true;
		android_data_path.cmd = CMD_SUSFS_SET_ANDROID_DATA_ROOT_PATH;
		SUSFS_LOGI("Set android data root dir: '%s', i_mapping: '0x%p'\n",
			android_data_path.target_pathname, inode->i_mapping);
		info.err = 0;
	} else if (info.cmd == CMD_SUSFS_SET_SDCARD_ROOT_PATH) {
		spin_lock(&inode->i_lock);
		set_bit(AS_FLAGS_SDCARD_ROOT_DIR, &inode->i_mapping->flags);
		spin_unlock(&inode->i_lock);
		strncpy(sdcard_path.target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME-1);
		sdcard_path.is_inited = true;
		sdcard_path.cmd = CMD_SUSFS_SET_SDCARD_ROOT_PATH;
		SUSFS_LOGI("Set sdcard root dir: '%s', i_mapping: '0x%p'\n",
			sdcard_path.target_pathname, inode->i_mapping);
		info.err = 0;
	} else {
		info.err = -EINVAL;
	}

out_path_put_path:
	path_put(&path);
out_copy_to_user:
	if (copy_to_user(&((struct st_external_dir __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	if (info.cmd == CMD_SUSFS_SET_ANDROID_DATA_ROOT_PATH) {
		SUSFS_LOGI("CMD_SUSFS_SET_ANDROID_DATA_ROOT_PATH -> ret: %d\n", info.err);
	} else if (info.cmd == CMD_SUSFS_SET_SDCARD_ROOT_PATH) {
		SUSFS_LOGI("CMD_SUSFS_SET_SDCARD_ROOT_PATH -> ret: %d\n", info.err);
	}
}

void susfs_add_sus_path(void __user **user_info) {
	struct st_susfs_sus_path_list *new_list = NULL;
	struct st_susfs_sus_path info = {0};
	struct path path;
	struct inode *inode = NULL;

	if (copy_from_user(&info, (struct st_susfs_sus_path __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	info.err = kern_path(info.target_pathname, 0, &path);
	if (info.err) {
		SUSFS_LOGE("Failed opening file '%s'\n", info.target_pathname);
		goto out_copy_to_user;
	}

	if (!path.dentry->d_inode) {
		info.err = -EINVAL;
		goto out_path_put_path;
	}
	inode = d_inode(path.dentry);

	if (strstr(info.target_pathname, android_data_path.target_pathname)) {
		if (!android_data_path.is_inited) {
			info.err = -EINVAL;
			SUSFS_LOGE("android_data_path is not configured yet, plz do like 'ksu_susfs set_android_data_root_path /sdcard/Android/data' first after your screen is unlocked\n");
			goto out_path_put_path;
		}
		new_list = kmalloc(sizeof(struct st_susfs_sus_path_list), GFP_KERNEL);
		if (!new_list) {
			info.err = -ENOMEM;
			goto out_path_put_path;
		}
		new_list->info.target_ino = info.target_ino;
		strncpy(new_list->info.target_pathname, path.dentry->d_name.name, SUSFS_MAX_LEN_PATHNAME - 1);
		strncpy(new_list->target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME - 1);
		new_list->info.i_uid = info.i_uid;
		new_list->path_len = strlen(new_list->info.target_pathname);
		INIT_LIST_HEAD(&new_list->list);
		spin_lock(&susfs_spin_lock_sus_path);
		list_add_tail(&new_list->list, &LH_SUS_PATH_ANDROID_DATA);
		spin_unlock(&susfs_spin_lock_sus_path);
		SUSFS_LOGI("target_ino: '%lu', target_pathname: '%s', i_uid: '%u', is successfully added to LH_SUS_PATH_ANDROID_DATA\n",
					new_list->info.target_ino, new_list->target_pathname, new_list->info.i_uid);
		info.err = 0;
		goto out_path_put_path;
	} else if (strstr(info.target_pathname, sdcard_path.target_pathname)) {
		if (!sdcard_path.is_inited) {
			info.err = -EINVAL;
			SUSFS_LOGE("sdcard_path is not configured yet, plz do like 'ksu_susfs set_sdcard_root_path /sdcard' first after your screen is unlocked\n");
			goto out_path_put_path;
		}
		new_list = kmalloc(sizeof(struct st_susfs_sus_path_list), GFP_KERNEL);
		if (!new_list) {
			info.err = -ENOMEM;
			goto out_path_put_path;
		}
		new_list->info.target_ino = info.target_ino;
		strncpy(new_list->info.target_pathname, path.dentry->d_name.name, SUSFS_MAX_LEN_PATHNAME - 1);
		strncpy(new_list->target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME - 1);
		new_list->info.i_uid = info.i_uid;
		new_list->path_len = strlen(new_list->info.target_pathname);
		INIT_LIST_HEAD(&new_list->list);
		spin_lock(&susfs_spin_lock_sus_path);
		list_add_tail(&new_list->list, &LH_SUS_PATH_SDCARD);
		spin_unlock(&susfs_spin_lock_sus_path);
		SUSFS_LOGI("target_ino: '%lu', target_pathname: '%s', i_uid: '%u', is successfully added to LH_SUS_PATH_SDCARD\n",
					new_list->info.target_ino, new_list->target_pathname, new_list->info.i_uid);
		info.err = 0;
		goto out_path_put_path;
	}

	spin_lock(&inode->i_lock);
	set_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags);
	spin_unlock(&inode->i_lock);
	SUSFS_LOGI("pathname: '%s', ino: '%lu', is flagged as AS_FLAGS_SUS_PATH\n", info.target_pathname, info.target_ino);
	info.err = 0;
out_path_put_path:
	path_put(&path);
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_sus_path __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_ADD_SUS_PATH -> ret: %d\n", info.err);
}

void susfs_add_sus_path_loop(void __user **user_info) {
	struct st_susfs_sus_path_list *new_list = NULL;
	struct st_susfs_sus_path info = {0};
	struct path path;
	struct inode *inode = NULL;

	if (copy_from_user(&info, (struct st_susfs_sus_path __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	info.err = kern_path(info.target_pathname, 0, &path);
	if (info.err) {
		SUSFS_LOGE("Failed opening file '%s'\n", info.target_pathname);
		goto out_copy_to_user;
	}

	if (!path.dentry->d_inode) {
		info.err = -EINVAL;
		goto out_path_put_path;
	}
	inode = d_inode(path.dentry);

	if (susfs_starts_with(info.target_pathname, "/storage/") ||
		susfs_starts_with(info.target_pathname, "/sdcard/"))
	{
		info.err = -EINVAL;
		SUSFS_LOGE("path starts with /storage and /sdcard cannot be added by add_sus_path_loop\n");
		goto out_path_put_path;
	}

	new_list = kmalloc(sizeof(struct st_susfs_sus_path_list), GFP_KERNEL);
	if (!new_list) {
		info.err = -ENOMEM;
		goto out_path_put_path;
	}
	new_list->info.target_ino = info.target_ino;
	strncpy(new_list->info.target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME - 1);
	strncpy(new_list->target_pathname, info.target_pathname, SUSFS_MAX_LEN_PATHNAME - 1);
	new_list->info.i_uid = info.i_uid;
	new_list->path_len = strlen(new_list->info.target_pathname);
	INIT_LIST_HEAD(&new_list->list);
	spin_lock(&susfs_spin_lock_sus_path);
	list_add_tail(&new_list->list, &LH_SUS_PATH_LOOP);
	spin_unlock(&susfs_spin_lock_sus_path);
	SUSFS_LOGI("target_ino: '%lu', target_pathname: '%s', i_uid: '%u', is successfully added to LH_SUS_PATH_LOOP\n",
				new_list->info.target_ino, new_list->target_pathname, new_list->info.i_uid);
	spin_lock(&inode->i_lock);
	set_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags);
	spin_unlock(&inode->i_lock);
	SUSFS_LOGI("pathname: '%s', ino: '%lu', is flagged as AS_FLAGS_SUS_PATH\n", info.target_pathname, info.target_ino);
	info.err = 0;
out_path_put_path:
	path_put(&path);
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_sus_path __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_ADD_SUS_PATH_LOOP -> ret: %d\n", info.err);
}

void susfs_run_sus_path_loop(uid_t uid) {
	struct st_susfs_sus_path_list *cursor = NULL;
	struct path path;
	struct inode *inode;

	list_for_each_entry(cursor, &LH_SUS_PATH_LOOP, list) {
		if (!kern_path(cursor->target_pathname, 0, &path)) {
			inode = path.dentry->d_inode;
			spin_lock(&inode->i_lock);
			set_bit(AS_FLAGS_SUS_PATH, &inode->i_mapping->flags);
			spin_unlock(&inode->i_lock);
			path_put(&path);
			SUSFS_LOGI("re-flag '%s' as SUS_PATH for uid: %u\n", cursor->target_pathname, uid);
		}
	}
}

static inline bool is_i_uid_in_android_data_not_allowed(uid_t i_uid) {
	return (likely(susfs_is_current_proc_umounted()) &&
		unlikely(current_uid().val != i_uid));
}

static inline bool is_i_uid_in_sdcard_not_allowed(void) {
	return (likely(susfs_is_current_proc_umounted()));
}

static inline bool is_i_uid_not_allowed(uid_t i_uid) {
	return (likely(susfs_is_current_proc_umounted()) &&
		unlikely(current_uid().val != i_uid));
}

bool susfs_is_base_dentry_android_data_dir(struct dentry* base) {
	return (base && !IS_ERR(base) && base->d_inode && (base->d_inode->i_mapping->flags & BIT_ANDROID_DATA_ROOT_DIR));
}

bool susfs_is_base_dentry_sdcard_dir(struct dentry* base) {
	return (base && !IS_ERR(base) && base->d_inode && (base->d_inode->i_mapping->flags & BIT_ANDROID_SDCARD_ROOT_DIR));
}

bool susfs_is_sus_android_data_d_name_found(const char *d_name) {
	struct st_susfs_sus_path_list *cursor = NULL;

	if (d_name[0] == '\0') {
		return false;
	}

	list_for_each_entry(cursor, &LH_SUS_PATH_ANDROID_DATA, list) {
		// - we use strstr here because we cannot retrieve the dentry of fuse_dentry
		//   and attacker can still use path travesal attack to detect the path, but
		//   lucky we can check for the uid so it won't let them fool us
		if (!strncmp(d_name, cursor->info.target_pathname, cursor->path_len) &&
		    (d_name[cursor->path_len] == '\0' || d_name[cursor->path_len] == '/') &&
			is_i_uid_in_android_data_not_allowed(cursor->info.i_uid))
		{
			SUSFS_LOGI("hiding path '%s'\n", cursor->target_pathname);
			return true;
		}
	}
	return false;
}

bool susfs_is_sus_sdcard_d_name_found(const char *d_name) {
	struct st_susfs_sus_path_list *cursor = NULL;

	if (d_name[0] == '\0') {
		return false;
	}
	list_for_each_entry(cursor, &LH_SUS_PATH_SDCARD, list) {
		if (!strncmp(d_name, cursor->info.target_pathname, cursor->path_len) &&
		    (d_name[cursor->path_len] == '\0' || d_name[cursor->path_len] == '/') &&
			is_i_uid_in_sdcard_not_allowed())
		{
			SUSFS_LOGI("hiding path '%s'\n", cursor->target_pathname);
			return true;
		}
	}
	return false;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
bool susfs_is_inode_sus_path(struct mnt_idmap* idmap, struct inode *inode) {
	if (unlikely(inode->i_mapping->flags & BIT_SUS_PATH &&
		is_i_uid_not_allowed(i_uid_into_vfsuid(idmap, inode).val)))
	{
		SUSFS_LOGI("hiding path with ino '%lu'\n", inode->i_ino);
		return true;
	}
	return false;
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
bool susfs_is_inode_sus_path(struct inode *inode) {
	if (unlikely(inode->i_mapping->flags & BIT_SUS_PATH &&
		is_i_uid_not_allowed(i_uid_into_mnt(i_user_ns(inode), inode).val)))
	{
		SUSFS_LOGI("hiding path with ino '%lu'\n", inode->i_ino);
		return true;
	}
	return false;
}
#else
bool susfs_is_inode_sus_path(struct inode *inode) {
	if (unlikely(inode->i_mapping->flags & BIT_SUS_PATH &&
		is_i_uid_not_allowed(inode->i_uid.val)))
	{
		SUSFS_LOGI("hiding path with ino '%lu'\n", inode->i_ino);
		return true;
	}
	return false;
}
#endif

#endif // #ifdef CONFIG_SUSFS_SUS_PATH

#ifdef CONFIG_SUSFS_ENABLE_LOG
static DEFINE_SPINLOCK(susfs_spin_lock_enable_log);

void susfs_enable_log(void __user **user_info) {
	struct st_susfs_log info = {0};

	if (copy_from_user(&info, (struct st_susfs_log __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	spin_lock(&susfs_spin_lock_enable_log);
	susfs_is_log_enabled = info.enabled;
	spin_unlock(&susfs_spin_lock_enable_log);
	if (susfs_is_log_enabled) {
		pr_info("susfs: enable logging to kernel");
	} else {
		pr_info("susfs: disable logging to kernel");
	}
	info.err = 0;
out_copy_to_user:
	if (copy_to_user(&((struct st_susfs_log __user*)*user_info)->err, &info.err, sizeof(info.err))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_ENABLE_LOG -> ret: %d\n", info.err);
}
#endif // #ifdef CONFIG_SUSFS_ENABLE_LOG

/* get susfs enabled features */
static int copy_config_to_buf(const char *config_string, char *buf_ptr, size_t *copied_size, size_t bufsize) {
	size_t tmp_size = strlen(config_string);

	*copied_size += tmp_size;
	if (*copied_size >= bufsize) {
		SUSFS_LOGE("bufsize is not big enough to hold the string.\n");
		return -EINVAL;
	}
	strncpy(buf_ptr, config_string, tmp_size);
	return 0;
}

void susfs_get_enabled_features(void __user **user_info) {
	struct st_susfs_enabled_features *info = (struct st_susfs_enabled_features *)kzalloc(sizeof(struct st_susfs_enabled_features), GFP_KERNEL);
	char *buf_ptr = NULL;
	size_t copied_size = 0;

	if (!info) {
		info->err = -ENOMEM;
		goto out_copy_to_user;
	}

	if (copy_from_user(info, (struct st_susfs_enabled_features __user*)*user_info, sizeof(struct st_susfs_enabled_features))) {
		info->err = -EFAULT;
		goto out_copy_to_user;
	}

	buf_ptr = info->enabled_features;

#ifdef CONFIG_SUSFS_SUS_PATH
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_SUS_PATH\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif
#ifdef CONFIG_SUSFS_ENABLE_LOG
	info->err = copy_config_to_buf("CONFIG_KSU_SUSFS_ENABLE_LOG\n", buf_ptr, &copied_size, SUSFS_ENABLED_FEATURES_SIZE);
	if (info->err) goto out_copy_to_user;
	buf_ptr = info->enabled_features + copied_size;
#endif

	info->err = 0;
out_copy_to_user:
	if (copy_to_user((struct st_susfs_enabled_features __user*)*user_info, info, sizeof(struct st_susfs_enabled_features))) {
		info->err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_SHOW_ENABLED_FEATURES -> ret: %d\n", info->err);
	if (info) {
		kfree(info);
	}
}

/* show_variant */
void susfs_show_variant(void __user **user_info) {
	struct st_susfs_variant info = {0};

	if (copy_from_user(&info, (struct st_susfs_variant __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	strncpy(info.susfs_variant, SUSFS_VARIANT, SUSFS_MAX_VARIANT_BUFSIZE-1);
	info.err = 0;
out_copy_to_user:
	if (copy_to_user((struct st_susfs_variant __user*)*user_info, &info, sizeof(info))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_SHOW_VARIANT -> ret: %d\n", info.err);
}

/* show version */
void susfs_show_version(void __user **user_info) {
	struct st_susfs_version info = {0};

	if (copy_from_user(&info, (struct st_susfs_version __user*)*user_info, sizeof(info))) {
		info.err = -EFAULT;
		goto out_copy_to_user;
	}

	strncpy(info.susfs_version, SUSFS_VERSION, SUSFS_MAX_VERSION_BUFSIZE-1);
	info.err = 0;
out_copy_to_user:
	if (copy_to_user((struct st_susfs_version __user*)*user_info, &info, sizeof(info))) {
		info.err = -EFAULT;
	}
	SUSFS_LOGI("CMD_SUSFS_SHOW_VERSION -> ret: %d\n", info.err);
}

int susfs_handle_sys_reboot(int magic1, int magic2, unsigned int cmd, void __user **arg)
{
    if (magic1 != KSU_INSTALL_MAGIC1) {
        return -EINVAL; 
    }

    // If magic2 is susfs and current process is root
    if (magic2 == SUSFS_MAGIC && current_uid().val == 0) {
#ifdef CONFIG_SUSFS_SUS_PATH
        if (cmd == CMD_SUSFS_ADD_SUS_PATH) {
            susfs_add_sus_path(arg);
            return 0;
        }
        if (cmd == CMD_SUSFS_ADD_SUS_PATH_LOOP) {
            susfs_add_sus_path_loop(arg);
            return 0;
        }
        if (cmd == CMD_SUSFS_SET_ANDROID_DATA_ROOT_PATH) {
            susfs_set_i_state_on_external_dir(arg);
            return 0;
        }
        if (cmd == CMD_SUSFS_SET_SDCARD_ROOT_PATH) {
            susfs_set_i_state_on_external_dir(arg);
            return 0;
        }
#endif //#ifdef CONFIG_SUSFS_SUS_PATH
#ifdef CONFIG_SUSFS_ENABLE_LOG
        if (cmd == CMD_SUSFS_ENABLE_LOG) {
            susfs_enable_log(arg);
            return 0;
        }
#endif //#ifdef CONFIG_SUSFS_ENABLE_LOG
        if (cmd == CMD_SUSFS_SHOW_ENABLED_FEATURES) {
            susfs_get_enabled_features(arg);
           return 0;
        }
       if (cmd == CMD_SUSFS_SHOW_VARIANT) {
            susfs_show_variant(arg);
            return 0;
        }
        if (cmd == CMD_SUSFS_SHOW_VERSION) {
            susfs_show_version(arg);
            return 0;
        }
        return 0;
    }
    return 0;
}

int susfs_handle_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
    uid_t new_uid = ruid;

    // We only interest in process spwaned by zygote
    if (!is_zygote(current_cred())) {
        return 0;
    }
    // Check if spawned process is isolated service first, and force to do umount if so
    if (is_zygote_isolated_service_uid(new_uid)) {
        goto do_umount;
    }

    if (likely(is_zygote_normal_app_uid(new_uid) &&
               ksu_uid_should_umount(new_uid))) {
        goto do_umount;
    }

    return 0;

do_umount:
    susfs_set_current_proc_umounted();

#ifdef CONFIG_SUSFS_SUS_PATH
    susfs_run_sus_path_loop(new_uid);
#endif // #ifdef CONFIG_SUSFS_SUS_PATH
    return 0;
}
