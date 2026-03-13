#include "wiring.h"
#ifndef AnalogMuxScanner_h
#define AnalogMuxScanner_h
#include "core_pins.h"

typedef void (*Change)(int channel, int pot, uint16_t value);

class AnalogMuxScanner {
private:
  uint8_t _s0, _s1, _s2, _s3, _ena;
  uint8_t _a[3];
  uint8_t _channels;

  Change  _onChange = nullptr;

  uint8_t  _mux = 0;
  uint32_t _lastScanMs = 0;
  uint16_t _scanIntervalMs = 5;
  uint16_t _hysteresis     = 3;
  uint8_t  _samplesPerRead = 1;

  uint16_t* _potHysteresis;
  uint16_t* _stable;
  bool*     _hasStable;

  uint16_t readInvertedAnalog(uint8_t pin) {
    if (_samplesPerRead <= 1) {
      return (uint16_t)(1023 - analogRead(pin));
    }
    uint32_t sum = 0;
    for (uint8_t i = 0; i < _samplesPerRead; ++i) {
      sum += analogRead(pin);
      delayMicroseconds(5);
    }
    uint16_t avg = (uint16_t)(sum / _samplesPerRead);
    return (uint16_t)(1023 - avg);
  }

public:
  AnalogMuxScanner(uint8_t s0, uint8_t s1, uint8_t s2, uint8_t s3, uint8_t ena,
                   uint8_t a0, uint8_t a1, uint8_t a2,
                   uint8_t channels)
  : _s0(s0), _s1(s1), _s2(s2), _s3(s3), _ena(ena),
    _a{a0, a1, a2},
    _channels(channels)
  {
    _stable = new uint16_t[_channels * 3];
    _hasStable = new bool[_channels * 3];
    _potHysteresis = new uint16_t[_channels * 3];
    for (uint16_t i = 0; i < _channels * 3; ++i) {
      _stable[i] = 0;
      _hasStable[i] = false;
      _potHysteresis[i] = _hysteresis;
    }
  }

  ~AnalogMuxScanner() {
    delete[] _stable;
    delete[] _hasStable;
    delete[] _potHysteresis;
  }

  void begin() {
    pinMode(_s0, OUTPUT);
    pinMode(_s1, OUTPUT);
    pinMode(_s2, OUTPUT);
    pinMode(_s3, OUTPUT);
    pinMode(_ena, OUTPUT);

    digitalWrite(_ena, LOW); // Enable the multiplexer
    analogReadResolution(10);
  }

  void setScanInterval(uint16_t ms) { _scanIntervalMs = ms; }
  void setHysteresis(uint16_t hysteresis) {
    for(int i=0; i<(_channels*3); i++) {
      _potHysteresis[i] = hysteresis;
    }
  }
  void setPotHysteresis(int channel, int pot, uint16_t hysteresis) {
    if(channel < 0 || channel >= _channels) return;
    if(pot < 0 || pot >= 3) return;
    _potHysteresis[pot * PARTS + channel] = hysteresis;
  }
  void setSamplesPerRead(uint8_t n) { _samplesPerRead = (n == 0) ? 1 : n; }

  void onChange(Change onChangeHandler) {
    _onChange = onChangeHandler;
  }

  void scan(unsigned long now) {
    if (now - _lastScanMs < _scanIntervalMs) return;
    _lastScanMs = now;

    digitalWrite(_s0, (_mux & 0x01));
    digitalWrite(_s1, (_mux & 0x02) >> 1);
    digitalWrite(_s2, (_mux & 0x04) >> 2);
    //digitalWrite(_s3, (_mux & 0x08) >> 3);
    digitalWrite(_s3, LOW); // until the upper 8 inputs are connected

    delayMicroseconds(20);

    uint16_t values[3] = {
      readInvertedAnalog(_a[0]),
      readInvertedAnalog(_a[1]),
      readInvertedAnalog(_a[2])
    };

    // char s[100];
    // sprintf(s, "mux: %d  v1: %d  v2: %d  v3: %d", _mux, values[0], values[1], values[2]);
    // Serial.println(s);    

    for (int i = 0; i < 3; ++i) {
      //int off = 2 * i;                       
      //int ch  = (int)(_mux + off) / 3 + off; 
      int ch = _mux;
      //int pot = (int)(_mux + off) % 3;       
      int pot = i;

      if (ch < 0 || ch >= _channels) continue;

      uint16_t raw = values[i];
      uint16_t idx = i * PARTS + ch;

      if(pot==1) continue;

      if (!_hasStable[idx]) {
        _stable[idx] = raw;
        _hasStable[idx] = true;
        if (_onChange) _onChange(ch, pot, _stable[idx]); // until more muxes
      } else {
        int diff = (int)raw - (int)_stable[idx];
        if (diff < 0) diff = -diff;
        if (diff >= _potHysteresis[idx]) {
          // char s[100];
          // sprintf(s, "idx: %d  ch: %d  pot: %d  stable: %d  raw: %d  diff: %d  hyst: %d", idx, ch, pot, _stable[idx], raw, diff, _potHysteresis[idx]);
          // Serial.println(s);

          _stable[idx] = raw;
          if (_onChange) _onChange(ch, pot, _stable[idx]);  // until more muxes
        }
      }
    }

    //_mux = (_mux + 1) & 0x0F;
    _mux = (_mux + 1) & 0x07; // Only cycle through 0..7
  }
};

#endif