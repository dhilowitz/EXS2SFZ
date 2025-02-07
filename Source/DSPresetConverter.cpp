/*
  ==============================================================================

    DSPresetConverter.cpp
    Created: 16 Aug 2021 9:55:21pm
    Author:  David Hilowitz

  ==============================================================================
*/

#include "DSPresetConverter.h"

void DSPresetConverter::parseDSEXS24(DSEXS24 exs24, juce::String samplePath, juce::File outputDir) {
    // Replace slashes in samplePath
    samplePath = samplePath.replace("\\", "/");
    // Remove trailing slash
    if(samplePath.endsWith("/")) {
        samplePath = samplePath.dropLastCharacters(1);
    }
    // Setup the EXS24 data
    juce::Array<DSEXS24Zone> &zones = exs24.getZones();
    juce::Array<DSEXS24Group> &groups = exs24.getGroups();
    juce::Array<DSEXS24Sample> &samples = exs24.getSamples();
    valueTree = juce::ValueTree("DecentSampler");
    
    // Add a generic UI
    addGenericUI();
    
    juce::ValueTree groupsVT = juce::ValueTree("groups");
    valueTree.appendChild(groupsVT, nullptr);
    
    for (DSEXS24Zone zone : zones) {
        if(zone.groupIndex < 0) {
            continue;
        } else if(zone.groupIndex > 100) {
            printf("This zone's group index is greater than 100. This converter may not support this file.");
            continue;
        }
        while (zone.groupIndex >= groups.size()) {
            DSEXS24Group newGroup;
            newGroup.name = "Couldn't find group index " + juce::String(zone.groupIndex);
            groups.add(newGroup);
        }
    }
    
    int highestSequenceNumber = 0;
    
    // Iterate through the groups and add their respective samples
    for(int groupIndex = -1; groupIndex < groups.size(); groupIndex++) {
        bool hasSamples = false;
        DSEXS24Group group = (groupIndex >= 0) ? groups[groupIndex] : DSEXS24Group();
        juce::ValueTree dsGroup ("group");
        if(group.name != "") {
            dsGroup.setProperty("name", group.name, nullptr);
        }
//        dsGroup.setProperty("_exsGroupIndex", groupIndex, nullptr);
        if(group.pan != 0) {
            dsGroup.setProperty("pan", group.pan, nullptr);
        }
        if(group.volume != 0) {
            dsGroup.setProperty("volume", juce::String(group.volume) + "dB", nullptr);
        }
        
        if(group.seqNumber != 0) {
            dsGroup.setProperty("seqPosition", group.seqNumber, nullptr);
            if(group.seqNumber > highestSequenceNumber) {
                highestSequenceNumber = group.seqNumber;
            }
        }
//        dsGroup.setProperty("_exsSequence", group.exsSequence, nullptr);
        
        for (DSEXS24Zone zone : zones) {
            if (zone.groupIndex == groupIndex) {
                juce::ValueTree dsSample("sample");
                
                int sampleIndex = zone.sampleIndex;
                if(sampleIndex < 0 || sampleIndex >= samples.size()) {
                    continue;
                }
                
                DSEXS24Sample &sample = samples.getReference(sampleIndex);
                if(samplePath.isNotEmpty()) {
                    dsSample.setProperty("path", samplePath + "/" + sample.fileName.replace("\\", "/"), nullptr);
                } else {
                    juce::File sampleFile = juce::File(sample.filePath).getChildFile(sample.fileName);
                    dsSample.setProperty("path", sampleFile.getRelativePathFrom(outputDir), nullptr);
                }
                    
                dsSample.setProperty("name", zone.name, nullptr);
                if(zone.pitch == false) {
                    dsSample.setProperty("pitchKeyTrack", 0, nullptr);
                }
                dsSample.setProperty("rootNote", zone.key, nullptr);
                dsSample.setProperty("loNote", zone.keyLow, nullptr);
                dsSample.setProperty("hiNote", zone.keyHigh, nullptr);
                dsSample.setProperty("loVel", zone.velocityRangeOn ? zone.loVel : 0, nullptr);
                dsSample.setProperty("hiVel", zone.velocityRangeOn ? zone.hiVel : 127, nullptr);
                
                float tuning = (float) zone.coarseTuning + (((float)zone.fineTuning)/100.0f);
                if(tuning != 0) {
                    dsSample.setProperty("tuning", tuning, nullptr);
                }
                if(zone.pan != 0) {
                    dsSample.setProperty("pan", zone.pan, nullptr);
                }
                if(zone.volume != 0) {
                    dsSample.setProperty("volume", juce::String(zone.volume) + "dB", nullptr);
                }
                
                if(zone.sampleStart != 0) {
                    dsSample.setProperty("start", zone.sampleStart, nullptr);
                }
                
                if(zone.sampleEnd != 0) {
                    dsSample.setProperty("end", zone.sampleEnd - 1, nullptr);
                }
                
                if(zone.loopEnabled) {
                    dsSample.setProperty("loopEnabled", zone.loopEnabled, nullptr);
                    dsSample.setProperty("loopStart", zone.loopStart, nullptr);
                    dsSample.setProperty("loopEnd", zone.loopEnd - 1, nullptr);
                
                    if(zone.loopCrossfade != 0) {
                        dsSample.setProperty("loopCrossfade", zone.loopCrossfade, nullptr);
                    }
                    
                    dsSample.setProperty("loopCrossfadeMode", zone.loopEqualPower ? "equal_power" : "linear", nullptr);
                }
                
                
                hasSamples = true;
                dsGroup.appendChild(dsSample, nullptr);
            }
        }
        if(hasSamples) {
            groupsVT.appendChild(dsGroup, nullptr);
        }
    }
    
    // Go back through and set seqLength as needed
    for(juce::ValueTree groupVT : groupsVT) {
        if(groupVT.hasProperty("seqPosition")) {
            groupVT.setProperty("seqLength", highestSequenceNumber, nullptr);
            groupVT.setProperty("seqMode", "round_robin", nullptr);
        }
        for(juce::ValueTree sampleVT : groupVT) {
            if(sampleVT.hasProperty("seqPosition")) {
                sampleVT.setProperty("seqLength", highestSequenceNumber, nullptr);
                sampleVT.setProperty("seqMode", "round_robin", nullptr);
            }
        }
    }
    
}

