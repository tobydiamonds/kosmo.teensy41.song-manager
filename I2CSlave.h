#ifndef I2CSlave_h
#define I2CSlave_h

#include "Common.h"
#include "Models.h"

class I2CSlave {
private:
  InstructionQueue pendingInstructions;
  InstructionWrapper instructionInProgress;
  unsigned long lastInstructionTime;
  unsigned long nextInstructionTime;

  void cleanInstruction(InstructionWrapper& instruction) {
    instruction.opcode = Instruction::None;
    instruction.payload.partIndex = 0;
    instruction.payload.size = 0;
    // if (instruction.payload.data) {
    //   delete[] instruction.payload.data;
    //   instruction.payload.data = nullptr;
    // }
  }
  
protected:
  uint8_t address;
public:
  I2CSlave(uint8_t address) : lastInstructionTime(0), nextInstructionTime(0), address(address) {
    lastExecutionResult = true;
    retries = 0;
    instructionInProgress.opcode = Instruction::None;
  }

  bool lastExecutionResult;
  int retries;

  virtual const uint8_t* getData(const Part& part) = 0;
  virtual void setData(Part &part, const uint8_t* buffer, size_t size) = 0;
  virtual size_t getDataSize() = 0;
  virtual int getChunkTxInterval() = 0;
  virtual int getRetryInterval() = 0;

  uint8_t getAddress() const {
    return address;
  }

  InstructionWrapper getPendingInstruction() {
    InstructionWrapper instruction;
    if(pendingInstructions.peek(instruction))
      return instruction;
    else {
      instruction.opcode = Instruction::None;
      return instruction;
    }
  }

  InstructionWrapper getInstructionInProgress() {
    return instructionInProgress;
  }

  unsigned long getLastInstructionTime() {
    return lastInstructionTime;
  }

  void setLastInstructionTime(unsigned long t) {
    lastInstructionTime = t;
  }

  unsigned long getNextInstructionTime() {
    return nextInstructionTime;
  }

  void setNextInstructionTime(unsigned long t) {
    nextInstructionTime = t;
  }

  long addPendingInstruction(Instruction opcode, InstructionPayload payload, long traceId = millis()) {
    InstructionWrapper instruction;
    instruction.opcode = opcode;
    instruction.payload = payload;
    instruction.traceId = traceId;
    if(!pendingInstructions.push(instruction)) {
      Serial.println("UNABLE TO ADD INSTRUCTION - QUEUE IS FULL!!!");
      return -1;
    }
    return instruction.traceId;
  }

  void setPendingInstructionInProgress(unsigned long now) {
    if(!pendingInstructions.peek(instructionInProgress))
      instructionInProgress.opcode = Instruction::None;
    lastInstructionTime = now;
  }

  long completeInstruction() {
    long traceId = instructionInProgress.traceId;
    InstructionWrapper pending;
    if(pendingInstructions.pop(pending))
      cleanInstruction(pending);
    cleanInstruction(instructionInProgress);
    retries = 0;    
    nextInstructionTime = 0;
    return traceId;
  }

  long cancelInstruction() {
    long traceId = instructionInProgress.traceId;
    InstructionWrapper pending;
    if(pendingInstructions.pop(pending))
      cleanInstruction(pending);
    cleanInstruction(instructionInProgress);

    retries = 0;
    lastExecutionResult = true;
    lastInstructionTime = 0;
    nextInstructionTime = 0;
    return traceId;
  }

  int getQueueSize() {
    return pendingInstructions.size();
  }

  void clearAllPendingInstructions() {
    while(pendingInstructions.size()>0) {
      InstructionWrapper pending;
      if(pendingInstructions.pop(pending))
        cleanInstruction(pending);      
    }
  }
};

class ClockSlave : public I2CSlave {
public:
  ClockSlave(uint8_t address) : I2CSlave(address) {}

  const uint8_t* getData(const Part& part) override {
    return reinterpret_cast<const uint8_t*>(&part.clockData);
  }

  void setData(Part &part, const uint8_t* buffer, size_t size) override {
    memcpy(&part.clockData, buffer, size);
  }

  size_t getDataSize() override {
    return sizeof(ClockPart);
  }

  int getChunkTxInterval() override {
    return 0;
  }  

  int getRetryInterval() override {
    return 100;
  }
};

class DrumSequencerSlave : public I2CSlave {
public:
  DrumSequencerSlave(uint8_t address) : I2CSlave(address) {}

  const uint8_t* getData(const Part& part) override {
    return reinterpret_cast<const uint8_t*>(&part.drumSequencerData);
  }

  void setData(Part &part, const uint8_t* buffer, size_t size) override {
    memcpy(&part.drumSequencerData, buffer, size);
  }

  size_t getDataSize() override {
    return sizeof(DrumSequencerPart);
  }

  int getChunkTxInterval() override {
    return 0;
  }  

  int getRetryInterval() override {
    return 100;
  }
};

class SamplerSlave : public I2CSlave {
public:
  SamplerSlave(uint8_t address) : I2CSlave(address) {}

  const uint8_t* getData(const Part& part) override {
    return reinterpret_cast<const uint8_t*>(&part.samplerData);
  }

  void setData(Part &part, const uint8_t* buffer, size_t size) override {
    memcpy(&part.samplerData, buffer, size);
  }  

  size_t getDataSize() override {
    return sizeof(SamplerPart);
  }

  int getChunkTxInterval() override {
    return 0;
  }    

  int getRetryInterval() override {
    return 100;
  }  
};

#endif