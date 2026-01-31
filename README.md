# libudev-bsd
A libudev compatible interface for BSD systems, based on the [libudev-devd](https://github.com/wulf7/libudev-devd) project for FreeBSD.
Includes changes from [libudev-openbsd](https://github.com/rnagy/libudev-openbsd/tree/dev) to support OpenBSD, and incorporates [patches from DPorts](https://github.com/DragonFlyBSD/DPorts/tree/master/devel/libudev-devd) for DragonFly support.
NetBSD support is provided via [drvctl(4)](https://man.netbsd.org/drvctl.4), using [ndevd](https://github.com/kikadf/ndevd).

### Device support
| | FreeBSD | DragonFly | OpenBSD | NetBSD |
| ---: | :---: | :---: | :---: | :---: |
| **input** | 
| evdev/input | /dev/input/event[0-9]* | /dev/input/event[0-9]* | - | - |
| USB keyboard | /dev/ukbd[0-9]* | ? */dev/kbd[0-9]\** | - | - |
| AT keyboard | /dev/atkbd[0-9]* | ? */dev/kbd[0-9]\** | - | - |
| keyboard multiplexer | /dev/kbdmux[0-9]* | ? */dev/kbd[0-9]\** | - | - |
| USB mouse | /dev/ums[0-9]* | /dev/ums[0-9]* | - | - |
| PS/2 mouse | /dev/psm[0-9]* | /dev/psm[0-9]* | - | /dev/psm[0-9]* |
| joystick | /dev/joy[0-9]* | - | - | /dev/joy[0-9]* |
| Apple touchpad | /dev/atp[0-9]* | - | - | - |
| Wellspring touchpad | /dev/wsp[0-9]* | /dev/wsp[0-9]* | - | - |
| eGalax touchscreen | /dev/uep[0-9]* | /dev/uep[0-9]* | - | - |
| virtualized mouse | /dev/sysmouse | /dev/sysmouse | - | - |
| vboxguest | /dev/vboxguest | - | - | - |
| generic keyboard | - | - | /dev/wskbd[0-9]* | /dev/wskbd[0-9]* |
| generic mouse | - | - | /dev/wsmouse[0-9]* | /dev/wsmouse[0-9]* |
| **drm** | /dev/dri/card[0-9]* | /dev/dri/card[0-9]* | /dev/dri/card[0-9]* | /dev/dri/card[0-9]* |
| | /dev/drm/[0-9]* | - | - | - |
| **net** | if* | ? if* | ? | if* |
| **hidraw** | /dev/hidraw[0-9]* | - | - | - |
| **pci** | devinfo.h | devinfo.h | - | - |
| **fido** | - | - | /dev/fido/[0-9]* | /dev/uhid[0-9]* |
