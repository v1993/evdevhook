# Config file format

evdevhook uses JSON for its configuration files. This document aims to explain structure of those.

# Example config file

This example aims to show all available options in a example as short as possible.

```json
{
	"port": 26761,
	"profiles": {
		"Controller Guys Incorporated": {
			"accel": "y+z-x+",
			"gyro": "y+z-x+"
		},
        "Motion Corporation": {
        	"accel": "x+y-z-",
            "gyro": "z+x+y-",
            "gyroSensitivity": 1.08
        }
	},
	"devices": [
		{
			"name": "Controller From These Guys - Motion Input",
			"profile": "Controller Guys Incorporated"
		},
		{
			"name": "Even Better Controller From These Guys - Motion Input",
			"profile": "Controller Guys Incorporated"
		},
		{
			"name": "Motion Corporation Motion Recorder",
			"profile": "Motion Corporation"
		}
	]
}

```

# Profiles

The `profiles` map describes profiles used to correctly bind motion of device to DSU protocol. Because there is no standardized orientation (mapping of physical directions to axles), you have to figure it out for every motion device you own - or, more likely, every motion device driver you use - same driver is more than likely to stay consistent with its orientation.

## Mapping strings

Both accelerometer and gyroscope use the same specifier format. Position of letter in string determines which logical direction you're defining while character itself defines physical axis you're assigning. Both accel and gyro physical axles are simply called x, y, z, in order driver have defined them. Logical axles are a little trickier: for accelerometer they go in order `figure it out later`, while for gyro they go in order `pitch, yaw, roll` (figure out correct signs for those directions and how to describe them). Additionally, a sign (`+` or `-`) is placed after each axis, specifying it it needs to be inverted.

It is recommended to configure both accelerometer and gyroscope, but some devices (like DualShock 3) lack gyroscope, so it needs not to be configured.

### How the hell do I find out what the correct mapping is?!

Start with arbitrary one, then use some tool (dolphin emulator's input window works the best) to correct it until it works. You'd better start with acceleration and only after that go to calibrating gyroscope. Good luck!

## `gyroSensitivity` (optional)

It allows to set custom multiplier for your gyro input. It should not be needed (so default is 1.0), but some drivers may mess up a bit.

# Devices

`devices` arrays describes mapping of devices exposed via DSU protocol (no more than four) to your physical devices.

## `name`

Name of motion device. Run `evdevhook` without arguments to get list of present ones.

## `profile`

Profile to use. It's common for few devices to use the same profile.

# Top level entries

## `port` (optional)

Allows to specify custom port to use. Default value is `26760`, but you may want to use this option if you run few motion providers at once.
