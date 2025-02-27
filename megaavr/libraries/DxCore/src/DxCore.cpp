#include "Arduino.h"
#include "DxCore.h"

// *INDENT-OFF*
void configXOSC32K(X32K_TYPE_t settings, X32K_ENABLE_t enable) {
  uint8_t newval = settings | enable;
  // if any of the bits are "enable protected" we need to turn off the external crystal/clock.
  if ((CLKCTRL.XOSC32KCTRLA ^ newval) & (CLKCTRL_SEL_bm | CLKCTRL_CSUT_gm)) {
    _PROTECTED_WRITE(CLKCTRL.XOSC32KCTRLA, CLKCTRL.XOSC32KCTRLA & 0xFE); // disable external crystal
    while (CLKCTRL.MCLKSTATUS & CLKCTRL_XOSC32KS_bm); // unclear if this is immediately cleared or not...
  }
  _PROTECTED_WRITE(CLKCTRL.XOSC32KCTRLA, newval);
}


uint8_t enableAutoTune() {
  if ((CLKCTRL.MCLKCTRLA & 0x0F) != 0) {
    return 0xFF;
  }
  if (CLKCTRL.XOSC32KCTRLA & 0x01) {
    _PROTECTED_WRITE(CLKCTRL.XOSC32KCTRLA, X32K_HIGHPWR_START1S | X32K_ENABLED);
  }
  _PROTECTED_WRITE(CLKCTRL.OSCHFCTRLA, CLKCTRL.OSCHFCTRLA | 0x01);
  uint8_t csut = (CLKCTRL.XOSC32KCTRLA & CLKCTRL_CSUT_gm) >> 4;
  uint32_t verifytime = 500 + ((csut == 3) ? 2000 : (500 * csut));
  uint32_t startedAt = millis();
  while ((millis() - startedAt < verifytime) && (!(CLKCTRL.MCLKSTATUS & CLKCTRL_XOSC32KS_bm)));
  if (CLKCTRL.MCLKSTATUS & CLKCTRL_XOSC32KS_bm) {
    return 0;
  }
  _PROTECTED_WRITE(CLKCTRL.OSCHFCTRLA, CLKCTRL.OSCHFCTRLA & 0xFE);// turn it back off - crystal not working
  return 1;
}


// uint8_t disableAutoTune()
// Returns 255 (-1) if autotune was not enabled.
// Returns 0 if autotune is successfully disabled.
int8_t disableAutoTune() {
  if (CLKCTRL.OSCHFCTRLA & 0x01) {
    _PROTECTED_WRITE(CLKCTRL.OSCHFCTRLA, CLKCTRL.OSCHFCTRLA & 0xFE);
    return 0x00;
  } else {
    return 0xFF;
  }
}
#endif

bool setTCA0MuxByPort(uint8_t port) {
  if (port < 7) {
    TCA0.SPLIT.CTRLB = 0; // disconnect
    uint8_t base_pin = portToPinZero(port);
    uint8_t max_pin = min((base_pin + 6 > NUM_DIGITAL_PINS ? NUM_DIGITAL_PINS : base_pin + 6), portToPinZero(port + 1)) - 1;
    for (byte i = base_pin; i < (min(max_pin, base_pin + 6); i++)) {
      turnOffPWM(i);
    }
    PORTMUX.TCAROUTEA = (PORTMUX.TCAROUTEA & (~PORTMUX_TCA0_gm)) | (port & PORTMUX_TCA0_gm);
    return true;
  }
  return false;
}

bool setTCA0MuxByPin(uint8_t pin) {
  if (digitalPinToBitPosition(pin) < 6) {
    return setTCA0MuxByPort(digitalPinToPort(pin));
  }
  return false;
}

