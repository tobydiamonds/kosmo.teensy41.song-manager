#ifndef SongRepository_h
#define SongRepository_h

#include <SD.h>
#include <SPI.h>
#include "Common.h"
#include "Models.h"
#include "SongSerializer.h"
#include "SongDeserializer.h"

class SongRepository {

private:
  File writeFile;
  static SongRepository* instance;

  static void staticWriteLine(const String& line) {
    if(instance) {
      instance->writeLine(line);
    }
  }

  void writeLine(const String& line) {
    if(writeFile) {
      writeFile.println(line);
    } else {
      Serial.println("Error writing line to file.");
    }
  }

public:
  SongRepository() {
    instance = this;
  }

  void begin() {
    // Initialize the SD card
    if (!SD.begin(BUILTIN_SDCARD)) {
      Serial.println("Initialization failed!");
      return;
    }    
  }

  Song load(int index, bool &success) {
    char filename[15];
    snprintf(filename, sizeof(filename), "song_%d.dat", index);
    if(!SD.exists(filename)) {
      Serial.print("File does not exist: ");
      Serial.println(filename);
      success = false;
      return Song();
    }    
    File file = SD.open(filename);
    if(!file) {
      Serial.print("error accessing file: ");
      Serial.println(filename);
      success = false;
      return Song();    
    }    

    Song song;
    SongDeserializer deserializer(song);
    while (file.available()) {
      String line = file.readStringUntil('\n');
      int partIndex = deserializer.deserialize(line);
      if (partIndex == -1) {
        Serial.println("Error parsing line.");
        success = false;
        return Song();
      }
    }

    if(!file) {
      file.close();
      Serial.print("Song loaded from: ");
      Serial.println(filename);  
    }
    success = true;
    return song;
  }

  bool save(Song song, int index) {
    char filename[15];
    snprintf(filename, sizeof(filename), "song_%d.dat", index);

    // delete existing file
    if(SD.exists(filename)) {
      SD.remove(filename);
    }

    writeFile = SD.open(filename, FILE_WRITE);
    if (!writeFile) {
      Serial.print("Error opening file: ");
      Serial.println(filename);
    }

    SongSerializer serializer;
    serializer.serialize(song, SongRepository::staticWriteLine);

    if(writeFile) {
      writeFile.close();
    }

    Serial.print("Song saved to: ");
    Serial.println(filename);  
    return true;
  }

  bool list(int index) {
    char filename[15];
    snprintf(filename, sizeof(filename), "song_%d.dat", index);
    if(!SD.exists(filename)) {
      Serial.print("File does not exist: ");
      Serial.println(filename);
      return false;
    }    
    File file = SD.open(filename);
    if(!file) {
      Serial.print("error accessing file: ");
      Serial.println(filename);
      return false;
    }    

    while (file.available()) {
      String line = file.readStringUntil('\n');
      Serial.println(line);
    }

    if(file) {
      file.close();
    }
    return true;
  }  

};

SongRepository* SongRepository::instance = nullptr;


#endif