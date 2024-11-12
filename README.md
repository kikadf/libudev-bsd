# libbudev
libudev compatible interface for BSDs based on the [libudev-devd](https://github.com/wulf7/libudev-devd) FreeBSD project.
Apply the [libudev-openbsd](https://github.com/rnagy/libudev-openbsd/tree/dev) changes to support OpenBSD, [patches from DPorts](https://github.com/DragonFlyBSD/DPorts/tree/master/devel/libudev-devd) to support DragonFly.
NetBSD support over the [drvctl(4)](https://man.netbsd.org/drvctl.4).

### Device support
| | FreeBSD | DragonFly | OpenBSD | NetBSD |
| ---: | :---: | :---: | :---: | :---: |
| **input** | 
| evdev/input | /dev/input/evenet[0-9]* | ? | ? | ? |
| USB keyboard | /dev/ukbd[0-9]* | ? | - | - |
| AT keyboard | /dev/atkbd[0-9]* | ? | - | - |
| keyboard multiplexer | /dev/kbdmux[0-9]* | ? | - | - |
| USB mouse | /dev/ums[0-9]* | ? | - | - |
| PS/2 mouse | /dev/psm[0-9]* | ? | - | - |
| joystick | /dev/joy[0-9]* | ? | - | - |
| Apple touchpad | /dev/atp[0-9]* | ? | - | - |
| Wellspring touchpad | /dev/wsp[0-9]* | ? | - | - |
| eGalax touchscreen | /dev/uep[0-9]* | ? | - | - |
| virtualized mouse | /dev/sysmouse | ? | - | - |
| vboxguest | /dev/vboxguest | ? | - | - |
| generic keyboard | - | - | - (*/dev/wskbd\**) | - (*/dev/wskbd\**) |
| generic mouse | - | - | - (*/dev/wsmouse\**) | - (*/dev/wsmouse\**) |
| **drm** | /dev/dri/card[0-9]* | /dev/dri/card[0-9]* | ? | ? |
| | /dev/drm/[0-9]* | - | ? | ? |
| **net** | if* | ? | ? | ? |
| **hidraw** | /dev/hidraw[0-9]* | ? | ? | ? |
| **pci** | devinfo.h | ? | ? | ? |
| **fido** | - | - | /dev/fido/[0-9]* | ? |
