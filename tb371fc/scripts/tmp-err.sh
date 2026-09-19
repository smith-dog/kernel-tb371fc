#!/system/bin/sh
dmesg | grep -icE 'dsi.*error|underflow|command transfer failed|dma_tx done'
dmesg | grep -c 'P108'
