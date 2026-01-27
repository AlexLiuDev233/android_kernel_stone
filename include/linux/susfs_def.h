#ifndef SUSFS_DEF_H
#define SUSFS_DEF_H

#include <linux/bits.h>

/* shared with userspace ksu_susfs tool */
#define SUSFS_MAGIC 0xFAFAFAFA
#define CMD_SUSFS_ADD_SUS_PATH 0x55550
#define CMD_SUSFS_SET_ANDROID_DATA_ROOT_PATH 0x55551
#define CMD_SUSFS_SET_SDCARD_ROOT_PATH 0x55552
#define CMD_SUSFS_ADD_SUS_PATH_LOOP 0x55553
#define CMD_SUSFS_ENABLE_LOG 0x555a0
#define CMD_SUSFS_SHOW_VERSION 0x555e1
#define CMD_SUSFS_SHOW_ENABLED_FEATURES 0x555e2
#define CMD_SUSFS_SHOW_VARIANT 0x555e3

#define SUSFS_MAX_LEN_PATHNAME 256 // 256 should address many paths already unless you are doing some strange experimental stuff, then set your own desired length
#define SUSFS_ENABLED_FEATURES_SIZE 8192 // 8192 is enough I guess
#define SUSFS_MAX_VERSION_BUFSIZE 16
#define SUSFS_MAX_VARIANT_BUFSIZE 16

/*
 * mount->mnt.susfs_mnt_id_backup => storing original mount's mnt_id
 * inode->i_mapping->flags => A 'unsigned long' type storing flag 'AS_FLAGS_', bit 1 to 31 is not usable since 6.12
 * nd->state => storing flag 'ND_STATE_'
 * nd->flags => storing flag 'ND_FLAGS_'
 * task_struct->thread_info.flags => storing flag 'TIF_'
 */
 // thread_info->flags is unsigned long :D
#define TIF_PROC_UMOUNTED 33

#define AS_FLAGS_SUS_PATH 33
#define AS_FLAGS_ANDROID_DATA_ROOT_DIR 37
#define AS_FLAGS_SDCARD_ROOT_DIR 38
#define BIT_SUS_PATH BIT(33)
#define BIT_ANDROID_DATA_ROOT_DIR BIT(37)
#define BIT_ANDROID_SDCARD_ROOT_DIR BIT(38)

#define ND_STATE_LOOKUP_LAST 32
#define ND_STATE_OPEN_LAST 64
#define ND_STATE_LAST_SDCARD_SUS_PATH 128
#define ND_FLAGS_LOOKUP_LAST		0x2000000
#endif // #ifndef KSU_SUSFS_DEF_H
