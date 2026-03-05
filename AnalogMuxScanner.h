#ifndef AnalogMuxScanner_h
#define AnalogMuxScanner_h


typedef void (*Change)(int channel, int pot, uint16_t value);

class AnalogMuxScanner {
private:
  uint8_t _s0, _s1, _s2;
  uint8_t _a[3];
  uint8_t _channels;

  Change  _onChange = nullptr;

  uint8_t  _mux = 0;
  uint32_t _lastScanMs = 0;
  uint16_t _scanIntervalMs = 5;
  uint16_t _hysteresis     = 3;
  uint8_t  _samplesPerRead = 1;

  uint16_t* _stable;
  bool*     _hasStable;

  uint16_t readInvertedAnalog(uint8_t pin) {
    if (_samplesPerRead <= 1) {
      return (uint16_t)(1023 - analogRead(pin));
    }
    uint32_t sum = 0;
    for (uint8_t i = 0; i < _samplesPerRead; ++i) sum += analogRead(pin);
    uint16_t avg = (uint16_t)(sum / _samplesPerRead);
    return (uint16_t)(1023 - avg);
  }


public:


  AnalogMuxScanner(uint8_t s0, uint8_t s1, uint8_t s2,
                   uint8_t a0, uint8_t a1, uint8_t a2,
                   uint8_t channels)
  : _s0(s0), _s1(s1), _s2(s2),
    _a{a0, a1, a2},
    _channels(channels)
  {
    // allocate per-channel/pot stable storage
    _stable = new uint16_t[_channels * 3];
    _hasStable = new bool[_channels * 3];
    for (uint16_t i = 0; i < _channels * 3; ++i) {
      _stable[i] = 0;
      _hasStable[i] = false;
    }
  }

  ~AnalogMuxScanner() {
    delete[] _stable;
    delete[] _hasStable;
  }

  void begin() {
    pinMode(_s0, OUTPUT);
    pinMode(_s1, OUTPUT);
    pinMode(_s2, OUTPUT);
  }

  void setScanInterval(uint16_t ms) { _scanIntervalMs = ms; }
  void setHysteresis(uint16_t counts) { _hysteresis = counts; }    // e.g., 2–5 counts
  void setSamplesPerRead(uint8_t n) { _samplesPerRead = (n == 0) ? 1 : n; }

  void onChange(Change onChangeHandler) {
    _onChange = onChangeHandler;
  }

  // Call regularly (e.g., in loop) when you want scanning active
  void scan(unsigned long now) {
    if (now - _lastScanMs < _scanIntervalMs) return;
    _lastScanMs = now;

    // Select mux address
    digitalWrite(_s0, (_mux & 0x01));
    digitalWrite(_s1, (_mux & 0x02) >> 1);
    digitalWrite(_s2, (_mux & 0x04) >> 2);

    // Read inverted values (same as map(..., 1023,0,0,1023), but faster)
    uint16_t values[3] = {
      readInvertedAnalog(_a[0]),
      readInvertedAnalog(_a[1]),
      readInvertedAnalog(_a[2])
    };

    // Distribute readings using the derived formula:
    // off = 2*i; ch = (mux + off)/3 + off; pot = (mux + off)%3
    for (int i = 0; i < 3; ++i) {
      int off = 2 * i;                       
      int ch  = (int)(_mux + off) / 3 + off; 
      int pot = (int)(_mux + off) % 3;       

      if (ch < 0 || ch >= _channels) continue;

      uint16_t raw = values[i];
      uint16_t idx = ch * 3 + pot;

      if (!_hasStable[idx]) {
        _stable[idx] = raw;
        _hasStable[idx] = true;
        if (_onChange) _onChange(ch, pot, _stable[idx]); // first report
      } else {
        int diff = (int)raw - (int)_stable[idx];
        if (diff < 0) diff = -diff;
        if (diff >= _hysteresis) {
          _stable[idx] = raw;
          if (_onChange) _onChange(ch, pot, _stable[idx]); // report significant change
        }
      }
    }

    // Advance mux (0..7)
    _mux = (_mux + 1) & 0x07;
  }
};

#endif