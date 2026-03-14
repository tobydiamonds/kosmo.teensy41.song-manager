#ifndef SongManagerUI_h
#define SongManagerUI_h

#include "Common.h"
#include "Models.h"
#include "Channel.h"
#include "DebounceButton165.h"
#include "AnalogMuxScanner16.h"

// all led outputs via 74HC595
#define LED_CLK 2  // => 595/11
#define LED_LATCH 3 // => 595/12
#define LED_DATA 4// => 595/14

// all digital inputs vua 74HC165
#define BTN_LOAD 5 // => 165/1
#define BTN_CLK 6  // => 165/2
#define BTN_DATA 7 // <= 165/9
#define BTN_LATCH 8// => 165/15

// mux pins
#define MUX_CH_1 24
#define MUX_CH_2 25
#define MUX_CH_3 26
#define MUX_S0   28
#define MUX_S1   29
#define MUX_S2   30
#define MUX_S3   31
#define MUX_ENA  11

// operations board input bit mask
#define PROGRAM_BTN 0
#define LOAD_BTN 1
#define NEXT_SONG_BTN 2
#define PREV_SONG_BTN 3

// operations board led bit mask
#define CLOCK_IN_LED 1 // DIG0
#define PROGRAMMER_LED_R 2 // DIG1
#define PROGRAMMER_LED_G 3 // DIG2
#define PROGRAMMER_LED_B 4 // DIG3


#define UPDATE_INTERVAL 1
#define SCAN_INTERVAL 50  
#define POT_SCAN_INTERVAL 10
#define DIGITS 5
#define LED_SHORT_PULSE 300
#define LED_VERY_SHORT_PULSE 25


class SongManagerUI {

private:
  DebounceButton165 programBtn;
  DebounceButton165 loadBtn;
  DebounceButton165 nextSongBtn;
  DebounceButton165 prevSongBtn;
  Channel (&parts)[PARTS];
  AnalogMuxScanner analogPotBank1;

  unsigned long lastUpdate;
  unsigned long lastScan;
  unsigned long lastProgrammingLed = 0;
  unsigned long lastSongLoadingLed = 0;
  unsigned long lastSongLoading = 0;
  unsigned long lastSongBlink = 0;
  unsigned long lastClockInLed = 0;  
  unsigned long lastPotScan = 0;
  bool blinkSongNumber;
  bool programmingLed;
  bool songIsLoading;
  bool songLoadingLed;
  bool programming;
  bool clockInLed;

  int selectedSongNumber;
  int prevSongNumber;

  void (*songNumberSelectedCallback)(const int) = nullptr;
  void (*programmingStartedCallback)(const int) = nullptr;
  void (*programmingEndedCallback)(const int) = nullptr;
  void (*programmingCancelledCallback)(const int) = nullptr;
  void (*partProgrammingChangedCallback)(const int, Channel) = nullptr; // partIndex, channel
  void (*partButtonPressedCallback)(const int, Channel&, bool, bool) = nullptr; // partIndex, channel, programming, songloading


  static SongManagerUI* instance;

  uint8_t read165byte() {
    uint8_t value = 0;
    for (int i = 0; i < 8; i++) {
      digitalWrite(BTN_CLK, LOW);      // prepare falling edge
      if (digitalRead(BTN_DATA)) {
        value |= (1 << i);               // store bit in LSB first
      }
      digitalWrite(BTN_CLK, HIGH);     // shift register updates here
    }
    return value;
  }  

  void scanOperationsBoard(unsigned long now) {
    uint8_t incoming = read165byte();
    //printByteln(incoming);
    if(incoming == 0xFF) return;

    programBtn.update(incoming, now);
    loadBtn.update(incoming, now);
    if(!programming) {
      nextSongBtn.update(incoming, now);
      prevSongBtn.update(incoming, now);
    }

    // NEXT SONG INDEX
    if(!programming && nextSongBtn.wasPressed() && !prevSongBtn.isDown()) {
      if(selectedSongNumber < MAX_SONGS-2)
        selectedSongNumber++;
      else
        selectedSongNumber=1;
    }
    // PREV SONG INDEX
    if(!programming && prevSongBtn.wasPressed() && !nextSongBtn.isDown()) {
      if(selectedSongNumber > 1)
        selectedSongNumber--;
      else
        selectedSongNumber = MAX_SONGS;
    }
    // LOAD SONG
    if(!programming && loadBtn.wasPressed()) { 
      songIsLoading = true;
      songLoadingLed = true;
      lastSongLoading = now;
      prevSongNumber = selectedSongNumber;
      if(songNumberSelectedCallback) songNumberSelectedCallback(selectedSongNumber);
    }    
    // CANCEL LOAD SONG
    if(songIsLoading && loadBtn.wasPressed()) {
      songIsLoading = false;
      songLoadingLed = true;
      selectedSongNumber = prevSongNumber;
      if(songNumberSelectedCallback) songNumberSelectedCallback(selectedSongNumber);
    }
    // CANCEL PROGRAMMING
    if(programming && loadBtn.wasPressed()) { 
      programming = false;
      programmingLed = false;
      if(programmingCancelledCallback)programmingCancelledCallback(selectedSongNumber);
    }    
    if(!songIsLoading && programBtn.wasPressed()) { 
    // START PROGRAMMING
      if(!programming) {                      
        programming = true;
        if(programmingStartedCallback)programmingStartedCallback(selectedSongNumber);
      } else { 
    // END PROGRAMMING                          
        programming = false;
        if(programmingEndedCallback)programmingEndedCallback(selectedSongNumber);
      }
    } 
  }  

