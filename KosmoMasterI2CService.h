#ifndef KosmoMasterI2CService_h
#define KosmoMasterI2CService_h

#include <Arduino.h>
#include <Wire.h>
#include "Common.h"
#include "Models.h"
#include "I2CSlave.h"

class KosmoMasterI2CService {
private:
  I2CSlave** slaves;
  int numSlaves;
  /* callback when instruction has completed to single client
  *  params:
  *    traceId: long
  *    slave address: uint8_t
  *    instruction: Instruction enum
  *    partIndex, uint8_t
  */
  void (*instructionCompleteCallback)(const long, const uint8_t, const Instruction, const uint8_t) = nullptr;
  void (*instructionCancelledCallback)(const long, const uint8_t, const Instruction, const uint8_t) = nullptr;


  bool sendToSlave(I2CSlave* slave, Instruction instruction, uint8_t partIndex, const uint8_t* data, size_t dataSize) {
    // Prepare the first byte with instruction and part index
    uint8_t firstByte = (static_cast<uint8_t>(instruction) & 0xF0) | (partIndex & 0x0F);

    // Calculate the total size including the first byte in each chunk
    size_t totalChunks = (dataSize + I2C_CHUNK_MAX - 1) / I2C_CHUNK_MAX;
    if(dataSize == 0)
      totalChunks = 1;

    // char s[100];
    // sprintf(s, "Sending part in %d chunks - total size: %d", totalChunks, dataSize);
    // Serial.println(s);

    for (size_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
      size_t offset = chunkIndex * I2C_CHUNK_MAX;
      size_t chunkSize = min<size_t>(I2C_CHUNK_MAX, dataSize - offset);

      uint8_t buffer[I2C_CHUNK_MAX + 2]; // Buffer for instruction byte + chunk index + data
      buffer[0] = firstByte; // Prepend the instruction byte
      buffer[1] = chunkIndex; // Add chunk index as the second byte
      if(dataSize > 0) {
        memcpy(buffer + 2, data + offset, chunkSize);
      }

      // sprintf(s, "chunk: %d  offset: %d  size: %d", chunkIndex, offset, chunkSize);
      // Serial.println(s);

      Wire.beginTransmission(slave->getAddress());
      Wire.write(buffer, chunkSize + 2); // Send chunk size plus instruction byte and chunk index
      int result = Wire.endTransmission();

      if(totalChunks > 0)
        delay(slave->getChunkTxInterval());
      
      if (result != 0) {
        Serial.print("Error while sending data to slave at address ");
        Serial.print(slave->getAddress(), HEX);
        Serial.print(" error code: ");
        Serial.println(result);
        return false;
      }
    }
    return true;
  }

  
  uint8_t* requestFromSlave(uint8_t address, size_t size) {
    size_t totalChunks = (size + I2C_MAX-1) / I2C_MAX;
    uint8_t* buffer = new uint8_t[size];

    for(size_t chunkIndex=0; chunkIndex<totalChunks; chunkIndex++) {
      size_t offset = chunkIndex * I2C_MAX;
      size_t chunkSize = min<size_t>(I2C_MAX, size - offset);
      Wire.requestFrom(address, chunkSize);    
      size_t bytesRead = 0;

      while(Wire.available()) {
        buffer[offset + bytesRead++] = Wire.read();
      }
    }  
    return buffer;
  }    



public:
  KosmoMasterI2CService(I2CSlave* slaves[], int numSlaves)
    : slaves(slaves), numSlaves(numSlaves) {
      Wire.begin();
      //Wire.setClock(400000);
    }

  void onInstructionComplete(void (*callback)(const long, const uint8_t, const Instruction, const uint8_t)) {
    instructionCompleteCallback = callback;
  }

  void onInstructionCancelled(void (*callback)(const long, const uint8_t, const Instruction, const uint8_t)) {
    instructionCancelledCallback = callback;
  }

