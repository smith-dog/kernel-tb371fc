#!/usr/bin/env python3
"""p222b - fix p222 instrumentation: soc is void* in cds_api.c scope, cast to
ol_txrx_soc_handle before dereferencing ops. Replaces the broken lines."""
P = "/home/smith/t34/qcacld-3.0/core/cds/src/cds_api.c"
src = open(P).read()
FIX = [
 ("(void *)soc, (soc && soc->ops) ? (void *)soc->ops : NULL,",
  "(void *)soc, (soc && ((ol_txrx_soc_handle)soc)->ops) ? (void *)((ol_txrx_soc_handle)soc)->ops : NULL,"),
 ("(soc && soc->ops) ? (void *)soc->ops->cfg_ops : NULL,",
  "(soc && ((ol_txrx_soc_handle)soc)->ops) ? (void *)((ol_txrx_soc_handle)soc)->ops->cfg_ops : NULL,"),
 ("(soc && soc->ops && soc->ops->cfg_ops) ? (void *)soc->ops->cfg_ops->cfg_attach : NULL);",
  "(soc && ((ol_txrx_soc_handle)soc)->ops && ((ol_txrx_soc_handle)soc)->ops->cfg_ops) ? (void *)((ol_txrx_soc_handle)soc)->ops->cfg_ops->cfg_attach : NULL);"),
]
for o, n2 in FIX:
    assert src.count(o) == 1, "fix anchor x%d: %s" % (src.count(o), o[:40])
    src = src.replace(o, n2, 1)
open(P, "w").write(src)
print("p222b: casts fixed")
