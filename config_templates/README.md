# Premade config files

This directory contains a list of config files that users of evdevhook may find useful. Each of them aims to support a
reasonable subset of devices (usually determined by driver used) that evdevhook is compatible with.

**Contributions to this list are very welcome!**

* `nintendo.json` - support for JoyCons and ProCon.
Currently requires https://github.com/nicman23/dkms-hid-nintendo (until that driver gets into mainline kernel, which may happen soon).
* `playstation.json` - support for DualShock and DualSense controllers (DualSense is not in mainline kernel yet). Thanks to forsh33 and qurious-pixel for help with testing those.
