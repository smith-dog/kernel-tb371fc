#!/system/bin/sh
dmesg | grep -iE 'camss|cam_|camera|sensor|eeprom|actuator|ois|csiphy|csid' | grep -v camx | head -40
echo "=== dev nodes ==="
ls /dev/ | grep -iE 'cam|media|v4l' 
ls /dev/media* 2>/dev/null | head