  void run(unsigned long now) {
    for(int i=0; i<numSlaves; i++) {
      InstructionWrapper pending = slaves[i]->getPendingInstruction();
      if(pending.opcode != Instruction::None) {
        InstructionWrapper inProgress = slaves[i]->getInstructionInProgress();
        if(inProgress.opcode == Instruction::None) { // if not slave is currently processing an instruction
          // char s[100];
          // sprintf(s, "slave %d: setting pending instruction %d as next - queue-size: %d - retry: %d - trace-id: %d", i, pending.opcode, slaves[i]->getQueueSize(), slaves[i]->retries, pending.traceId);
          // Serial.print(s);
          // printInstructionPayload(pending.payload);
          // Serial.println();

          slaves[i]->setPendingInstructionInProgress(now);
        }

        unsigned long nextInstructionTime = slaves[i]->getNextInstructionTime();
        if(now >= nextInstructionTime) {
          slaves[i]->setLastInstructionTime(now);
          slaves[i]->lastExecutionResult = sendToSlave(slaves[i], pending.opcode, pending.payload.partIndex, pending.payload.data, pending.payload.size);

          if(slaves[i]->lastExecutionResult) {
            InstructionWrapper current = slaves[i]->getInstructionInProgress();
            long traceId = slaves[i]->completeInstruction();
            if(instructionCompleteCallback)
              instructionCompleteCallback(traceId, slaves[i]->getAddress(), current.opcode, current.payload.partIndex);

          } else if(slaves[i]->retries < I2C_RETRY_LIMIT) {
            char s[100];
            sprintf(s, "slave %d: instruction failed - queue-size: %d - retries: %d", i, slaves[i]->getQueueSize(), slaves[i]->retries);
            Serial.println(s);

            slaves[i]->retries++;
            unsigned long lastInstructionTime = slaves[i]->getLastInstructionTime();
            slaves[i]->setNextInstructionTime(lastInstructionTime + slaves[i]->getRetryInterval());
          } else {
            char s[100];
            sprintf(s, "slave %d: instruction cancelled - queue-size: %d - retries: %d", i, slaves[i]->getQueueSize(), slaves[i]->retries);
            Serial.println(s);      

            InstructionWrapper current = slaves[i]->getInstructionInProgress();
            long traceId = slaves[i]->cancelInstruction();
            if(instructionCancelledCallback)
              instructionCancelledCallback(traceId, slaves[i]->getAddress(), current.opcode, current.payload.partIndex);            
          }          
        }
      }
    }
  }


  InstructionPackage sendSongParts(const Song& song) {
    long traceId = millis();
    InstructionPackage package(traceId);
    for(int j=0; j<numSlaves; j++) {
      size_t dataSize = slaves[j]->getDataSize();
      for(int i=0; i<PARTS; i++) {
        InstructionPayload payload;
        payload.partIndex = i;
        payload.data = slaves[j]->getData(song.parts[i]);
        payload.size = dataSize;

        slaves[j]->addPendingInstruction(Instruction::SetParts, payload, traceId);

        package.add(Instruction::SetParts, slaves[j]->getAddress(), i);
      }
    }
    return package;
  }

  InstructionPackage sendCurrentPartIndex(uint8_t partIndex) {
    long traceId = millis();
    InstructionPackage package(traceId);
    InstructionPayload payload;
    payload.partIndex = partIndex;
    payload.data = nullptr;
    payload.size = 0;
    for (int i = 0; i < numSlaves; i++) {
      slaves[i]->addPendingInstruction(Instruction::SetPartIndex, payload, traceId);
      package.add(Instruction::SetPartIndex, slaves[i]->getAddress(), partIndex);
    }
    return package;
  }

  InstructionPackage sendInstruction(uint8_t address, Instruction instruction) {
    long traceId = millis();
    InstructionPackage package(traceId);
    InstructionPayload payload;
    payload.partIndex = 0;
    payload.data = nullptr;
    payload.size = 0;    
    for(int i=0; i<numSlaves; i++) {
      if(slaves[i]->getAddress()==address) {
        slaves[i]->addPendingInstruction(instruction, payload, traceId);
        package.add(instruction, address, 0);
      }
    }
    return package;
  }

  InstructionPackage sendAutomation(uint8_t address, Automation automation) {
    long traceId = millis();
    InstructionPackage package(traceId);
    InstructionPayload payload;
    payload.partIndex = 0;
    payload.data = reinterpret_cast<const uint8_t*>(&automation);
    payload.size = sizeof(Automation);    
    for(int i=0; i<numSlaves; i++) {
      if(slaves[i]->getAddress()==address) {
        slaves[i]->addPendingInstruction(Instruction::SetAutomation, payload, traceId);
        package.add(Instruction::SetAutomation, address, 0);
      }
    }
    return package;    
  }


  Part retrievePartFromSlaves() {
    Part part;
    for (int j = 0; j < numSlaves; j++) {
      uint8_t slaveAddress = slaves[j]->getAddress();
      size_t dataSize = slaves[j]->getDataSize();      
      uint8_t* buffer = requestFromSlave(slaveAddress, dataSize);
      slaves[j]->setData(part, buffer, dataSize);
      delete[] buffer;
    } 
    return part;   
  }


};

#endif