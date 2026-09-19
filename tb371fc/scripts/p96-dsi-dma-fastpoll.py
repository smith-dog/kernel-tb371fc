#!/usr/bin/env python3
"""p96 — fast-poll DSI cmd-DMA done instead of 200ms completion timeout.

TB371FC self-built kernel: the dsi_ctrl DMA-done interrupt stops reaching the
GIC after boot (only 2 deliveries ever; parent msm_drm irq is healthy). Every
DCS command therefore burns the full DSI_CTRL_TX_TO_MS (200ms) in
dsi_ctrl_dma_cmd_wait_for_done before the polling fallback clears the status.
Panel wake sequences (sleep-out/display-on) get stretched ~10x and race with
the composer -> intermittent "backlight on, black screen" after screen-off.

Fix: poll INT_STATUS at 1ms granularity (up to 20ms) inside the same work.
If the IRQ path works, wait_for_completion_timeout(1ms) still lets the ISR
complete fast; if it is dead, the transfer is confirmed done in ~1-2ms.
Demote the per-command WARN to DBG (kills the dmesg storm too).
"""
P = "/home/smith/android_kernel_lenovo_paladin/techpack/display/msm/dsi/dsi_ctrl.c"
NL = chr(10)
T = chr(9)

OLD = (
    T + "if (atomic_read(&dsi_ctrl->dma_irq_trig))" + NL +
    T + T + "goto done;" + NL +
    NL +
    T + "ret = wait_for_completion_timeout(" + NL +
    T + T + T + "&dsi_ctrl->irq_info.cmd_dma_done," + NL +
    T + T + T + "msecs_to_jiffies(DSI_CTRL_TX_TO_MS));" + NL +
    T + "if (ret == 0 && !atomic_read(&dsi_ctrl->dma_irq_trig)) {" + NL +
    T + T + "status = dsi_hw_ops.get_interrupt_status(&dsi_ctrl->hw);" + NL +
    T + T + "if (status & mask) {" + NL +
    T + T + T + "status |= (DSI_CMD_MODE_DMA_DONE | DSI_BTA_DONE);" + NL +
    T + T + T + "dsi_hw_ops.clear_interrupt_status(&dsi_ctrl->hw," + NL +
    T + T + T + T + T + "status);" + NL +
    T + T + T + "DSI_CTRL_WARN(dsi_ctrl," + NL +
    T + T + T + T + T + "\"dma_tx done but irq not triggered\\n\");" + NL +
    T + T + "} else {" + NL +
    T + T + T + "DSI_CTRL_ERR(dsi_ctrl," + NL +
    T + T + T + T + T + "\"Command transfer failed\\n\");" + NL +
    T + T + "}" + NL +
    T + T + "dsi_ctrl_disable_status_interrupt(dsi_ctrl," + NL +
    T + T + T + T + T + "DSI_SINT_CMD_MODE_DMA_DONE);" + NL +
    T + "}"
)

msecs = "msecs_to_jiffies(1)"

NEW = (
    T + "if (atomic_read(&dsi_ctrl->dma_irq_trig))" + NL +
    T + T + "goto done;" + NL +
    NL +
    T + "/* TB371FC p96: dsi_ctrl DMA-done IRQ is not delivered after the" + NL +
    T + " * first screen cycle on this self-built kernel (verified via" + NL +
    T + " * /proc/interrupts: count frozen at 2 while parent msm_drm IRQ" + NL +
    T + " * is healthy). Poll the done status at 1ms granularity instead of" + NL +
    T + " * blocking the full 200ms TX timeout per DCS command, which used" + NL +
    T + " * to stretch panel wake sequences and race the composer into a" + NL +
    T + " * backlight-on/black-panel state. */" + NL +
    T + "{int i;" + NL +
    NL +
    T + T + "for (i = 0; i < 20; i++) {" + NL +
    T + T + T + "ret = wait_for_completion_timeout(" + NL +
    T + T + T + T + T + "&dsi_ctrl->irq_info.cmd_dma_done," + NL +
    T + T + T + T + T + msecs + ");" + NL +
    T + T + T + "if (ret > 0 || atomic_read(&dsi_ctrl->dma_irq_trig))" + NL +
    T + T + T + T + T + "goto done;" + NL +
    T + T + T + "status = dsi_hw_ops.get_interrupt_status(&dsi_ctrl->hw);" + NL +
    T + T + T + "if (status & mask) {" + NL +
    T + T + T + T + "status |= (DSI_CMD_MODE_DMA_DONE | DSI_BTA_DONE);" + NL +
    T + T + T + T + "dsi_hw_ops.clear_interrupt_status(&dsi_ctrl->hw," + NL +
    T + T + T + T + T + T + "status);" + NL +
    T + T + T + T + "DSI_CTRL_DBG(dsi_ctrl," + NL +
    T + T + T + T + T + T + "\"dma_tx done via poll\\n\");" + NL +
    T + T + T + T + "goto done;" + NL +
    T + T + T + "}" + NL +
    T + T + "}" + NL +
    T + T + "DSI_CTRL_ERR(dsi_ctrl, \"Command transfer failed\\n\");" + NL +
    T + T + "dsi_ctrl_disable_status_interrupt(dsi_ctrl," + NL +
    T + T + T + T + T + "DSI_SINT_CMD_MODE_DMA_DONE);" + NL +
    T + "}"
)

src = open(P).read()
cnt = src.count(OLD)
assert cnt == 1, f"anchor x{cnt}"
open(P, "w").write(src.replace(OLD, NEW))
print("p96 applied: fast-poll cmd-dma done (1ms x20) in dsi_ctrl.c")