#ifdef TCA1
  bool setTCA1MuxByPort(uint8_t port, bool takeover_only_ports_ok = false) {
    #if defined(DB_64_PINS)
      if (!((port == 1) || (port == 6) || (((port == 2) || (port == 4)) && takeover_only_ports_ok))) {
        return false;
      }
      // not one of the 4 working mapping options that we have on DB64. TCA1 mapping
      // for this port doesn't exist, port is invalid, or not a port if not caught by above #if
    #else
      // if not a DB, the PORTG mapping option doesn't work per errata...
      if (port != 1 && (!(port == 2  && takeover_only_ports_ok))) {
        return false;
      }
    #endif
    // AND with group mask cuts off the unwanted low bit leaving us with the 2 high bits which is what we care about
    TCA1.SPLIT.CTRLB = 0; // disconnect all PWM channels
    uint8_t base_pin = portToPinZero(port);
    uint8_t max_pin = min((base_pin + 6 > NUM_DIGITAL_PINS ? NUM_DIGITAL_PINS : base_pin + 6), portToPinZero(port + 1)) - 1;
    for (byte i = base_pin; i < max_pin; i++) {
      turnOffPWM(i);
    }
    PORTMUX.TCAROUTEA = (PORTMUX.TCAROUTEA & (~PORTMUX_TCA1_gm)) | ((port << 2) & PORTMUX_TCA1_gm);
    return true;
  }

  bool setTCA1MuxByPin(uint8_t pin, bool takeover_only_ports_ok = false) {
    uint8_t port = digitalPinToPort(pin);
    uint8_t bit_mask = digitalPinToBitMask(pin);
    #if defined(DB_64_PINS)
      // AVR128DB, AVR64DB work with the high MUX options
      if (((port == 1   || port == 6) && (bit_mask & 0x3F)) || (bit_mask & 0x70)) {
    #else  // AVR128DA64 definitely do not work. AVR64DA64 untested.
      if (((port == 1 /*  errata  */) && (bit_mask & 0x3F)) || (bit_mask & 0x70)) {
    #endif // And those are the only 4 parts in the product line for which those pins exist. *INDENT-ON*
      // PORTB and PORTG have full-service TCA1 (well, not PG on the 128DA63 up to at least the A8 die
      // rev). for those, bit_mask will be 0x01, 0x02, 0x04, 0x08, 0x10, or 0x20 - but not 0x40 or
      // 0x80. Hence test against 0x3F works. For the others, it is either 0x10, 0x20, or 0x40 so
      // test against 0x70; the port function will check that the non-B/G port is valid.
      return setTCA1MuxByPort(digitalPinToPort(pin), takeover_only_ports_ok);
    }
    return false;
  }
#endif // TCA1

bool setTCD0MuxByPort(uint8_t port, bool takeover_only_ports_ok = false) {
  /* Errata :-(
    if ((!(port < 2 || port > 4)) || (port >6))
      return false;
    if (port > 4)
      port -= 3; // 0, 1 left as is, 2, 3, 4 got return'ed out of. 5, 6 get turned into 2 and 3.
    PORTMUX.TCDROUTEA = port;
    return true;
  */
  if (port == 0 || takeover_only_ports_ok) { // See errata; appears to be broken on all parts, not just 128k ones. Return false unless the one working port was requested.
    PORTMUX.TCDROUTEA = 0;
    return true;
  }
  return false;

}

bool setTCD0MuxByPin(uint8_t pin, bool takeover_only_ports_ok = false) {
  if (digitalPinToBitPosition(pin) & 0xF0) {
    return setTCD0MuxByPort(digitalPinToPort(pin), takeover_only_ports_ok); // See errata; appears to be broken on all parts, not just 128k ones. So, if it's not pin 4-7,
  }
  return false; // it's definitely no good. If it is 4-7, pass the other function to check port (though we could optimize further here, since
}               // chips that one might want to call this for don't exist, let's not bother :-)


#if defined(AZDUINO_DB_MULTIREG)
/* The Maxim regulator has 2 pins each of which can float, or be driven high or low to set the voltage, then you bounce enable to latch the new voltage.
   REG_OFF    0xFF
   REG_1V2    0b0100 0100
   REG_1V5    0b1000 1000
   REG_1V8    0b0100 0000
   REG_2V5    0b0000 0000
   REG_3V0    0b1100 0000
   REG_3V1    0b1100 0100
   REG_3V3    0b1000 0000
   REG_4V0    0b1100 1000
   REG_5V0    0b1100 1100
*/



int8_t setMVIOVoltageReg(uint8_t setting) {
  if (setting == REGOFF) {
    VPORTE.OUT &= ~(1<<6);
    return 0;
  } else if (setting & 0x33) {
      return -1; /* error - invalid setting */
  } else if ((setting - (setting << 4)) < 0) {
    return -1;
  } else{
      VPORTE.OUT     &= ~(1 << 6);     // cbi
      VPORTE.DIR     |=  (1 << 6);     // sbi
      if (setting     &  (1 << 7)) {   // andi breq
        VPORTG.DIR   |=  (1 << 7);     // sbi
        if (setting   &  (1 << 3)) {   // sbrc
          VPORTG.OUT |=  (1 << 7);     // sbi
        } else {                       // sbrs
          VPORTG.OUT &= ~(1 << 7);     // cbi
        }                              // rjmp
      } else {
        VPORTG.DIR   &= ~(1 << 7);     // cbi
      }
      if (setting     &  (1 << 6)) {  // sbrs rjmp
        VPORTG.DIR   |=  (1 << 6);    // sbi
        if (setting   &  (1 << 2)) {  // sbrc
          VPORTG.OUT |=  (1 << 6);    // sbi
        } else {                      // sbrs
          VPORTG.OUT &= ~(1 << 6);    // cbi
        }                             // rjmp
      } else {
        VPORTG.DIR   &= ~(1 << 6);    // cbi
      }
      VPORTE.OUT     |=  (1 << 6);    // sbi
      return 1;                       // ldi ret
    }
  }
}
#endif

int16_t getMVIOVoltage() {
  if (getMVIOStatus() == MVIO_OKAY) {
    uint8_t tempRef = VREF.ADC0REF; // save reference
    analogReference(INTERNAL1V024);  // known reference
    analogRead(ADC_VDDIO2DIV10); // exercise the ADC with this reference
    int32_t tempval = analogReadEnh(ADC_VDDIO2DIV10,13); // 0-8191,  8191 = 10240mv (obv not possible im practice);  So we want a multipication by 1.25 if not an error.
    VREF.ADC0REF = tempRef; // restore reference
    if (tempval < 0) {
      tempval += 2099990000; // make error numbers from enhanced reads fit in int16.
      // errors will be numbered -10000, -10001, etc
      return tempval;
    } else {
      // temp val should thus be 0-4095 unless analogReadEnh is broken.
      // multiply by by 1.25 the fast way.
      uint16_t retval=tempval; // truncate leading 0's.
      return retval + (retval >> 2);
    }
  } else {
    // sadly, if it's undervoltage, you can't get read of VDDIO2 this way!
    return getMVIOStatus();
  }
}
// *INDENT-ON*
