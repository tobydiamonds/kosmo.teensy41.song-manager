#include <Wire.h>

#include "Common.h"
#include "Models.h"
#include "I2CSlave.h"
#include "KosmoMasterI2CService.h"
#include "AutomationController.h"
#include "ui.h"
#include "Channel.h"
#include "SongRepository.h"
#include "SongDeserializer.h"
#include "SerialCLI.h"

#define CLOCK_IN_PIN 12

// these pins are connected to the pinheader, but not in use
#define NOT_USED_2 9
#define NOT_USED_3 10

#define NOT_USED_6 27 // analog pin

ClockSlave clockSlave(8);
DrumSequencerSlave drumSequencerSlave(9);
SamplerSlave samplerSlave(10);


I2CSlave* slaves[] = {&clockSlave, &drumSequencerSlave};
KosmoMasterI2CService master(slaves, 2);
EXTMEM AutomationController automationController;

EXTMEM Song currentSong;
InstructionPackage songLoaderInstruction;

EXTMEM Channel parts[PARTS];
SongManagerUI* ui;

SongRepository songRepository;
SerialCLI serialCLI;


unsigned long now = 0;
unsigned long lastClockPulse = 0;
bool edgeDetected = false;
bool hasPulse = false;
bool reset = false;

int ppqnCounter = 0;
int currentStep = 0;
int currentPartIndex = 0;
int currentSongNumber = 0;

bool partCompleted = false;
int completedPartIndex = -1;
bool chainToNextPart = false;
int nextPartIndex = -1;


void setup() {
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
  pinMode(CLOCK_IN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLOCK_IN_PIN), onClockPulse, RISING);

  // sd card
  songRepository.begin();

  // master-slave
  automationController.onAutomation(onAutomation);
  master.onInstructionComplete(onInstructionComplete);
  master.onInstructionCancelled(onInstructionCancelled);

  // cli
  serialCLI.onDeserializeCommand(onDeserializeCommand);
  serialCLI.onApplySong(onApplySong);
  serialCLI.onInitSong(onInitSong);
  serialCLI.onListSong(onListSong);
  serialCLI.onPrintSong(onPrintSong);
  serialCLI.onLoadSong(onLoadSong);
  serialCLI.onSaveSong(onSaveSong);
  serialCLI.onStopSong(onStopSong);
  serialCLI.onPrintPartSong(onPrintPartSong);
  serialCLI.onStartPartSong(onStartPartSong);

  Serial.println("Initialization done.");


  // AutomationSequence sequence;
  // sequence.startStep = 3;
  // sequence.interval = 5;
  // for(int i=0; i<5; i++) {
  //   Automation automation;
  //   automation.slaveAddress = 8;
  //   automation.target = 0xAA;
  //   automation.value = i*2;
  //   sequence.automations[i] = automation;
  // }
  // currentSong.parts[1].automationSequences[0] = sequence;


}



void applyCurrentSongToPart(const int partIndex) {
  parts[partIndex].SetPageCount(currentSong.parts[partIndex].pages);
  parts[partIndex].SetChainTo(currentSong.parts[partIndex].chainTo);
  parts[partIndex].SetRepeats(currentSong.parts[partIndex].repeats);        
  parts[partIndex].SetDrumSequencerPart(currentSong.parts[partIndex].drumSequencerData);
  parts[partIndex].SetClockPart(currentSong.parts[partIndex].clockData);
  parts[partIndex].SetSamplerPart(currentSong.parts[partIndex].samplerData);
}

void applyCurrentSongToParts() {
  for(int i=0; i<PARTS; i++) {
    applyCurrentSongToPart(i);
  }
}

void loadTheSong(int songNumber) {
  Serial.print("LOADING SONG ");
  Serial.println(songNumber);
  ui->startSongLoading(songNumber);
  bool success;
  currentSong = songRepository.load(songNumber, success);
  if(success) {
    currentSongNumber = songNumber;
    master.cancelAllInstructions();
    songLoaderInstruction = master.sendSongParts(currentSong);  
  } else {
    ui->endSongLoading();
    Serial.println("ERROR LOADING SONG!!!");
  }
}

