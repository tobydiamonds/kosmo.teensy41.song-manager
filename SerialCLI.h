#ifndef SerialCLI_h
#define SerialCLI_h

#include "Common.h"

class SerialCLI {
private:
  // song operations
  void (*loadSongCallback)(const int) = nullptr; // song index
  void (*saveSongCallback)(const int) = nullptr; // song index
  void (*printSongCallback)(void) = nullptr;
  void (*listSongCallback)(const int) = nullptr; // song index
  void (*stopSongCallback)(void) = nullptr;
  void (*initSongCallback)(void) = nullptr;
  void (*applySongCallback)(void) = nullptr;
  // part operations
  void (*startPartCallback)(const int) = nullptr; // part index
  void (*printPartCallback)(const int) = nullptr; // part index
  // general
  void (*deserializeCommandCallback)(const String) = nullptr; // command
public:

  void onLoadSong(void (*callback)(const int)) {
    loadSongCallback = callback;
  }

  void onSaveSong(void (*callback)(const int)) {
    saveSongCallback = callback;
  }  

  void onPrintSong(void (*callback)(void)) {
    printSongCallback = callback;
  }    

  void onListSong(void (*callback)(const int)) {
    listSongCallback = callback;
  }    

  void onStopSong(void (*callback)(void)) {
    stopSongCallback = callback;
  }  

  void onInitSong(void (*callback)(void)) {
    initSongCallback = callback;
  }    

  void onApplySong(void (*callback)(void)) {
    applySongCallback = callback;
  }   

  void onStartPartSong(void (*callback)(const int)) {
    startPartCallback = callback;
  }  

  void onPrintPartSong(void (*callback)(const int)) {
    printPartCallback = callback;
  }    

  void onDeserializeCommand(void (*callback)(const String)) {
    deserializeCommandCallback = callback;
  }

  void run() {
    if(!Serial.available()) return;

    String command = Serial.readStringUntil('\n');
    command.trim();
    if(command.indexOf("load ")==0) {
      int songIndex=-1;
      int size=0;
      String* parts = splitString(command, ' ', size);
      if(size == 2 && tryGetInt(parts[1], songIndex) && songIndex > 0 && loadSongCallback) {
        loadSongCallback(songIndex);
      }
    } else if(command.indexOf("save ")==0) {
      int songIndex=-1;
      int size=0;
      String* parts = splitString(command, ' ', size);
      if(size == 2 && tryGetInt(parts[1], songIndex) && songIndex > 0 && saveSongCallback) {
        saveSongCallback(songIndex);
      }      
    } else if(command=="print") {
      if(printSongCallback) printSongCallback();
    } else if(command.indexOf("list ")==0) {
      int songIndex=-1;
      int size=0;
      String* parts = splitString(command, ' ', size);
      if(size == 2 && tryGetInt(parts[1], songIndex) && songIndex > 0 && listSongCallback) {
        listSongCallback(songIndex);
      }
    } else if(command.indexOf("start ")==0) {
      int partToStart=-1;
      int size=0;
      String* parts = splitString(command, ' ', size);
      if(size == 2 && tryGetInt(parts[1], partToStart) && partToStart >= 0 && partToStart < PARTS && startPartCallback) {
        startPartCallback(partToStart);
      }      
    } else if(command=="stop") {
      if(stopSongCallback) stopSongCallback();
    } else if(command=="init") {
      if(initSongCallback) initSongCallback();
    } else if (command=="apply") {
      if(applySongCallback) applySongCallback();
    } else if (command.indexOf('?') == 0) {
      String number = command.substring(1);
      int index=number.toInt();
      if(index >= 0 && index < PARTS && printPartCallback) {
        printPartCallback(index);
      }
    } else {
      if(deserializeCommandCallback) deserializeCommandCallback(command);
    }
  }     
};

#endif