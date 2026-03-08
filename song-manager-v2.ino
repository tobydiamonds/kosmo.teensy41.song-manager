#include <Wire.h>
//#include <SD.h>
//#include <SPI.h>

#include "Common.h"
#include "Models.h"
#include "I2CSlave.h"
#include "KosmoMasterI2CService.h"
#include "AutomationController.h"
#include "ui.h"
#include "Channel.h"

#define CLOCK_IN_PIN 12

// these pins are connected to the pinheader, but not in use
#define NOT_USED_2 9
#define NOT_USED_3 10

#define NOT_USED_6 27 // analog pin


//const int chipSelect = BUILTIN_SDCARD;

ClockSlave clockSlave(8);
DrumSequencerSlave drumSequencerSlave(9);
SamplerSlave samplerSlave(10);


I2CSlave* slaves[] = {&clockSlave, &drumSequencerSlave};
KosmoMasterI2CService master(slaves, 2);
EXTMEM AutomationController automationController;

EXTMEM Song currentSong;
InstructionPackage loadSong;

EXTMEM Channel parts[PARTS];
SongManagerUI* ui;


unsigned long now = 0;
unsigned long lastClockPulse = 0;
bool edgeDetected = false;
bool hasPulse = false;
bool reset = false;

int ppqnCounter = 0;
int currentStep = 0;
int currentPartIndex = 0;

bool partCompleted = false;
int completedPartIndex = -1;
bool chainToNextPart = false;
int nextPartIndex = -1;


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
  // parts
  for(int i=0; i<PARTS; i++) {
    parts[i] = Channel(i, 7-i); // first parameter: channel nummber, second parameter: bit index of the channel button from 595
    parts[i].OnPartCompleted(onPartCompleted);
    parts[i].OnBeforePartCompleted(onBeforePartCompleted);
    parts[i].OnPartStarted(onPartStarted);
    parts[i].OnPartStopped(onPartStopped);
  }

  // ui
  ui = new SongManagerUI(parts);  
  ui->onSongNumberSelected(onSongNumberSelected);
  ui->onProgrammingStarted(onProgrammingStarted);
  ui->onProgrammingEnded(onProgrammingEnded);
  ui->onProgrammingCancelled(onProgrammingCancelled);
  ui->onPartProgrammingChanged(onPartProgrammingChanged);
  ui->onPartButtonPressed(onPartButtonPressed);
  ui->begin();

  // clock in
  pinMode(CLOCK_IN_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(CLOCK_IN_PIN), onClockPulse, RISING);

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
  // copy all part data into song parts
  for(int i=0; i<PARTS; i++) {
    currentSong.parts[i].pages = parts[i].PageCount();
    currentSong.parts[i].repeats = parts[i].Repeats();
    currentSong.parts[i].chainTo = parts[i].ChainTo();
    currentSong.parts[i].clockData = parts[i].GetClockPart();
    currentSong.parts[i].drumSequencerData = parts[i].GetDrumSequencerPart();
    currentSong.parts[i].samplerData = parts[i].GetSamplerPart();
  }
  // save the song
}

void onProgrammingCancelled(int songNumber) {
  // revert part data from current song
  for(int i=0; i<PARTS; i++) {
    parts[i].SetPageCount(currentSong.parts[i].pages);
    parts[i].SetRepeats(currentSong.parts[i].repeats);
    parts[i].SetChainTo(currentSong.parts[i].chainTo);
    parts[i].SetClockPart(currentSong.parts[i].clockData);
    parts[i].SetDrumSequencerPart(currentSong.parts[i].drumSequencerData);
    parts[i].SetSamplerPart(currentSong.parts[i].samplerData);
  }  
}

