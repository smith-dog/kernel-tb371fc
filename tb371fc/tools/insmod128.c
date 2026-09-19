/* insmod128 — finit_module loader with MODULE_INIT_IGNORE_MODVERSIONS (1)
 * 用途：内核开 CONFIG_MODVERSIONS=y（VINTF 兼容矩阵要求）后，Lenovo vendor
 * 模块（无 CRC 表或 CRC 不匹配）仍需加载。vermagic 检查保留（p116 体系）。
 * 用法：insmod128 /path/to/mod.ko [module args...]
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#ifndef MODULE_INIT_IGNORE_MODVERSIONS
#define MODULE_INIT_IGNORE_MODVERSIONS 1
#endif
#ifndef MODULE_INIT_IGNORE_VERMAGIC
#define MODULE_INIT_IGNORE_VERMAGIC 2
#endif
/* Lenovo vendor modules: vermagic lacks 'modversions' (same-length patch
 * impossible) and carry no CRC table -> ignore both (flags=3). ABI is
 * preserved by our module-compat config work (p115/p116 lineage). */
#define LENOMO_FLAGS (MODULE_INIT_IGNORE_MODVERSIONS | MODULE_INIT_IGNORE_VERMAGIC)

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s module.ko [args...]\n", argv[0]);
        return 2;
    }
    int fd = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "open %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
    /* concat args with spaces (params), kernel wants one params string */
    char params[4096] = "";
    for (int i = 2; i < argc; i++) {
        strncat(params, argv[i], sizeof(params) - strlen(params) - 2);
        if (i + 1 < argc) strncat(params, " ", sizeof(params) - strlen(params) - 2);
    }
    long rc = syscall(273 /* __NR_finit_module on arm64 (asm-generic) */, fd,
                      argc > 2 ? params : "", LENOMO_FLAGS);
    if (rc != 0) {
        fprintf(stderr, "finit_module %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
    return 0;
}
