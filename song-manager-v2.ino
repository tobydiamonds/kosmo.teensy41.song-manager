#include <Wire.h>
//#include <SD.h>
//#include <SPI.h>

#include "Common.h"
#include "Models.h"
#include "I2CSlave.h"
#include "KosmoMasterI2CService.h"
#include "AutomationController.h"
#include "UI.h"
#include "Channel.h"

#define CLOCK_IN_PIN 12

// these pins are connected to the pinheader, but not in use
#define NOT_USED_2 9
#define NOT_USED_3 10
#define NOT_USED_4 11
#define NOT_USED_6 27 // analog pin


//const int chipSelect = BUILTIN_SDCARD;

DrumSequencerSlave drumSequencerSlave(9);
ClockSlave clockSlave(8);

I2CSlave* slaves[] = {&clockSlave,&drumSequencerSlave};
KosmoMasterI2CService master(slaves, 2);
EXTMEM AutomationController automationController;

EXTMEM Song currentSong;
InstructionPackage loadSong;

Channel parts[PARTS];
SongManagerUI ui(parts);


unsigned long now = 0;
int currentStep = 0;


void printStructureSizes() {
  Serial.print("Size of DrumSequencerChannel: ");
  Serial.println(sizeof(DrumSequencerChannel));

  Serial.print("Size of DrumSequencerPart: ");
  Serial.println(sizeof(DrumSequencerPart));
}

// void writeData(const char* filename, const char* data) {
//   // Open the file for writing
//   File file = SD.open(filename, FILE_WRITE);

//   if (file) {
//     file.println(data); // Write the data
//     Serial.println("Data written to file.");
//     file.close(); // Close the file
//   } else {
//     Serial.println("Error opening file for writing.");
//   }
// }

// void readData(const char* filename) {
//   // Open the file for reading
//   File file = SD.open(filename);

//   if (file) {
//     Serial.println("Reading from file:");
//     while (file.available()) {
//       Serial.write(file.read()); // Read and print data
//     }
//     file.close(); // Close the file
//   } else {
//     Serial.println("Error opening file for reading.");
//   }
// }
void setup() {


  //Wire.begin();
  Serial.begin(115200);

  // ui
  ui.onSongNumberSelected(onSongNumberSelected);
  ui.onProgrammingStarted(onProgrammingStarted);
  ui.onProgrammingEnded(onProgrammingEnded);
  ui.onProgrammingCancelled(onProgrammingCancelled);
  ui.onPartProgrammingChanged(onPartProgrammingChanged);
  ui.begin();

  // parts
  for(int i=0; i<PARTS; i++) {
    parts[i] = Channel(i, 7-i); // first parameter: channel nummber, second parameter: bit index of the channel button from 595
    parts[i].OnPartCompleted(onPartCompleted);
    parts[i].OnBeforePartCompleted(onBeforePartCompleted);
    parts[i].OnPartStarted(onPartStarted);
    parts[i].OnPartStopped(onPartStopped);
  }


  // Initialize the SD card
  // if (!SD.begin(chipSelect)) {
  //   Serial.println("Initialization failed!");
  //   return;
  // }

  // // Write data to a file
  // writeData("example.txt", "Hello, Teensy!");

  // // Read data from the file
  // readData("example.txt");  


  currentSong.parts[0].drumSequencerData.channel[0].divider = 6;
  currentSong.parts[0].drumSequencerData.channel[0].enabled = 1;
  currentSong.parts[0].drumSequencerData.channel[0].lastStep = 15;
  currentSong.parts[0].drumSequencerData.channel[0].page[0] = 0x8888;
  currentSong.parts[0].drumSequencerData.channel[1].divider = 6;
  currentSong.parts[0].drumSequencerData.channel[1].enabled = 1;
  currentSong.parts[0].drumSequencerData.channel[1].lastStep = 15;
  currentSong.parts[0].drumSequencerData.channel[1].page[0] = 0x4444;


  currentSong.parts[1].drumSequencerData.channel[0].divider = 6;
  currentSong.parts[1].drumSequencerData.channel[0].enabled = 1;
  currentSong.parts[1].drumSequencerData.channel[0].lastStep = 15;
  currentSong.parts[1].drumSequencerData.channel[0].page[0] = 0x8888;
  currentSong.parts[1].drumSequencerData.channel[1].divider = 6;
  currentSong.parts[1].drumSequencerData.channel[1].enabled = 1;
  currentSong.parts[1].drumSequencerData.channel[1].lastStep = 15;
  currentSong.parts[1].drumSequencerData.channel[1].page[0] = 0x4444;  
  currentSong.parts[1].drumSequencerData.channel[2].divider = 6;
  currentSong.parts[1].drumSequencerData.channel[2].enabled = 1;
  currentSong.parts[1].drumSequencerData.channel[2].lastStep = 15;
  currentSong.parts[1].drumSequencerData.channel[2].page[0] = 0xFFFF;    


  AutomationSequence sequence;
  sequence.startStep = 3;
  sequence.interval = 5;
  for(int i=0; i<5; i++) {
    Automation automation;
    automation.slaveAddress = 8;
    automation.target = 0xAA;
    automation.value = i*2;
    sequence.automations[i] = automation;
  }
  currentSong.parts[1].automationSequences[0] = sequence;

  automationController.onAutomation(onAutomation);

  master.onInstructionComplete(onInstructionComplete);
  master.onInstructionCancelled(onInstructionCancelled);

  Serial.println("Initialization done.");

  printStructureSizes();
}

void onSongNumberSelected(int songNumber) {
  Serial.print("Song number selected: ");
  Serial.println(songNumber);

  loadSong = master.sendSongParts(currentSong);
  
  Serial.println("LOADING SONG");
  printInstructionPackage(loadSong);  
}