  void scanChannelBoards(unsigned long now) {
    uint8_t incoming = 0;
    for(int i=0; i<PARTS; i++) {
      if(i % 8 == 0) {// only read 1 165 pr 8 parts
        incoming = read165byte();
        //printByteln(incoming);
        if(incoming == 0xFF) return;
      }

      parts[i].Button()->update(incoming, now);
      if(parts[i].Button()->wasPressed()) {
        if(partButtonPressedCallback)
          partButtonPressedCallback(i, parts[i], programming, songIsLoading);
      }      
    }
  }

  void scanInputs(unsigned long now) {
    digitalWrite(BTN_LOAD, LOW);
    delayMicroseconds(5);
    digitalWrite(BTN_LOAD, HIGH);
    delayMicroseconds(5);
    digitalWrite(BTN_CLK, HIGH);
    digitalWrite(BTN_LATCH, LOW);

    // read data here
    scanOperationsBoard(now);
    scanChannelBoards(now);

    digitalWrite(BTN_LATCH, HIGH);
  }

  void write595byte(uint8_t data, uint8_t bitOrder = LSBFIRST) {
    shiftOut(LED_DATA, LED_CLK, bitOrder, data);
  }

  // 2-digit display
  const uint8_t digitEnable[2] = {
    static_cast<uint8_t>(~(B01000000)), // QG -> digit1 (MSD)
    static_cast<uint8_t>(~(B10000000))  // QH -> digit2
  };


  const byte digitToSegment[14] = {
    // GFEDCBAdp
    B01111110, // 0
    B00001100, // 1
    B10110110, // 2
    B10011110, // 3
    B11001100, // 4
    B11011010, // 5
    B11111010, // 6
    B00001110, // 7
    B11111110, // 8
    B11011110, // 9
    B00000000,  // reset display
    B11100110, // P
    B10100000, // r
    B11111010  // G
  };


  const byte digitToSegment28[12] = {
    // dpGFEDCBA
    B00111111, // 0
    B00110000, // 1
    B01011011, // 2
    B01111001, // 3
    B01110100, // 4
    B01101101, // 5
    B01101111, // 6
    B00111000, // 7
    B01111111, // 8
    B01111100, // 9
    B00000000,  // reset display
    B01000000, // dash
  };

  int getDigit(int number, int position) {
    // position = 0 => least significant digit
    for (int i = 0; i < position; i++) {
      number /= 10;
    }
    return number % 10;
  }


  void updateOperationsBoardDigit(int digit) {

    /*
    * 1st 595:          2nd 595:
    * QA => SEG G       QA => digit 1 enable (active low) 0x01
    * QB => SEG F       QB => digit 2 enable (Active low) 0x02
    * ...               QC => RGB R (active low)          0x08
    * QG => SEG A       QD => RGB G (active low)          0x10
    * QH => dp          QE => RGB B (active low)          0x20
    *                   QF => clock in led                0x04
    */


    uint8_t data[2] = {0};
    int index = (digit % 2 == 0) ? 1 : 0; // we have only 2 digits in the ops board, so whatever digit value comes in, we want to determine which of the 2 digits we are updating

    int digitVal = getDigit(selectedSongNumber, index);
    data[0] = (blinkSongNumber) ? 0x00 : digitToSegment[digitVal];
    data[1] = digitEnable[index];

    if(programmingLed)  // red led blinking while loading
      data[1] &= ~0x08;
    else
      data[1] |= 0x08;

    if(songIsLoading) {
      if(songLoadingLed)  // green led blinking while loading
        data[1] &= ~0x10;
      else
        data[1] |= 0x10;
    } else if(!programming) {
      data[1] &= ~0x10;  // green led static when song is loaded
    }

    if(clockInLed)
      data[1] &= ~0x04;
    else
      data[1] |= 0x04;
    

    write595byte(data[1]);
    write595byte(data[0]);

  }

