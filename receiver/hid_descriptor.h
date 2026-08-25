#ifndef HID_DESCRIPTOR_H
#define HID_DESCRIPTOR_H

#include "USB.h"
#include "USBHID.h"

// 4-bajtowa struktura nagłówka (5. bajt odbiornik czyta osobno z paczki)
typedef struct __attribute__((packed)) {
  uint8_t deviceType; // 0 = Paddle, 1 = Spinner
  int16_t value;      // 0-4095
  uint8_t buttons;    // Bitmaska przycisków
} struct_message;

// Deskryptor Native Daemonbite (Czysty, pojedynczy raport bez REPORT_ID)
static const uint8_t hidReportDescriptor[] = {
  0x05, 0x01,                       // USAGE_PAGE (Generic Desktop)
  0x09, 0x04,                       // USAGE (Joystick)
  0xa1, 0x01,                       // COLLECTION (Application)
    0xa1, 0x00,                     // COLLECTION (Physical)
    
      0x05, 0x09,                   // USAGE_PAGE (Button)
      0x19, 0x01,                   // USAGE_MINIMUM (Button 1)
      0x29, 0x04,                   // USAGE_MAXIMUM (Button 4)
      0x15, 0x00,                   // LOGICAL_MINIMUM (0)
      0x25, 0x01,                   // LOGICAL_MAXIMUM (1)
      0x95, 0x08,                   // REPORT_COUNT (8)
      0x75, 0x01,                   // REPORT_SIZE (1)
      0x81, 0x02,                   // INPUT (Data,Var,Abs)
    
      0x05, 0x01,                   // USAGE_PAGE (Generic Desktop)

      0x09, 0x37,                   // USAGE (Dial)
      0x15, 0x80,                   // LOGICAL_MINIMUM (-128)
      0x25, 0x7F,                   // LOGICAL_MAXIMUM (127)
      0x95, 0x01,                   // REPORT_COUNT (1)
      0x75, 0x08,                   // REPORT_SIZE (8)
      0x81, 0x06,                   // INPUT (Data,Var,Rel)

      0x09, 0x38,                   // USAGE (Wheel)
      0x15, 0x00,                   // LOGICAL_MINIMUM (0)
      0x26, 0xFF, 0x00,             // LOGICAL_MAXIMUM (255)
      0x95, 0x01,                   // REPORT_COUNT (1)
      0x75, 0x08,                   // REPORT_SIZE (8)
      0x81, 0x02,                   // INPUT (Data,Var,Abs)

    0xc0,                           // END_COLLECTION
  0xc0                              // END_COLLECTION 
};

struct GamepadReport {
  uint8_t buttons; 
  int8_t  spinner;
  uint8_t paddle; 
} __attribute__((packed));

extern volatile uint32_t usbReportCounterWindow;
extern USBHID HID;

class DaemonbiteHIDDevice : public USBHIDDevice {
public:
  DaemonbiteHIDDevice() {
    static bool initialized = false;
    if (!initialized) {
      HID.addDevice(this, sizeof(hidReportDescriptor));
      initialized = true;
    }
  }

  uint16_t _onGetDescriptor(uint8_t* buffer) override {
    memcpy(buffer, hidReportDescriptor, sizeof(hidReportDescriptor));
    return sizeof(hidReportDescriptor);
  }

  void sendReport(uint8_t buttons, int8_t spinner, uint8_t paddle) {
    GamepadReport report;
    report.buttons = buttons;
    report.spinner = spinner;
    report.paddle = paddle;
    if (HID.SendReport(0, &report, sizeof(report))) {
      usbReportCounterWindow++;
    }
  }
};

#endif