// CLI handlers
  void onLoadSong(const int songNumber) {
    loadTheSong(songNumber);
  }

  void onSaveSong(const int songNumber) {
    songRepository.save(currentSong, songNumber);
  }  

  void onPrintSong() {
    // print currentSong object
    printSong(currentSong);
  }    

  void onListSong(const int songIndex) {
    // list song from disc
    songRepository.list(songIndex);
  }    

  void onStopSong() {
    master.sendInstruction(clockSlave.getAddress(), Instruction::Stop);
  }  

  void onInitSong() {
    currentSong = Song();
    applyCurrentSongToParts();
  }    

  void onApplySong() {
    applyCurrentSongToParts();
    int partIndex = currentSong.firstPart();
    if(partIndex >= 0) {
      master.sendCurrentPartIndex(partIndex);
    }
  }   

  void onStartPartSong(const int partIndex) {
    master.sendCurrentPartIndex(partIndex);
    master.sendInstruction(clockSlave.getAddress(), Instruction::Start);
    parts[partIndex].Start();
  }  

  void onPrintPartSong(const int partIndex) {
    printSongPart(currentSong.parts[partIndex], partIndex);
  }    

  void onDeserializeCommand(const String command) {
    SongDeserializer deserializer(currentSong);
    int partIndex = deserializer.deserialize(command);
    if(partIndex >= 0) {
      applyCurrentSongToPart(partIndex);
    }
  }

// operation board handlers

void onSongNumberSelected(int songNumber) {
  loadTheSong(songNumber);
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
  songRepository.save(currentSong, songNumber);
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

// parts

void onPartButtonPressed(const int partIndex, Channel& channel, bool programming, bool songIsLoading) {
  char s[100];
  sprintf(s, "button pressed: %d  programming: %s  loading: %s", partIndex, programming ? "yes" : "no", songIsLoading ? "yes" : "no");
  Serial.println(s);
  channel.Print();
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


// automations

void onAutomation(const Automation automation) {
  master.sendAutomation(automation.slaveAddress, automation);
}

// instructions

void onInstructionCancelled(long traceId, uint8_t slaveAddress, Instruction instruction, uint8_t partIndex) {
  char s[100];
  sprintf(s, "instruction cancelled => %d  slave:%d  part-index: %d  trace-id: %ld", instruction, slaveAddress, partIndex, traceId);
  Serial.println(s);

  if(traceId == songLoaderInstruction.getTraceId()) {
    songLoaderInstruction.clear();
  }  
}

void onInstructionComplete(long traceId, uint8_t slaveAddress, Instruction instruction, uint8_t partIndex) {
  char s[100];
  sprintf(s, "instruction completed => %d  slave:%d  part-index: %d  trace-id: %ld", instruction, slaveAddress, partIndex, traceId);
  Serial.println(s);

  if(traceId == songLoaderInstruction.getTraceId()) {
    songLoaderInstruction.markCompleted(slaveAddress, instruction, partIndex);

    printInstructionPackage(songLoaderInstruction);

    if(songLoaderInstruction.isComplete()) {
      Serial.println("SONG LOADED");
      songLoaderInstruction.clear();
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

#define DEBOUNCE_THRESHOLD 3000  // Microseconds
#define MOVING_AVERAGE_SIZE 5
unsigned long lastPulseTime = 0;
unsigned long pulseInterval = 0;
float bpmValues[MOVING_AVERAGE_SIZE];
int bpmIndex = 0;

void onClockPulse() {
  unsigned long currentTime = micros();
  pulseInterval = currentTime - lastPulseTime;

  if (pulseInterval > DEBOUNCE_THRESHOLD) {
    lastPulseTime = currentTime;
    float currentBPM = (60000000.0 / pulseInterval) / 24;

    // Store BPM in moving average array
    bpmValues[bpmIndex] = currentBPM;
    bpmIndex = (bpmIndex + 1) % MOVING_AVERAGE_SIZE;

    // Calculate moving average
    float bpmSum = 0.0;
    for (int i = 0; i < MOVING_AVERAGE_SIZE; i++) {
      bpmSum += bpmValues[i];
    }
    float averageBPM = bpmSum / MOVING_AVERAGE_SIZE;

    // Check stability
    if (abs(currentBPM - averageBPM) < 5.0) {  // Adjust threshold as needed
      edgeDetected = true;
      lastClockPulse = now;
    } else {
      edgeDetected = false;
    }

    // char s[100];
    // sprintf(s, "Current BPM: %f, Average BPM: %f", currentBPM, averageBPM);
    // Serial.println(s);
  }
  hasPulse = true;
}

void triggerClockPulse() {
  ppqnCounter = (ppqnCounter + 1) % 24;
  parts[currentPartIndex].Pulse(ppqnCounter);
  // for(int i=0; i<PARTS; i++)
  //   parts[i].Pulse(ppqnCounter);
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

  serialCLI.run();

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

}