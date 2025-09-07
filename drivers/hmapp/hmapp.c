#include <linux/uaccess.h>
#include <uapi/linux/limits.h>   // For PATH_MAX
#include <linux/cred.h>


#define TARGET_PATH "/storage/emulated/0/Android/data/"
#define TARGET_PATH_LEN (sizeof(TARGET_PATH) - 1)
// 内置 deny list
static const char *deny_list[] = {
    "com.silverlab.app.deviceidchanger.free",
    "me.bingyue.IceCore",
    "com.modify.installer",
    "o.dyoo",
    "com.zhufucdev.motion_emulator",
    "me.weishu.kernelsu",
    "me.simpleHook",
    "com.cshlolss.vipkill",
    "io.github.a13e300.ksuwebui",
    "com.demo.serendipity",
    "me.iacn.biliroaming",
    "me.teble.xposed.autodaily",
    "com.example.ourom",
    "dialog.box",
    "top.hookvip.pro",
    "tornaco.apps.shortx",
    "moe.fuqiuluo.portal",
    "com.github.tianma8023.xposed.smscode",
    "moe.shizuku.privileged.api",
    "lin.xposed",
    "com.lerist.fakelocation",
    "com.yxer.packageinstalles",
    "xzr.hkf",
    "web1n.stopapp",
    "Hook.JiuWu.Xp",
    "io.github.qauxv",
    "com.houvven.guise",
    "xzr.konabess",
    "com.xayah.databackup.foss",
    "com.sevtinge.hyperceiler",
    "github.tornaco.android.thanos",
    "nep.timeline.freezer",
    "cn.geektang.privacyspace",
    "org.lsposed.lspatch",
    "zako.zako.zako",
    "com.topmiaohan.hidebllist",
    "com.tsng.hidemyapplist",
    "com.tsng.pzyhrx.hma",
    "com.rifsxd.ksunext",
    "com.byyoung.setting",
    "com.omarea.vtools",
    "cn.myflv.noactive",
    "io.github.vvb2060.magisk",
    "com.bug.hookvip",
    "com.junge.algorithmAidePro",
    "bin.mt.termex",
    "tmgp.atlas.toolbox",
    "com.wn.app.np",
    "com.sukisu.ultra",
    "ru.maximoff.apktool",
    "top.bienvenido.saas.i18n",
    "com.syyf.quickpay",
    "tornaco.apps.shortx.ext",
    "com.mio.kitchen",
    "eu.faircode.xlua",
    "com.dna.tools",
    "cn.myflv.monitor.noactive",
    "com.yuanwofei.cardemulator.pro",
    "com.termux",
    "com.suqi8.oshin",
    "me.hd.wauxv",
    "have.fun",
    "miko.client",
    "com.kooritea.fcmfix",
    "com.twifucker.hachidori",
    "com.luckyzyx.luckytool",
    "com.padi.hook.hookqq",
    "cn.lyric.getter",
    "com.parallelc.micts",
    "me.plusne",
    "com.hchen.appretention",
    "com.hchen.switchfreeform",
    "name.monwf.customiuizer",
    "com.houvven.impad",
    "cn.aodlyric.xiaowine",
    "top.sacz.timtool",
    "nep.timeline.re_telegram",
    "com.fuck.android.rimet",
    "cn.kwaiching.hook",
    "cn.android.x",
    "cc.aoeiuv020.iamnotdisabled.hook",
    "vn.kwaiching.tao",
    "com.nnnen.plusne",
    "com.fkzhang.wechatxposed",
    "one.yufz.hmspush",
    "cn.fuckhome.xiaowine",
    "com.fankes.tsbattery",
    "com.rifsxd.ksunext",
    "com.rkg.IAMRKG",
    "me.gm.cleaner",
    "moe.shizuku.redirectstorage",
    "com.ddm.qute",
    "io.github.vvb2060.magisk",
    "kk.dk.anqu",
    "com.qq.qcxm",
    "com.wei.vip",
    "dknb.con",
    "dknb.coo8",
    "com.tencent.jingshi",
    "com.tencent.JYNB",
    "com.apocalua.run",
    "com.coderstory.toolkit",
    "com.didjdk.adbhelper",
    "org.lsposed.manager",
    "io.github.Retmon403.oppotheme",
    "com.fankes.enforcehighrefreshrate",
    "es.chiteroman.bootloaderspoofer",
    "com.hchai.rescueplan",
};
#define DENY_LIST_SIZE (sizeof(deny_list)/sizeof(deny_list[0]))

#ifdef CONFIG_HMA_PP_KSU
extern bool ksu_uid_should_umount(uid_t uid);
#endif

// 检查路径是否命中 deny_list
static inline int is_in_deny_list(const char *path, uid_t caller) {
    // path 形如 /storage/emulated/0/Android/data/com.xxx.xxx
    // 需提取包名部分
    const char *p = path;
    size_t prefix_len = strlen(TARGET_PATH);
    char *pkg;
    char pkgname[128];
    size_t i = 0;

    if (strncmp(p, TARGET_PATH, prefix_len) != 0) return 0;
#ifdef CONFIG_HMA_PP_KSU
    if (!ksu_uid_should_umount(caller)) return 0; // 遵守排除列表
#endif 
    pkg = p + prefix_len;
    // 只取包名部分（遇到 / 或 \ 或字符串结尾
    while (*pkg && *pkg != '/' && *pkg != '\\' && i < sizeof(pkgname) - 1) {
        pkgname[i++] = *pkg++;
    }
    pkgname[i] = '\0';

    for (i = 0; i < DENY_LIST_SIZE; ++i) {
        if (strcmp(pkgname, deny_list[i]) == 0) return 1;
    }
    return 0;
}

inline int hmapp_check_path(const char __user *pathname) {
    char filename_kernel[PATH_MAX];
    long len;
    uid_t caller = current_uid().val;

    len = strncpy_from_user(filename_kernel, pathname, sizeof(filename_kernel));
    if (len <= 0 || len >= sizeof(filename_kernel)) {
        return 0; // Let the original syscall handle invalid paths or too long paths
    }
    filename_kernel[len] = '\0'; // Ensure null-termination

    if (strncmp(filename_kernel, TARGET_PATH, TARGET_PATH_LEN) == 0) {
        // 只拦名单，其它一律放行
        if (unlikely(is_in_deny_list(filename_kernel, caller))) {
            pr_warn("[HMA++]mkdirat: Denied by deny_list to create %s\n", filename_kernel);
            return 1;
        }
    }
    return 0;
}

