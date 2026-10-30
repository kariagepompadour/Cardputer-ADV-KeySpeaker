/*
  UsbNames.h — KeySpeaker (USB Speaker & BLE Keyboard): the names the computer shows for the USB functions (S2-5)

  The core's USB classes register fixed interface names ("TinyUSB UAC1", "TinyUSB HID", "TinyUSB CDC"), and macOS
  lists the speaker under its audio interface's name. The linker option -Wl,--wrap=tinyusb_add_string_descriptor
  (set in firmware/build.sh) sends those registrations here first, so they can be given KeySpeaker's names. Any other
  name passes through unchanged. Without the linker option, nothing calls this and the core's names stay.
*/
#pragma once
#include <string.h>
#include <stdint.h>

extern "C" uint8_t __real_tinyusb_add_string_descriptor(const char* str);

extern "C" uint8_t __wrap_tinyusb_add_string_descriptor(const char* str) {
  static const char* const NAMES[][2] = {
      {"TinyUSB UAC1", "KeySpeaker"},
      {"TinyUSB HID", "KeySpeaker Keyboard"},
      {"TinyUSB CDC", "KeySpeaker Serial"},
  };
  for (const auto& n : NAMES)
    if (str && strcmp(str, n[0]) == 0) return __real_tinyusb_add_string_descriptor(n[1]);
  return __real_tinyusb_add_string_descriptor(str);
}
