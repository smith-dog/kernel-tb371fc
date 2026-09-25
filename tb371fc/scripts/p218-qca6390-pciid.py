#!/usr/bin/env python3
"""p218 - TASK-034 P3: add qca6390 (17cb:1101) to the qcacld pld PCIe id table.

The 2019-08-30 qcacld-3.0 snapshot (v5.2.0.146F) carries only legacy Atheros
ids in pld_pcie_id_table; the QCA6390 (17cb:1101) entry was added upstream
right after this snapshot. cnss_wlan_register_driver() then rejects the
driver with 'PCIe device id is 1101, not supported by loading driver'.
Also adds a temporary build warning to verify the kernel config visibility
of the chipset gate variables. Idempotent."""

P = "/home/smith/t34/qcacld-3.0/Kbuild"
NL = chr(10)

# --- 1) id table entry ---
F = "/home/smith/t34/qcacld-3.0/core/pld/src/pld_pcie.c"
src = open(F).read()
if "0x17cb, 0x1101" in src:
    print("p218: pld_pcie id table ALREADY has 1101")
else:
    OLD = "\t{ 0x168c, 0x7021, PCI_ANY_ID, PCI_ANY_ID },"
    n = src.count(OLD)
    assert n == 1, "anchor x%d" % n
    new = OLD + NL + "\t{ 0x17cb, 0x1101, PCI_ANY_ID, PCI_ANY_ID }, /* p218: qca6390 */"
    open(F, "w").write(src.replace(OLD, new, 1))
    print("p218: pld_pcie id table gained 17cb:1101")

# --- 2) temporary gate-visibility warning in Kbuild ---
src = open(P).read()
if "P3DBG" in src:
    print("p218: Kbuild warning ALREADY present")
else:
    OLD = "include $(WLAN_ROOT)/configs/$(CONFIG_QCA_CLD_WLAN_PROFILE)_defconfig"
    n = src.count(OLD)
    assert n == 1, "anchor x%d" % n
    new = (OLD + NL +
           '$(warning P3DBG CNSS_QCA6390=$(CONFIG_CNSS_QCA6390) LITHIUM=$(CONFIG_LITHIUM) 11AX=$(CONFIG_WLAN_FEATURE_11AX))')
    open(P, "w").write(src.replace(OLD, new, 1))
    print("p218: gate-visibility warning added")
print("p218: OK")
