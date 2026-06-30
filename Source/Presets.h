#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

/**
 * Factory presets for ApexAmp.
 *
 * Each preset is a list of (parameterID, real-world value) pairs. Values are
 * in the parameter's natural units (dB, Hz, %, choice index, 0/1) and applied
 * via the parameter's own range, so they stay correct if ranges change.
 *
 * rigMode: 0 = Single, 1 = Blend
 * rig:     0 = Bite, 1 = Body, 2 = Edge
 * ir:      0 = Ashen, 1 = Meshuggah, 2 = PDI-09
 */
namespace ApexPresets
{
    struct Preset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };

    inline const std::vector<Preset>& all()
    {
        static const std::vector<Preset> presets = {
            { "Init / Flat", {
                { "inputGain", 0.0f }, { "outputGain", 0.0f }, { "tight", 20.0f },
                { "rigMode", 0 }, { "rig", 0 },
                { "mixBite", 1.0f }, { "mixBody", 1.0f }, { "mixEdge", 1.0f },
                { "cabMix", 100.0f }, { "ir", 0 }, { "presence", 0.0f }, { "lowCut", 80.0f },
                { "gateOn", 0 }, { "gate", -60.0f }, { "gateHold", 50.0f } } },

            { "Tight Rhythm", {
                { "inputGain", 0.0f }, { "outputGain", 0.0f }, { "tight", 110.0f },
                { "rigMode", 0 }, { "rig", 0 },
                { "cabMix", 100.0f }, { "ir", 0 }, { "presence", 2.0f }, { "lowCut", 90.0f },
                { "gateOn", 1 }, { "gate", -52.0f }, { "gateHold", 40.0f } } },

            { "Djent Chug", {
                { "inputGain", 0.0f }, { "outputGain", 0.0f }, { "tight", 140.0f },
                { "rigMode", 0 }, { "rig", 2 },
                { "cabMix", 100.0f }, { "ir", 1 }, { "presence", 1.0f }, { "lowCut", 95.0f },
                { "gateOn", 1 }, { "gate", -50.0f }, { "gateHold", 35.0f } } },

            { "Modern Lead", {
                { "inputGain", 1.0f }, { "outputGain", 1.0f }, { "tight", 40.0f },
                { "rigMode", 0 }, { "rig", 2 },
                { "cabMix", 90.0f }, { "ir", 2 }, { "presence", 4.0f }, { "lowCut", 70.0f },
                { "gateOn", 0 }, { "gate", -60.0f }, { "gateHold", 50.0f } } },

            { "Wall (Blend)", {
                { "inputGain", 0.0f }, { "outputGain", 0.0f }, { "tight", 30.0f },
                { "rigMode", 1 },
                { "mixBite", 1.0f }, { "mixBody", 0.8f }, { "mixEdge", 0.7f },
                { "cabMix", 100.0f }, { "ir", 0 }, { "presence", -1.0f }, { "lowCut", 60.0f },
                { "gateOn", 1 }, { "gate", -55.0f }, { "gateHold", 60.0f } } },

            { "Bright Cut", {
                { "inputGain", 0.0f }, { "outputGain", 0.0f }, { "tight", 90.0f },
                { "rigMode", 0 }, { "rig", 0 },
                { "cabMix", 85.0f }, { "ir", 2 }, { "presence", 6.0f }, { "lowCut", 85.0f },
                { "gateOn", 1 }, { "gate", -52.0f }, { "gateHold", 45.0f } } },

            { "Raw (No Cab)", {
                { "inputGain", 0.0f }, { "outputGain", 0.0f }, { "tight", 20.0f },
                { "rigMode", 0 }, { "rig", 0 },
                { "cabMix", 0.0f }, { "presence", 0.0f }, { "lowCut", 75.0f },
                { "gateOn", 0 }, { "gate", -60.0f }, { "gateHold", 50.0f } } },
        };
        return presets;
    }

    inline void apply (juce::AudioProcessorValueTreeState& apvts, int index)
    {
        const auto& list = all();
        if (! juce::isPositiveAndBelow (index, (int) list.size()))
            return;

        for (const auto& [id, value] : list[(size_t) index].values)
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (value));
    }
}
