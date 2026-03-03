#ifndef AutomationController_h
#define AutomationController_h

#include "Common.h"
#include "Models.h"

enum SequenceState {
  NotStarted = 0,
  Started = 1,
};

struct SequenceControl {
  SequenceState state;
  int nextAutomationIndex;
  unsigned long nextAutomationTime;
};

class AutomationController {
private:
  AutomationSequence sequences[MAX_AUTOMATION_SEQUENCES_PR_PART];
  SequenceControl controls[MAX_AUTOMATION_SEQUENCES_PR_PART];
  int sequenceCount = 0;

  void (*automationCallback)(const Automation) = nullptr;


  bool AutomationSequenceHasAutomations(AutomationSequence sequence) {
    for(int i=0; i<MAX_AUTOMATIONS_PR_SEQUENCE; i++) {
      Automation automation = sequence.automations[i];
      if(automation.slaveAddress != 0) {
        return true;
      }
    }
    return false;
  }

  void stopSequence(int index) {
    controls[index].nextAutomationIndex = -1;
    controls[index].nextAutomationTime = 0;
    controls[index].state = SequenceState::NotStarted;
  }

  void initialize() {
    for(int i=0; i<MAX_AUTOMATION_SEQUENCES_PR_PART; i++) {
      controls[i].nextAutomationIndex = -1;
      controls[i].nextAutomationTime = 0;
      controls[i].state = SequenceState::NotStarted;
      for(int j=0; j<MAX_AUTOMATIONS_PR_SEQUENCE; j++) {
        sequences[i].automations[j].slaveAddress = 0;
        sequences[i].automations[j].target = 0;
        sequences[i].automations[j].value = 0;
      }
    }
  }

public:
  void onAutomation(void (*callback)(const Automation)) {
    automationCallback = callback;
  }

  void load(Part part) {
    sequenceCount = 0;
    initialize();
    for(int i=0; i<MAX_AUTOMATION_SEQUENCES_PR_PART; i++) {
      if(AutomationSequenceHasAutomations(part.automationSequences[i])) {
        Serial.print("Found automation sequence for part => ");
        Serial.println(i);
        SequenceControl control;
        control.state = SequenceState::NotStarted;
        control.nextAutomationIndex = -1;
        control.nextAutomationTime = 0;
        controls[sequenceCount] = control;
        sequences[sequenceCount++] = part.automationSequences[i];
      }
    }
  }

  void run(unsigned long now, int currentStep) {
    for(int i=0; i<sequenceCount; i++) {
      AutomationSequence sequence = sequences[i];
      if(sequence.startStep == currentStep && controls[i].state == SequenceState::NotStarted) {
        controls[i].nextAutomationIndex = 0;
        controls[i].nextAutomationTime = now;
        controls[i].state = SequenceState::Started;
      }

      if(controls[i].state == SequenceState::Started &&  now >= controls[i].nextAutomationTime) {
        Automation automation = sequence.automations[controls[i].nextAutomationIndex];
        if(automationCallback)
          automationCallback(automation);

        controls[i].nextAutomationIndex++;
        if(controls[i].nextAutomationIndex ==MAX_AUTOMATIONS_PR_SEQUENCE) {
          stopSequence(i);
          continue;
        }
        Automation nextAutomation = sequence.automations[controls[i].nextAutomationIndex];
        if(nextAutomation.slaveAddress == 0) {
          stopSequence(i);
          continue;
        }

        // otherwise we setup the next automation to run
        controls[i].nextAutomationTime = now + sequence.interval;
      }

    }
  }

};


#endif