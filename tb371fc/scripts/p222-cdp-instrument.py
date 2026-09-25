#!/usr/bin/env python3
"""p222 - TASK-034 P3: instrument cds_cdp_cfg_attach's NULL branch to print
which of the three cdp_cfg_attach NULL exits fired (soc/ops missing,
cfg_ops missing, or underlying cfg_attach returned NULL). WMA_LOGP is
proven visible (p221). Idempotent."""

P = "/home/smith/t34/qcacld-3.0/core/cds/src/cds_api.c"
NL = chr(10)
T = chr(9)
OLD = (T + "if (!gp_cds_context->cfg_ctx) {" + NL +
       T + T + "WMA_LOGP(\"%s: failed to init cfg handle\", __func__);")
NEW = (T + "if (!gp_cds_context->cfg_ctx) {" + NL +
       T + T + "WMA_LOGP(\"p222: soc=%pK ops=%pK cfg_ops=%pK cfg_attach=%pK\", " + NL +
       T + T + T + "(void *)soc, (soc && soc->ops) ? (void *)soc->ops : NULL, " + NL +
       T + T + T + "(soc && soc->ops) ? (void *)soc->ops->cfg_ops : NULL, " + NL +
       T + T + T + "(soc && soc->ops && soc->ops->cfg_ops) ? (void *)soc->ops->cfg_ops->cfg_attach : NULL);" + NL +
       T + T + "WMA_LOGP(\"%s: failed to init cfg handle\", __func__);")
src = open(P).read()
if "p222:" in src:
    print("p222: ALREADY patched")
else:
    n = src.count(OLD)
    assert n == 1, "anchor x%d" % n
    open(P, "w").write(src.replace(OLD, NEW, 1))
    print("p222: instrumentation inserted")
print("p222: OK")