  void updatePartDigit(int partIndex, int digit) {
    /*
    * 1st 595:                                        2nd 595:
    * QA => DIG_0 enable (LEDS)                       QA => SEG_A
    * QB => DIG_1 enable (no. of repeats left digit)  QB => SEG_B
    * QC => DIG_2 enable (no. of repeats right digit) QC => SEG_C
    * QD => DIG_3 enable (chain ch left digit)        QD => SEG_D
    * QE => DIG_4 enable (chain ch right digit)       QE => SEG_E
    * QF => nc                                        QF => SEG_F
    * QG => nc                                        QG => SEG_F
    * QH => nc                                        QH => SEG_DP
    */


    uint8_t digitData = (1 << digit); // enable the digit
    uint8_t segmentData = 0;
    int8_t chainTo = parts[partIndex].ChainTo();

    switch(digit) {
      case 0:
        for(int i=0; i<4; i++) {
          if(parts[partIndex].PageLedState(i))
            segmentData |= (1 << i);
          else
            segmentData &= ~(1 << i);
        }
        break;
      case 1:
        if(parts[partIndex].IsStarted())
          segmentData = digitToSegment28[parts[partIndex].RemainingRepeats() % 10];
        else
          segmentData = digitToSegment28[parts[partIndex].Repeats() % 10];
        break;
      case 2:
        if(parts[partIndex].IsStarted())
          segmentData = digitToSegment28[parts[partIndex].RemainingRepeats() / 10];
        else
          segmentData = digitToSegment28[parts[partIndex].Repeats() / 10];
        break;      
      case 3:
        segmentData = (chainTo==-1) ? digitToSegment28[11] : digitToSegment28[(chainTo+1) % 10];
        break;
      case 4:
        segmentData = (chainTo==-1) ? digitToSegment28[11] : digitToSegment28[(chainTo+1) / 10];
        break;          
    }
    write595byte(digitData, MSBFIRST);
    write595byte(segmentData, MSBFIRST);
  }

  static void staticAnalogPotChangedHandler(int partIndex, int pot, uint16_t value) {
    if (instance) {
      instance->onAnalogPotChangedHandler(partIndex, pot, value);
    }
  }  

  void onAnalogPotChangedHandler(int partIndex, int pot, uint16_t value) {
    // char s[100];
    // sprintf(s, "part: %d  pot: %d  value: %d", partIndex, pot, value);
    // Serial.println(s);
    // ###handle bad pots###
    if(partIndex == 2 && pot == 0) return;
    if(partIndex == 4 && pot == 1) return;
    if(partIndex == 6 && pot == 0) return;

    if(pot==0) {
      parts[partIndex].SetPageCountRaw(value);
    } else if(pot==1) {
      parts[partIndex].SetRepeatsRaw(value);
    } else if(pot==2) {
      parts[partIndex].SetChainToRaw(value);
    }

    // if(partProgrammingChangedCallback)
    //   partProgrammingChangedCallback(partIndex, parts[partIndex]);
  }

public:

  SongManagerUI(Channel (&partsArray)[PARTS]) 
    : programBtn(PROGRAM_BTN), 
      loadBtn(LOAD_BTN), 
      nextSongBtn(NEXT_SONG_BTN), 
      prevSongBtn(PREV_SONG_BTN),
      parts(partsArray),
      analogPotBank1(MUX_S0, MUX_S1, MUX_S2, MUX_S3, MUX_ENA, A10, A11, A12, PARTS),
      lastUpdate(0),
      lastScan(0),
      selectedSongNumber(0),
      prevSongNumber(0) {
        instance = this;
        lastClockInLed = 0;
        songIsLoading = false;
        programming = false;
      }

  void clock(unsigned long now) {
    clockInLed = true;
    lastClockInLed = now;
  }

  void onSongNumberSelected(void (*callback)(const int)) {
    songNumberSelectedCallback = callback;
  }

  void onProgrammingStarted(void (*callback)(const int)) {
    programmingStartedCallback = callback;
  }

  void onProgrammingEnded(void (*callback)(const int)) {
    programmingEndedCallback = callback;
  }  

  void onProgrammingCancelled(void (*callback)(const int)) {
    programmingCancelledCallback = callback;
  }  

  void onPartProgrammingChanged(void (*callback)(const int, Channel)) {
    partProgrammingChangedCallback = callback;
  }

  void onPartButtonPressed(void (*callback)(const int, Channel&, bool, bool)) {
    partButtonPressedCallback = callback;
  }