void onProgrammingStarted(int songNumber) {
  Serial.print("Programming started for song number: ");
  Serial.println(songNumber);
}

void onProgrammingEnded(int songNumber) {
  Serial.print("Programming ended for song number: ");
  Serial.println(songNumber);
}

void onProgrammingCancelled(int songNumber) {
  Serial.print("Programming cancelled for song number: ");
  Serial.println(songNumber);
}

void onPartProgrammingChanged(const int partIndex, Channel part) {

  currentSong.parts[partIndex].pages = part.PageCount();
  currentSong.parts[partIndex].repeats = part.Repeats();
  currentSong.parts[partIndex].chainTo = part.ChainTo();
}

void onBeforePartCompleted(uint8_t partIndex, int8_t chainToPart) {
}

void onPartCompleted(uint8_t partIndex, int8_t chainToPart) {
}

void onPartStarted(uint8_t partIndex) {
}

void onPartStopped(uint8_t partIndex) {
}

void onAutomation(const Automation automation) {
  master.sendAutomation(automation.slaveAddress, automation);
}

void onInstructionCancelled(long traceId, uint8_t slaveAddress, Instruction instruction, uint8_t partIndex) {
  char s[100];
  sprintf(s, "on no - instruction cancelled => %d  slave:%d  part-index: %d  trace-id: %ld", instruction, slaveAddress, partIndex, traceId);
  Serial.println(s);

  if(traceId == loadSong.getTraceId()) {
    loadSong.clear();
  }  
}


void onInstructionComplete(long traceId, uint8_t slaveAddress, Instruction instruction, uint8_t partIndex) {
  char s[100];
  sprintf(s, "instruction completed => %d  slave:%d  part-index: %d  trace-id: %ld", instruction, slaveAddress, partIndex, traceId);
  Serial.println(s);

  // instructionLed = true;
  // lastInstructionLed = now;

  if(traceId == loadSong.getTraceId()) {
    loadSong.markCompleted(slaveAddress, instruction, partIndex);

    printInstructionPackage(loadSong);

    if(loadSong.isComplete()) {
      Serial.println("SONG LOADED");
      loadSong.clear();
      ui.endSongLoading();
    }
  }
}

uint8_t scanInput() {
  uint8_t incoming = 0;

  digitalWrite(BTN_LOAD, LOW);
  delayMicroseconds(5); // Small delay for load
  digitalWrite(BTN_LOAD, HIGH);
  delayMicroseconds(5); // Ensure load is complete

  for(int i=0; i<8; i++) {
    incoming <<= 1;
    incoming |= digitalRead(BTN_DATA);
    digitalWrite(BTN_CLK, HIGH);
    delayMicroseconds(5); // Small delay for clock pulse
    digitalWrite(BTN_CLK, LOW);
    delayMicroseconds(5); // Ensure stable reading
  }
  return incoming;
}


void updateUI(uint8_t data) {

  shiftOut(LED_DATA, LED_CLK, LSBFIRST, data);


  digitalWrite(LED_LATCH, LOW);
  digitalWrite(LED_LATCH, HIGH);
  digitalWrite(LED_LATCH, LOW);
}

void loop() {
  now = millis();

  ui.scan(now);

  // if(now > (lastCurrentStepChange + 128)) {
  //   lastCurrentStepChange = now;
  //   currentStep++;
  // }

  // if(now > (lastPartIndexChange + 2048)) {
  //   lastPartIndexChange = now;
  //   currentStep = 0;
  //   lastCurrentStepChange = now;

  //   if(currentPartIndex == PARTS) {
  //     currentPartIndex = 0;
  //   }

  //   if(currentPartIndex==0) {
  //     master.sendInstruction(8, Instruction::Start);
  //   }

  //   master.sendCurrentPartIndex(currentPartIndex);

  //   if(currentPartIndex==4) {
  //     Part part;
  //     part = master.retrievePartFromSlaves();
  //     //printSongPart(part, -1);
  //   }


  //   if(currentPartIndex==7) {

  //     master.sendInstruction(8, Instruction::Stop);

  //     songLoaded = false;
  //     songLoading = true;
  //     loadSong = master.sendSongParts(currentSong);
      
  //     Serial.println("LOADING SONG");
  //     printInstructionPackage(loadSong);
  //   }

  //   Serial.print("Loading automations for part ");
  //   Serial.println(currentPartIndex);
  //   automationController.load(currentSong.parts[currentPartIndex]);

  //   currentPartIndex++;
  // }

  automationController.run(now, currentStep);
  master.run(now);

  // if(songLoaded) {
  //   songLoadedLed = true;
  // }

  // if(songLoading && now > (lastSongLoadingLed + 200)) {
  //   lastSongLoadingLed = now;
  //   songLoadedLed = !songLoadedLed;
  // }

  // if(instructionLed && now > (lastInstructionLed + 150)) {
  //   lastInstructionLed = now;
  //   instructionLed = false;
  // }

  // if(songLoadedLed)
  //   ledPattern |= songLoadedPattern;
  // else
  //   ledPattern &= ~(songLoadedPattern);

  // if(instructionLed)
  //   ledPattern |= instructionLedPattern;
  // else 
  //   ledPattern &= ~(instructionLedPattern);
  // updateUI(ledPattern);


  for(int i=0; i<PARTS; i++) {
   
    parts[i].SetPageCount(2);
    parts[i].SetChainTo(-1);
    parts[i].SetRepeats(1);

    parts[i].Run(now);  
  }

  ui.update(now);
}


