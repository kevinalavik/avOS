#ifndef DEVICE_KEYBOARD_H
#define DEVICE_KEYBOARD_H

#include <Drivers/Device.h>
#include <stdint.h>

#define KbdCtrlEchoOn 1
#define KbdCtrlEchoOff 2
#define KbdCtrlGetCount 3
#define KbdCtrlIsKeyDownBase 0x10000u

#define KbdKeyNone 0x0000u
#define KbdKeyEscape 0x001Bu
#define KbdKeyBackspace 0x0008u
#define KbdKeyTab 0x0009u
#define KbdKeyEnter 0x000Au
#define KbdKeySpace 0x0020u
#define KbdKeyLeftShift 0x0100u
#define KbdKeyRightShift 0x0101u
#define KbdKeyLeftCtrl 0x0102u
#define KbdKeyRightCtrl 0x0103u
#define KbdKeyLeftAlt 0x0104u
#define KbdKeyRightAlt 0x0105u
#define KbdKeyUp 0x0110u
#define KbdKeyDown 0x0111u
#define KbdKeyLeft 0x0112u
#define KbdKeyRight 0x0113u

extern Driver KbdDriver;
extern Device KbdDevice;

#endif
