#ifndef SongSerializer_h
#define SongSerializer_h

#include "Models.h"

class SongSerializer {
public:
  void serialize(const Song& song, void (*lineCallback)(const String&)) {
    if(!lineCallback) return;
    for (int partIndex = 0; partIndex < PARTS; partIndex++) {
      const Part& part = song.parts[partIndex];

      // Song Programmer Command
      String line = String(partIndex) + "=" + 
                    String(part.pages) + " " + 
                    String(part.repeats) + " " + 
                    String(part.chainTo);
      lineCallback(line);

      if(part.pages == 0) continue;

      // Tempo Command
      line = String(partIndex) + ":tempo=" + String(part.clockData.bpm);
      lineCallback(line);

      // Sampler Command
      line = String(partIndex) + ":sampler=" + String(part.samplerData.bank);
      lineCallback(line);
      for (int i = 0; i < 5; i++) {
        line = String(partIndex) + ":sampler:" + String(i) + ".mix=" + String(part.samplerData.mix[i]);
        lineCallback(line);
      }

      // Drum Sequencer Commands
      for (int channelIndex = 0; channelIndex < 5; channelIndex++) {
        const DrumSequencerChannel& channel = part.drumSequencerData.channel[channelIndex];
        
        line = String(partIndex) + ":seq:" + String(channelIndex) + "=";
        for (int pageIndex = 0; pageIndex < 4; pageIndex++) {
          uint16_t steps = channel.page[pageIndex];
          if(steps == 0) {
            line += "0";
          } else {
            String pattern = String(steps, BIN);
            while(pattern.length() < 16) // pad with 0s
              pattern = "0" + pattern;
            line += pattern;
          }
          if (pageIndex < 3) {
            line += " ";
          }
        }
        lineCallback(line);

        line = String(partIndex) + ":seq:" + String(channelIndex) + ".div=" + String(channel.divider);
        lineCallback(line);

        line = String(partIndex) + ":seq:" + String(channelIndex) + ".ena=" + String(channel.enabled ? "1" : "0");
        lineCallback(line);

        line = String(partIndex) + ":seq:" + String(channelIndex) + ".last=" + String(channel.lastStep);
        lineCallback(line);
      }
    }    
  }  

};


#endif