void DSPresetConverter::parseSFZValueTree(juce::ValueTree sfz) {
    valueTree = juce::ValueTree("DecentSampler");
    juce::ValueTree groupsVT = juce::ValueTree("groups");
    translateSFZRegionProperties(sfz, groupsVT, headerLevelGlobal);
    valueTree.appendChild(groupsVT, nullptr);
    
    for (auto sfzSection : sfz) {
        if(sfzSection.hasType("group")) {
            juce::ValueTree dsGroup ("group");
            translateSFZRegionProperties(sfzSection, dsGroup, headerLevelGroup);
            for (auto sfzRegion : sfzSection) {
                juce::ValueTree dsSample("sample");
                translateSFZRegionProperties(sfzRegion, dsSample, headerLevelRegion);
                dsGroup.appendChild(dsSample, nullptr);
            }
            groupsVT.appendChild(dsGroup, nullptr);
        }
    }
}

void DSPresetConverter::translateSFZRegionProperties(juce::ValueTree sfzRegion, juce::ValueTree &dsEntity, HeaderLevel level) {
    for (int i = 0; i < sfzRegion.getNumProperties(); i++) {
        juce::String key = sfzRegion.getPropertyName(i).toString();
        juce::var value =      sfzRegion.getProperty(key);
        
        if(level == headerLevelGroup && key == "group_label") {
            dsEntity.setProperty("name", value, nullptr);
        } else if(key == "amp_veltrack") {
            dsEntity.setProperty("ampVelTrack", ((float)value)/100.0f, nullptr);
        } else if(key == "ampeg_attack") {
            dsEntity.setProperty("attack", value, nullptr);
        } else if(key == "ampeg_release") {
            dsEntity.setProperty("release", value, nullptr);
        } else if(key == "ampeg_sustain") {
            dsEntity.setProperty("sustain", value, nullptr);
        } else if(key == "ampeg_decay") {
            dsEntity.setProperty("decay", value, nullptr);
        } else if(key == "group") {
            dsEntity.setProperty("tags", "voice-group-" + value.toString(), nullptr);
        } else if(key == "end") {
            dsEntity.setProperty("end", value, nullptr);
        } else if(key == "hikey") {
            dsEntity.setProperty("hiNote", value, nullptr);
        } else if(key == "hivel") {
            dsEntity.setProperty("hiVel", value, nullptr);
        } else if(key == "key") {
            dsEntity.setProperty("rootNote", value, nullptr);
            dsEntity.setProperty("loNote", value, nullptr);
            dsEntity.setProperty("hiNote", value, nullptr);
        } else if(key == "lokey") {
            dsEntity.setProperty("loNote", value, nullptr);
        } else if(key == "loop_end") {
            dsEntity.setProperty("loopEnd", value, nullptr);
        } else if(key == "loop_mode") {
            if(value == "loop_continuous") {
                dsEntity.setProperty("loopEnabled", (value == "loop_continuous") ? "true" : "false", nullptr);
            }
        } else if(key == "loop_start") {
            dsEntity.setProperty("loopStart", value, nullptr);
        } else if(key == "lovel") {
            dsEntity.setProperty("loVel", value, nullptr);
        } else if(key == "off_by") {
            dsEntity.setProperty("silencedByTags", "voice-group-" + value.toString(), nullptr);
        } else if(key == "off_mode") {
            dsEntity.setProperty("silencingMode", value, nullptr);
        } else if(key == "offset") {
            dsEntity.setProperty("start", value, nullptr);
        } else if(key == "pitch_keycenter") {
            dsEntity.setProperty("rootNote", value, nullptr);
        } else if(key == "sample") {
            dsEntity.setProperty("path", value, nullptr);
        } else if(key == "seq_position") {
            dsEntity.setProperty("seqPosition", value, nullptr);
            dsEntity.setProperty("seqMode", "round_robin", nullptr);
        } else if(key == "seq_length") {
            dsEntity.setProperty("seqLength", value, nullptr);
            dsEntity.setProperty("seqMode", "round_robin", nullptr);
        } else if(key == "sw_previous") {
            dsEntity.setProperty("previousNote", value, nullptr);
        } else if(key == "trigger") {
            dsEntity.setProperty("trigger", value, nullptr);
        } else if(key == "tune") {
            dsEntity.setProperty("tuning", ((int)value)/100.0, nullptr);
        } else if(key == "volume") {
            dsEntity.setProperty("volume", value.toString() + "dB", nullptr);
        } else {
            switch (level) {
                case headerLevelGlobal:
                    DBG("<global> opcode " + key + " not supported.");
                    break;
                case headerLevelGroup:
                    DBG("<group> opcode " + key + " not supported.");
                    break;
                case headerLevelRegion:
                default:
                    DBG("<region> opcode " + key + " not supported.");
                    break;
            }
        }
    }
}

