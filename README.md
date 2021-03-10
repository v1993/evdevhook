# evdevhook - DSU server for motion from evdev compatible joysticks

This project uses evdev input system Linux provides to support motion input by devices whose drivers have exposed such functionality.

Its main goal is to reduce amount of motion providers, giving a unified solution for all devices whose drivers expose motion in standardized way.

## Support status for devices

Below is the list of devices with their support status.

Please note that ability to support device does not really depend on us and it's up to driver to provide correct motion interface.

### Supported out-of-box (example config provided)

* `hid-nintendo` - Nintendo Switch's JoyCons and ProCon (Note: not in mainline kernel yet, needs [dkms module](https://github.com/nicman23/dkms-hid-nintendo) as of now)
* `hid-sony` - DualShock 3 and 4 controllers (DS3 lacks gyro)
* `hid-playstation` - DualSense controller (Note: not in mainline kernel yet as of now)

### Supported, but not tested (no example config, help very welcome)

* `hid-udraw-ps3`, `wacom_wac` - THQ PS3 uDraw, Wacom 27QHD tablets (nobody was high enough to try and control games with motion from those devices yet, they also lack gyros)

### Not supported (notably)

* `hid-wiimote` - WiiMote's driver does not expose correctly marked device that can be used directly (check out [linuxmotehook](https://github.com/v1993/linuxmotehook))

# Dependences

This project depends on few common software packages. The following command can install build-time requirements on Ubuntu and derived distributives:

```bash
sudo apt install libevdev-dev libudev-dev libglibmm-2.4-dev nlohmann-json3-dev zlib1g-dev
```

You'll also need CMake and a C++ compiler (like gcc), but you probably already have them.

# Usage

Basic usage is as follows:

```bash
evdevhook [config file]
```
Check out `config_templates` for useful configs and information on how to create your own if needed. Run without arguments to see what motion devices are connected to your system.
