#ifndef SUSFS_H
#define SUSFS_H

#include <linux/types.h>
#include <linux/susfs_def.h>

#define SUSFS_VERSION "v2.0.0"
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
#define SUSFS_VARIANT "NON-GKI"
#else
#define SUSFS_VARIANT "GKI"
#endif

#ifdef CONFIG_SUSFS_SUS_PATH
struct st_susfs_sus_path {
	unsigned long                           target_ino;
	char                                    target_pathname[SUSFS_MAX_LEN_PATHNAME];
	unsigned int                            i_uid;
	int                                     err;
};

struct st_susfs_sus_path_list {
	struct list_head                        list;
	struct st_susfs_sus_path                info;
	char                                    target_pathname[SUSFS_MAX_LEN_PATHNAME];
	size_t                                  path_len;
};

struct st_external_dir {
	char                                    target_pathname[SUSFS_MAX_LEN_PATHNAME];
	bool                                    is_inited;
	int                                     cmd;
	int                                     err;
};

void susfs_set_i_state_on_external_dir(void __user **user_info);
void susfs_add_sus_path(void __user **user_info);
void susfs_add_sus_path_loop(void __user **user_info);
#endif

/* enable_log */
#ifdef CONFIG_SUSFS_ENABLE_LOG
struct st_susfs_log {
	bool                                    enabled;
	int                                     err;
};
#endif

/* get enabled features */
struct st_susfs_enabled_features {
	char                                    enabled_features[SUSFS_ENABLED_FEATURES_SIZE];
	int                                     err;
};

/* show variant */
struct st_susfs_variant {
	char                                    susfs_variant[16];
	int                                     err;
};

/* show version */
struct st_susfs_version {
	char                                    susfs_version[16];
	int                                     err;
};

#ifdef CONFIG_KSU_SUSFS_ENABLE_LOG
void susfs_enable_log(void __user **user_info);
#endif

void susfs_show_variant(void __user **user_info);
void susfs_show_version(void __user **user_info);

#endif