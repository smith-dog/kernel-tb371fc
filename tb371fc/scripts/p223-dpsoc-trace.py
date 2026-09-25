#!/usr/bin/env python3
"""p223 - TASK-034 P3: trace dp_soc lifetime inside cds_open. Prints
target_type + gp_cds_context->dp_soc right after the attach (line ~678) and
right before cds_cdp_cfg_attach (line ~690), plus gp_cds_context pointer in
both places, to locate where/why the SOC context reads NULL. Idempotent."""

P = "/home/smith/t34/qcacld-3.0/core/cds/src/cds_api.c"
NL = chr(10)
T = chr(9)

A_OLD = (T + "if (!gp_cds_context->dp_soc) {" + NL +
         T + T + "status = QDF_STATUS_E_FAILURE;")
A_NEW = (T + "WMA_LOGP(\"p223a: gp=%pK target=%d dp_soc=%pK\", (void *)gp_cds_context, hdd_ctx->target_type, (void *)gp_cds_context->dp_soc);" + NL +
         A_OLD)

B_OLD = T + "cds_cdp_cfg_attach(psoc);"
B_NEW = (T + "WMA_LOGP(\"p223b: gp=%pK dp_soc=%pK\", (void *)gp_cds_context, (void *)gp_cds_context->dp_soc);" + NL +
         B_OLD)

src = open(P).read()
if "p223a" in src:
    print("p223: ALREADY patched")
else:
    assert src.count(A_OLD) == 1, "anchorA x%d" % src.count(A_OLD)
    assert src.count(B_OLD) == 1, "anchorB x%d" % src.count(B_OLD)
    src = src.replace(A_OLD, A_NEW, 1).replace(B_OLD, B_NEW, 1)
    open(P, "w").write(src)
    print("p223: dp_soc lifetime probes inserted")
print("p223: OK")