void onPartButtonPressed(const int partIndex, Channel channel, bool programming, bool songIsLoading) {
  char s[100];
  sprintf(s, "button pressed: %d  programming: %s  loading: %s", partIndex, programming ? "yes" : "no", songIsLoading ? "yes" : "no");
  Serial.println(s);
  if(songIsLoading) return;
  if(programming && channel.PageCount()==0) { 
    // when we click the button for a part that is not in use we want to initialize slaves with default values to simplify starting a new part
    channel.SetPageCount(1);
    channel.SetRepeats(4);
    channel.SetChainTo(partIndex); // chain to self
    channel.SetClockPart(InitClockPart());
    channel.SetDrumSequencerPart(InitDrumSequencerPart());
    channel.SetSamplerPart(InitSamplerPart());    
    Serial.print("initialized channel: ");
    Serial.println(partIndex);
    channel.Print();
  } else if (programming) {
    // get data from slaves and store in parts
    Part incoming;
    Serial.println("retreving data from slaves");
    incoming = master.retrievePartFromSlaves();
    printSongPart(incoming, partIndex);

    channel.SetClockPart(incoming.clockData);
    channel.SetDrumSequencerPart(incoming.drumSequencerData);
    channel.SetSamplerPart(incoming.samplerData);
  } else if(parts[currentPartIndex].IsStarted()) {
    // set part that were pressed as next part to the current part
    parts[currentPartIndex].SetChainTo(partIndex);
  } else {
    // send part to slaves
    master.sendCurrentPartIndex(partIndex);
    master.sendInstruction(clockSlave.getAddress(), Instruction::Start);
    channel.Start();
  }
}

void onPartProgrammingChanged(const int partIndex, Channel part) {
// do we need this?
}

void onBeforePartCompleted(uint8_t partIndex, int8_t chainToPart) {
  if(chainToPart == -1) return;
  // send the next part index to slaves so they can prepare data on the next down beat
  master.sendCurrentPartIndex(chainToPart);
}

void onPartCompleted(uint8_t partIndex, int8_t chainToPart) {
  completedPartIndex = partIndex;
  partCompleted = true;  

  if(chainToPart == -1) {
    // if not next part we want to stop the clock
    master.sendInstruction(clockSlave.getAddress(), Instruction::Stop);
    Serial.println("no chain - stopping the clock");
    chainToNextPart = false;
    nextPartIndex = -1;
  } else {
    // otherwise we want to chain to the next part 
    nextPartIndex = chainToPart;
    chainToNextPart = true;
  }
}

void onPartStarted(uint8_t partIndex) {
  currentPartIndex = partIndex;
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
      ui->endSongLoading();
    }
  }
}

bool AnyPartsPlaying() {
  for(int i=0; i<PARTS; i++) {
    if(parts[i].IsStarted()) return true;
  }
  return false;
}

void onClockPulse() {
  lastClockPulse = now;  
  edgeDetected = true;
  hasPulse = true;
}

void triggerClockPulse() {
  ppqnCounter = (ppqnCounter + 1) % 24;
  for(int i=0; i<PARTS; i++)
    parts[i].Pulse(ppqnCounter);
  if(ppqnCounter == 0) {
    ui->setLastClock(now);
  }
}

void loop() {
  now = millis();

  // handle reset
  if(now > (lastClockPulse + 2000) && hasPulse) {
    reset = true;
    hasPulse = false;
  }

  if(reset) {
    reset = false;
    ppqnCounter = 0;
    for(int i=0; i<PARTS; i++) {
      parts[i].Reset();
    }
    currentPartIndex = 0;
    ui->reset();
    Serial.println("reset!");
  }

  // handle clock in
  if(edgeDetected) {
    edgeDetected = false;
    if(!AnyPartsPlaying())
      parts[currentPartIndex].Start();
    triggerClockPulse();
  }  

  if(partCompleted && ppqnCounter == 0 && completedPartIndex >= 0) {
    partCompleted = false;
    parts[completedPartIndex].Stop();
  }

  if(chainToNextPart && ppqnCounter == 0 && nextPartIndex >= 0) {
    chainToNextPart = false;
    parts[nextPartIndex].Start();
  }

  ui->scan(now);
  automationController.run(now, currentStep);
  master.run(now);

  for(int i=0; i<PARTS; i++) {
    parts[i].Run(now);  
  }
  ui->update(now);   

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



}


