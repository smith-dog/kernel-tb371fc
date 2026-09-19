#!/bin/bash
F=$(grep -l 'spinel_tianma_nt36532_dsc_3k_video' /tmp/dtbo_*.dts | head -1)
echo "FILE=$F"
grep -n 'spinel_tianma_nt36532_dsc_3k_video\|mdss-dsi-on-command\|mdss-dsi-off-command\|on-command-state\|off-command-state\|panel-status-command\|panel-status-read-length\|status-command-state\|mdss-dsi-reset-sequence\|panel-init-command\|display-on-command\|qcom,mdss-dsi-post-init-command' "$F" | head -40
