#!/system/bin/sh
dmesg | grep -iE 'dsi.*error|underflow|command transfer failed|dma_tx done'
