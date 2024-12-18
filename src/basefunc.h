#ifndef _BASE_FUNC_H
#define _BASE_FUNC_H

#include <arduino.h>
    
#ifndef DebugBegin
  #if defined(_APP_DEBUG)
    #if ARDUINO_USB_CDC_ON_BOOT && defined(USE_DEVELOP_KIT)
      #define DebugBegin(baud_rate)    Serial0.begin(baud_rate)
      #define DebugPrintln(message)    Serial0.println(message)
      #define DebugPrint(message)      Serial0.print(message)
      #define DebugPrintf(format, ...) Serial0.printf(format, ##__VA_ARGS__)
    #else  
      #define DebugBegin(baud_rate)    Serial.begin(baud_rate)
      #define DebugPrintln(message)    Serial.println(message)
      #define DebugPrint(message)      Serial.print(message)
      #define DebugPrintf(format, ...) Serial.printf(format, ##__VA_ARGS__)
    #endif
  #else
    #define DebugBegin(baud_rate)
    #define DebugPrintln(message)
    #define DebugPrint(message)
    #define DebugPrintf(format, ...)
  #endif
#endif

#define DebugPrintVersion() 	DebugPrintln("\nbuild time: " + String(__DATE__) + ", " + String(__TIME__))

class CTimerMs {
  public:
    CTimerMs(unsigned long interval = 100) {
        _msInterval = interval;
    }

    void startUp(unsigned long interval = 0) {
        if (interval != 0)
            _msInterval = interval;

        this->update();
    }
    void suspend() {
        _msSuspend = millis();
    }
    void resume() {
        if (_msSuspend == 0)
            return;

        _msUpdate += (millis() - _msSuspend);
        _msSuspend = 0;
    }
    void reset() {
        _msUpdate = 0, _msSuspend = 0, _countUpdate = 0;
    }
    void update() {
        _msUpdate = millis();
        _countUpdate++;
        _msSuspend = 0;
    }
    bool isWorking() const {
		return _msUpdate != 0 && _countUpdate != 0;
    }
    bool isTimeOut() const {
        if (_msSuspend != 0)
            return false;

        return millis() >= _msUpdate + _msInterval;
    }
    bool isActive() const {
        return _msUpdate != 0 && !this->isTimeOut();
    }
    bool toNextTime() {
        return this->isTimeOut() ? this->update(), true : false;
    }
    unsigned long getUpdateCount() const {
        return _countUpdate;
    }
  private:
    unsigned long _msInterval   = 0;
    unsigned long _msUpdate     = 0;
    unsigned long _msSuspend    = 0;
    unsigned long _countUpdate  = 0;
};


#endif
