#!/system/bin/sh
dmesg | grep -E '^\[ *9[0-7]\.' | grep -vE 'BLFIX|audit|healthd|QCOM-BATT'