void DSPresetConverter::addGenericUI() {
    juce::String effectsXML = "<effects> \
      <effect type=\"lowpass\" frequency=\"22000.0\"/>\
          <effect type=\"chorus\"  mix=\"0.0\" modDepth=\"0.2\" modRate=\"0.2\" />\
          <effect type=\"reverb\" wetLevel=\"0.5\"/>\
        </effects>";
    
    valueTree.appendChild(juce::ValueTree::fromXml(effectsXML), nullptr);
    
    juce::String uiXML = "<ui width=\"812\" height=\"375\" bgImage=\"Images/background.jpg\">\
        <tab name=\"main\">\
          <labeled-knob x=\"255\" y=\"80\" width=\"100\" textSize=\"16\" textColor=\"AA000000\"\
                        trackForegroundColor=\"CC000000\" trackBackgroundColor=\"66999999\"\
                        label=\"Attack\" type=\"float\" minValue=\"0.0\" maxValue=\"5\" value=\"0.2\" >\
            <binding type=\"amp\" level=\"instrument\" position=\"0\" parameter=\"ENV_ATTACK\" />\
          </labeled-knob>\
          <labeled-knob x=\"330\" y=\"80\" width=\"100\" textSize=\"16\" textColor=\"AA000000\"\
                        trackForegroundColor=\"CC000000\" trackBackgroundColor=\"66999999\"\
                        label=\"Decay\" type=\"float\" minValue=\"0.0\" maxValue=\"5\" value=\"1\" >\
            <binding type=\"amp\" level=\"instrument\" position=\"0\" parameter=\"ENV_DECAY\" />\
          </labeled-knob>\
          <labeled-knob x=\"405\" y=\"80\" width=\"100\" textSize=\"16\" textColor=\"AA000000\"\
                        trackForegroundColor=\"CC000000\" trackBackgroundColor=\"66999999\"\
                        label=\"Sustain\" type=\"float\" minValue=\"0.0\" maxValue=\"1\" value=\"1\" >\
            <binding type=\"amp\" level=\"instrument\" position=\"0\" parameter=\"ENV_SUSTAIN\" />\
          </labeled-knob>\
          <labeled-knob x=\"480\" y=\"80\" width=\"100\" textSize=\"16\" textColor=\"AA000000\"\
                        trackForegroundColor=\"CC000000\" trackBackgroundColor=\"66999999\"\
                        label=\"Release\" type=\"float\" minValue=\"0.01\" maxValue=\"4.0\" value=\"0.1\" >\
            <binding type=\"amp\" level=\"instrument\" position=\"0\" parameter=\"ENV_RELEASE\" />\
          </labeled-knob>\
          <labeled-knob x=\"562\" y=\"80\" width=\"100\" textSize=\"16\" textColor=\"AA000000\"\
                        trackForegroundColor=\"CC000000\" trackBackgroundColor=\"66999999\"\
                        label=\"Chorus\" type=\"float\" minValue=\"0.0\" maxValue=\"1\" value=\"0\" >\
            <binding type=\"effect\" level=\"instrument\" position=\"1\" parameter=\"FX_MIX\" />\
          </labeled-knob>\
          <labeled-knob x=\"635\" y=\"80\" width=\"100\" textSize=\"16\" textColor=\"AA000000\"\
                        trackForegroundColor=\"CC000000\" trackBackgroundColor=\"66999999\"\
                        label=\"Tone\" type=\"float\" minValue=\"0.5\" maxValue=\"1\" value=\"1\">\
            <binding type=\"effect\" level=\"instrument\" position=\"0\" parameter=\"FX_FILTER_FREQUENCY\" translation=\"table\" translationTable=\"0,33;0.3,150;0.4,450;0.5,1100;0.7,4100;0.9,11000;1.0001,22000\"/>\
          </labeled-knob>\
          <labeled-knob x=\"710\" y=\"80\" width=\"100\" textSize=\"16\" textColor=\"AA000000\" trackForegroundColor=\"CC000000\" trackBackgroundColor=\"66999999\"\
                        label=\"Reverb\" type=\"percent\" minValue=\"0\" maxValue=\"100\"\
                        textColor=\"FF000000\" value=\"50\">\
            <binding type=\"effect\" level=\"instrument\" position=\"2\"\
                     parameter=\"FX_REVERB_WET_LEVEL\" translation=\"linear\"\
                     translationOutputMin=\"0\" translationOutputMax=\"1\" />\
          </labeled-knob>\
        </tab>\
      </ui>";
    
    valueTree.appendChild(juce::ValueTree::fromXml(uiXML), nullptr);
}