  void begin() {
    pinMode(LED_CLK, OUTPUT);
    pinMode(LED_DATA, OUTPUT);
    pinMode(LED_LATCH, OUTPUT);

    pinMode(BTN_LOAD, OUTPUT);
    pinMode(BTN_CLK, OUTPUT);
    pinMode(BTN_DATA, INPUT);
    pinMode(BTN_LATCH, OUTPUT);

    pinMode(MUX_S0, OUTPUT);
    pinMode(MUX_S1, OUTPUT);
    pinMode(MUX_S2, OUTPUT);
    pinMode(MUX_S3, OUTPUT);

    analogReadResolution(10);  // Set resolution to 10 bits
    analogReadAveraging(4);    // Average over 4 samples for stability    

    analogPotBank1.onChange(SongManagerUI::staticAnalogPotChangedHandler);
    analogPotBank1.setHysteresis(10);
    analogPotBank1.setPotHysteresis(2, 0, 20);
    analogPotBank1.setPotHysteresis(3, 0, 20);
    analogPotBank1.setPotHysteresis(6, 0, 20);
    analogPotBank1.setPotHysteresis(7, 0, 20);

    analogPotBank1.setPotHysteresis(3, 2, 20);
    analogPotBank1.setPotHysteresis(4, 2, 30);
    analogPotBank1.setPotHysteresis(6, 2, 20);
    analogPotBank1.setPotHysteresis(7, 2, 20);

    analogPotBank1.setSamplesPerRead(5);
    analogPotBank1.begin();    
  }

  void scan(unsigned long now) {
    if(now > (lastScan + SCAN_INTERVAL)) {
      lastScan = now;
      scanInputs(now);
    }
    if(programming && now > (lastPotScan + POT_SCAN_INTERVAL)) {
      lastPotScan = now;
      analogPotBank1.scan(now);
    }
//    testMux();
  }

  void startSongLoading(int songNumber) {
    prevSongNumber = selectedSongNumber;
    selectedSongNumber = songNumber;
    songIsLoading = true;
    songLoadingLed = false;
  }

  void endSongLoading() {
    prevSongNumber = selectedSongNumber;
    songIsLoading = false;
    songLoadingLed = true;
    programming = false;
  }

//unsigned long lastStatus = 0;
  void update(unsigned long now) {

    // if(now > (lastStatus + 2000)) {
    //   lastStatus = now;
    //   char s[100];
    //   sprintf(s, "UI STATE => selectedSongNumber: %d  prevSongNumer: %d  songIsLoading: %s  programming: %s"
    //   , selectedSongNumber
    //   , prevSongNumber
    //   , songIsLoading ? "yes" : "no"
    //   , programming ? "yes" : "no");
    //   Serial.println(s);
    // }

    if((songIsLoading || prevSongNumber != selectedSongNumber) && now > (lastSongBlink + LED_SHORT_PULSE)) {
      blinkSongNumber = !blinkSongNumber;
      lastSongBlink = now;
    }

    if(prevSongNumber == selectedSongNumber)
      blinkSongNumber = false;

    if(programming && now > (lastProgrammingLed + LED_SHORT_PULSE)) {
      programmingLed = !programmingLed;
      lastProgrammingLed = now;
    }

    if(songIsLoading && now > (lastSongLoadingLed + LED_SHORT_PULSE)) {
      songLoadingLed = !songLoadingLed;
      lastSongLoadingLed = now;
    }

    if(now > (lastClockInLed + LED_SHORT_PULSE)) {
      clockInLed = false;
      lastClockInLed = now;
    }


    for(int digit=0; digit<DIGITS; digit++) {
      digitalWrite(LED_LATCH, LOW);
      delayMicroseconds(1);
      updateOperationsBoardDigit(digit);
      for(int partIndex=0; partIndex<PARTS; partIndex++) {
        if(partIndex==4) {
          write595byte(0);
          write595byte(0);
        } else {
          updatePartDigit(partIndex, digit);
        }
      }
      digitalWrite(LED_LATCH, HIGH);
      //delayMicroseconds(10);
    }
  }

  void reset() {

  }

  void testMux() {

    int values[8] = {0};

    for (int i = 0; i < 8; i++) {
      digitalWrite(MUX_S0, (i & 0x01));
      digitalWrite(MUX_S1, (i & 0x02) >> 1);
      digitalWrite(MUX_S2, (i & 0x04) >> 2);
      digitalWrite(MUX_S3, LOW);

      delayMicroseconds(100);

      values[i] = analogRead(A12);

    }

    char s[100];
    sprintf(s, "%d  %d  %d", values[0], values[1], values[2]);
    Serial.println(s);
    sprintf(s, "%d  %d  %d", values[3], values[4], values[5]);
    Serial.println(s);
    sprintf(s, "%d  %d", values[6], values[7]);
    Serial.println(s);        
    Serial.println();
  }  

};

SongManagerUI* SongManagerUI::instance = nullptr;

#endif