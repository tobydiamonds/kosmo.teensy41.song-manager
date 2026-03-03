#ifndef Common_h
#define Common_h

#define I2C_MAX 32
#define I2C_CHUNK_MAX 30
#define I2C_RETRY_LIMIT 10

#define DRUM_CHANNELS 5
#define PARTS 8
#define MAX_AUTOMATION_SEQUENCES_PR_PART 5
#define MAX_AUTOMATIONS_PR_SEQUENCE 1000



void printByte(uint8_t data) {
  char s[9];
  for (int i = 0; i < 8; i++) {
    s[i] = (data & (1 << (7 - i))) ? '1' : '0';
  }
  s[8] = '\0'; // Null terminator for the string  
  Serial.print(s);
}

void printByteln(uint8_t data) {
  printByte(data);
  Serial.println();
}


void printInt(uint16_t value) {
  for(int i=0; i<16; i++) {
    if((value >> i) & 1) Serial.print("1");
    else Serial.print("0");
  }
  Serial.print(" ");
}

void printIntln(uint16_t value) {
  printInt(value);
  Serial.println();
}


enum Instruction {
  None = 0x00,
  SetPartIndex = 0x10, 
  SetParts = 0x20,     
  Start = 0x30,        
  Stop = 0x40,         
  InitPart = 0x50,     
  InitParts = 0x60,
  SetAutomation = 0x70,
  Reset = 0xF0
};

struct InstructionPayload {
  uint8_t partIndex;
  const uint8_t* data;
  size_t size;
};

struct InstructionWrapper {
  Instruction opcode;
  InstructionPayload payload;
  long traceId;
};

#define MAX_QUEUE_SIZE 10

class InstructionQueue {
private:
  InstructionWrapper queue[MAX_QUEUE_SIZE];
  int front;
  int rear;
  int count;

public:
  InstructionQueue() : front(0), rear(0), count(0) {}

  bool push(const InstructionWrapper& instruction) {
    if (count < MAX_QUEUE_SIZE) {
      queue[rear] = instruction;
      rear = (rear + 1) % MAX_QUEUE_SIZE;
      count++;
      return true;
    }
    return false; // Queue is full
  }

  bool pop(InstructionWrapper& instruction) {
    if (count > 0) {
      instruction = queue[front];
      front = (front + 1) % MAX_QUEUE_SIZE;
      count--;
      return true;
    }
    return false; // Queue is empty
  }

  bool peek(InstructionWrapper& instruction) const {
    if (count > 0) {
      instruction = queue[front];
      return true;
    }
    return false; // Queue is empty
  }

  bool isEmpty() const {
    return count == 0;
  }

  bool isFull() const {
    return count == MAX_QUEUE_SIZE;
  }

  int size() {
    return count;
  }
};

#define MAX_INSTRUCTION_PACKAGE_SIZE 10

struct InstructionPackageItem {
  Instruction opcode;
  uint8_t slaveAddress;
  uint8_t partIndex;
  bool completed;
};

class InstructionPackage {
private:
  long traceId;
  InstructionPackageItem items[MAX_INSTRUCTION_PACKAGE_SIZE];
  int itemCount;

public:
  InstructionPackage() : traceId(0), itemCount(0) {}
  InstructionPackage(long packageTraceId) : traceId(packageTraceId), itemCount(0) {}

  void add(Instruction opcode, uint8_t slaveAddress, uint8_t partIndex) {
    if(traceId==0) {
      Serial.println("SPECIFY TRACEID BEFORE ADDING ITEMS!!!");
      return;
    }
    if(itemCount < MAX_INSTRUCTION_PACKAGE_SIZE) {
      InstructionPackageItem item;
      item.opcode = opcode;
      item.slaveAddress = slaveAddress;
      item.partIndex = partIndex;
      item.completed = false;
      items[itemCount++] = item;
    }
  }

  long getTraceId() const { return traceId; }
  int getItemCount() const { return itemCount; }
  const InstructionPackageItem& getItem(int index) const { return items[index]; }

  void clear() {
    for(int i=0; i<itemCount; i++) {
      items[i].opcode = Instruction::None;
      items[i].slaveAddress = 0;
      items[i].partIndex = 0;
      items[i].completed = false;
    }
    itemCount = 0;
    traceId = 0;
  }

  bool isComplete() const {
    for (int i = 0; i < itemCount; i++) {
      if (!items[i].completed) {
        return false;
      }
    }
    return true;
  }

  void markCompleted(uint8_t slaveAddress, Instruction opcode, uint8_t partIndex) {
    for (int i = 0; i < itemCount; i++) {
      if (items[i].slaveAddress == slaveAddress && items[i].opcode == opcode && items[i].partIndex == partIndex) {
        items[i].completed = true;
      }
    }
  }  
};

void printInstruction(Instruction instruction) {
  char s[100];
  sprintf(s, "instruction: %d", instruction);
  Serial.println(s);
}

void printInstructionPayload(InstructionPayload payload) {
  char s[100];
  sprintf(s, " => part-index: %d  data-size: %d", payload.partIndex, payload.size);
  Serial.print(s);
}

void printInstructionPackage(const InstructionPackage& package) {
  char s[100];
  sprintf(s, "InstructionPackage trace-id: %ld", package.getTraceId());
  Serial.println(s);

  for (int i = 0; i < package.getItemCount(); i++) {
    const InstructionPackageItem& item = package.getItem(i);
    sprintf(s, "opcode: %d, Slave Address: %d, Part Index: %d, Completed: %s",
            item.opcode, item.slaveAddress, item.partIndex, item.completed ? "Yes" : "No");
    Serial.println(s);
  }
}



#endif