// Convert the current internal valueTree, which is in a basic DecentSampler format, into an SFZ file
juce::String DSPresetConverter::getSFZ() {
    // Initialize the SFZ file with a header
    juce::String sfz = "// SFZ file created with EXS2SFZ by David Hilowitz\n\n";
    
    juce::ValueTree groupsValueTree = valueTree.getChildWithName("groups");
    if(groupsValueTree == juce::ValueTree()) {
        sfz = sfz + "// Converted EXS file was empty. ";
        return sfz;
    }
    
    sfz = sfz + "\n<control>\n";

    auto parseSampleAndGroupProperties = [this](juce::String &sfzFile, juce::ValueTree &valueTree, HeaderLevel level) {
        if(level == headerLevelGroup) {
             if (valueTree.hasProperty("name")) {
                 sfzFile = sfzFile + "group_label=" + valueTree.getProperty("name").toString() + " ";
             }
        }
        if(valueTree.hasProperty("path")) {
            sfzFile = sfzFile + "sample=" + valueTree.getProperty("path").toString() + " ";
        }
        if(valueTree.hasProperty("rootNote")) {
            sfzFile = sfzFile + "pitch_keycenter=" + valueTree.getProperty("rootNote").toString() + " ";
        }
        if(valueTree.hasProperty("loNote") ) {
            sfzFile = sfzFile + "lokey=" + valueTree.getProperty("loNote").toString() + " ";
        }
        if(valueTree.hasProperty("hiNote")) {
            sfzFile = sfzFile + "hikey=" + valueTree.getProperty("hiNote").toString() + " ";
        }
        if(valueTree.hasProperty("loVel") && valueTree.getProperty("loVel").toString() != "0") {
            sfzFile = sfzFile + "lovel=" + valueTree.getProperty("loVel").toString() + " ";
        }
        if(valueTree.hasProperty("hiVel") && valueTree.getProperty("hiVel").toString() != "127") {
            sfzFile = sfzFile + "hivel=" + valueTree.getProperty("hiVel").toString() + " ";
        }
        if(valueTree.hasProperty("start") && valueTree.getProperty("start").toString() != "0") {
            sfzFile = sfzFile + "offset=" + valueTree.getProperty("start").toString() + " ";
        }
        if(valueTree.hasProperty("end")) {
            sfzFile = sfzFile + "end=" + valueTree.getProperty("end").toString() + " ";
        }
        if(valueTree.hasProperty("loopEnabled")) {
            sfzFile = sfzFile + "loop_mode=loop_continuous\n";
        }
        if(valueTree.hasProperty("loopStart")) {
            sfzFile = sfzFile + "loop_start=" + valueTree.getProperty("loopStart").toString() + " ";
        }
        if(valueTree.hasProperty("loopEnd")) {
            sfzFile = sfzFile + "loop_end=" + valueTree.getProperty("loopEnd").toString() + " ";
        }

        if(valueTree.hasProperty("ampVelTrack") && valueTree.getProperty("ampVelTrack") != "1.0") {
            float value = valueTree.getProperty("ampVelTrack");
            if(value >= 0 && value < 1) {
                sfzFile = sfzFile + "amp_veltrack=" + juce::String((int)(value*100)) + " ";
            }
        }
        if(valueTree.hasProperty("attack")) {
            sfzFile = sfzFile + "ampeg_attack=" + valueTree.getProperty("attack").toString() + " ";
        }
        if(valueTree.hasProperty("release")) {
            sfzFile = sfzFile + "ampeg_release=" + valueTree.getProperty("release").toString() + " ";
        }
        if(valueTree.hasProperty("sustain")) {
            sfzFile = sfzFile + "ampeg_sustain=" + valueTree.getProperty("sustain").toString() + " ";
        }
        if(valueTree.hasProperty("decay")) {
            sfzFile = sfzFile + "ampeg_decay=" + valueTree.getProperty("decay").toString() + " ";
        }
        if(valueTree.hasProperty("seqPosition")) {
            sfzFile = sfzFile + "seq_position=" + valueTree.getProperty("seqPosition").toString() + " ";
        }
        if(valueTree.hasProperty("seqLength")) {
            sfzFile = sfzFile + "seq_length=" + valueTree.getProperty("seqLength").toString() + " ";
        }
        if(valueTree.hasProperty("tags")) {
            if(valueTree.getProperty("tags").toString().contains("voice-group-")) {
                sfzFile = sfzFile + "group=" + valueTree.getProperty("tags").toString().replace("voice-group-", "") + " ";
            }
        }
        
        if(valueTree.hasProperty("silencedByTags")) {
            sfzFile = sfzFile + "off_by=" + valueTree.getProperty("silencedByTags").toString().replace("voice-group-", "") + " ";
        }
        if(valueTree.hasProperty("silencingMode")) {
            sfzFile = sfzFile + "off_mode=" + valueTree.getProperty("silencingMode").toString() + " ";
        }
        
        
        if(valueTree.hasProperty("previousNote")) {
            sfzFile = sfzFile + "sw_previous=" + valueTree.getProperty("previousNote").toString() + " ";
        }
        if(valueTree.hasProperty("trigger")) {
            sfzFile = sfzFile + "trigger=" + valueTree.getProperty("trigger").toString() + " ";
        }
        if(valueTree.hasProperty("tuning")) {
            sfzFile = sfzFile + "tune=" + juce::String((int)((float)valueTree.getProperty("tuning") * 100.0f)) + " ";
        }
        if(valueTree.hasProperty("volume")) {
            sfzFile = sfzFile + "volume=" + juce::String(linearOrDbStringToDb(valueTree.getProperty("volume").toString())) + " ";
        }
    };
    
    parseSampleAndGroupProperties(sfz, groupsValueTree, headerLevelGlobal);
    sfz << "\n";
    
    // Iterate through the groups and add their samples
     for (auto groupValueTree : groupsValueTree) {
         if(!groupValueTree.hasType("group")) {
             continue;
         }
         sfz << "<group>";
         parseSampleAndGroupProperties(sfz, groupValueTree, headerLevelGroup);
         sfz << "\n";
        
         for (auto sampleValueTree : groupValueTree) {
             if(!sampleValueTree.hasType(juce::Identifier("sample"))) {
                 continue;
             }
             
             sfz << "<region>";
             parseSampleAndGroupProperties(sfz, sampleValueTree, headerLevelRegion);
             sfz << "\n";
         }
     }
    return sfz;
}


double DSPresetConverter::linearOrDbStringToDb(const juce::String inputString) {
    if (inputString.contains("dB")) {
        return juce::jlimit(
             (double) -100,
             (double) 24,
             inputString.upToFirstOccurrenceOf("dB", false, true).getDoubleValue());
        
    } else {
        return juce::Decibels::gainToDecibels(inputString.getDoubleValue());
    }
}
