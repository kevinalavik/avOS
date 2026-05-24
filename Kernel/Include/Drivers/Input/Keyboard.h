#ifndef DEVICE_KEYBOARD_H
#define DEVICE_KEYBOARD_H

#include <Drivers/Device.h>

#define KbdCtrlEchoOn  1
#define KbdCtrlEchoOff 2

extern Driver KbdDriver;
extern Device KbdDevice;

#endif
