#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <utility>

namespace apexpresets
{
/**
    A factory preset = a name plus a list of (parameterID, real value) pairs.
    Values are in each parameter's natural units (e.g. gain 0..1, master in dB,
    channel/tonestack as the choice index). They are applied by converting to the
    parameter's normalised range, so automation/host stay in sync.

    Presets are tuned as *starting points* in the spirit of the reference tones —
    they are original settings, not copies of anyone's product.
*/
struct Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;
};

inline std::vector<Preset> factory()
{
    // Each preset defines the full patch (incl. cabBlend / Punch / Loud) so
    // recalling one is fully deterministic.
    return {
        { "Init", {
            { "channel", 0 }, { "tonestack", 0 }, { "inputTrim", 0.0f },
            { "gain", 0.5f }, { "push", 0.0f }, { "tight", 0.3f }, { "superCut", 0.0f }, { "bias", 0.02f },
            { "bass", 0.5f }, { "mid", 0.5f }, { "treble", 0.5f },
            { "chug", 0.0f }, { "lowDirtDrive", 0.0f }, { "lowDirtMix", 0.0f },
            { "sag", 0.3f }, { "powerDrive", 0.3f }, { "gate", -60.0f },
            { "cabOn", 1.0f }, { "cabType", 0 }, { "cabBlend", 0.0f },
            { "outPunch", 0.0f }, { "outLoud", 0.0f }, { "master", -6.0f }
        }},
        { "Chug Machine", {
            { "channel", 1 }, { "tonestack", 3 }, { "inputTrim", 0.0f },
            { "gain", 0.8f }, { "push", 0.5f }, { "tight", 0.78f }, { "superCut", 0.55f }, { "bias", 0.02f },
            { "bass", 0.4f }, { "mid", 0.45f }, { "treble", 0.62f },
            { "chug", 0.55f }, { "lowDirtDrive", 0.45f }, { "lowDirtMix", 0.28f },
            { "sag", 0.4f }, { "powerDrive", 0.5f }, { "gate", -42.0f },
            { "cabOn", 1.0f }, { "cabType", 2 }, { "cabBlend", 0.0f },
            { "outPunch", 0.25f }, { "outLoud", 2.0f }, { "master", -6.0f }
        }},
        { "Djent Tight", {
            { "channel", 1 }, { "tonestack", 3 }, { "inputTrim", 0.0f },
            { "gain", 0.75f }, { "push", 0.45f }, { "tight", 0.88f }, { "superCut", 0.6f }, { "bias", 0.0f },
            { "bass", 0.35f }, { "mid", 0.4f }, { "treble", 0.66f },
            { "chug", 0.6f }, { "lowDirtDrive", 0.5f }, { "lowDirtMix", 0.3f },
            { "sag", 0.35f }, { "powerDrive", 0.5f }, { "gate", -38.0f },
            { "cabOn", 1.0f }, { "cabType", 2 }, { "cabBlend", 0.0f },
            { "outPunch", 0.35f }, { "outLoud", 2.0f }, { "master", -6.0f }
        }},
        { "Modern Lead", {
            { "channel", 1 }, { "tonestack", 2 }, { "inputTrim", 0.0f },
            { "gain", 0.88f }, { "push", 0.6f }, { "tight", 0.5f }, { "superCut", 0.4f }, { "bias", 0.03f },
            { "bass", 0.45f }, { "mid", 0.6f }, { "treble", 0.62f },
            { "chug", 0.3f }, { "lowDirtDrive", 0.2f }, { "lowDirtMix", 0.15f },
            { "sag", 0.55f }, { "powerDrive", 0.6f }, { "gate", -52.0f },
            { "cabOn", 1.0f }, { "cabType", 0 }, { "cabBlend", 0.0f },
            { "outPunch", 0.2f }, { "outLoud", 1.0f }, { "master", -6.0f }
        }},
        { "Lead Boost", {
            { "channel", 1 }, { "tonestack", 2 }, { "inputTrim", 0.0f },
            { "gain", 0.92f }, { "push", 0.7f }, { "tight", 0.5f }, { "superCut", 0.35f }, { "bias", 0.03f },
            { "bass", 0.45f }, { "mid", 0.68f }, { "treble", 0.6f },
            { "chug", 0.25f }, { "lowDirtDrive", 0.2f }, { "lowDirtMix", 0.12f },
            { "sag", 0.6f }, { "powerDrive", 0.65f }, { "gate", -50.0f },
            { "cabOn", 1.0f }, { "cabType", 0 }, { "cabBlend", 0.0f },
            { "outPunch", 0.25f }, { "outLoud", 4.0f }, { "master", -6.0f }
        }},
        { "Tight Rhythm (Marshall)", {
            { "channel", 0 }, { "tonestack", 0 }, { "inputTrim", 0.0f },
            { "gain", 0.62f }, { "push", 0.25f }, { "tight", 0.6f }, { "superCut", 0.0f }, { "bias", 0.02f },
            { "bass", 0.5f }, { "mid", 0.62f }, { "treble", 0.55f },
            { "chug", 0.35f }, { "lowDirtDrive", 0.0f }, { "lowDirtMix", 0.0f },
            { "sag", 0.35f }, { "powerDrive", 0.45f }, { "gate", -46.0f },
            { "cabOn", 1.0f }, { "cabType", 1 }, { "cabBlend", 0.0f },
            { "outPunch", 0.0f }, { "outLoud", 0.0f }, { "master", -6.0f }
        }},
        { "Crunch", {
            { "channel", 0 }, { "tonestack", 0 }, { "inputTrim", 0.0f },
            { "gain", 0.5f }, { "push", 0.2f }, { "tight", 0.45f }, { "superCut", 0.0f }, { "bias", 0.02f },
            { "bass", 0.5f }, { "mid", 0.6f }, { "treble", 0.58f },
            { "chug", 0.2f }, { "lowDirtDrive", 0.0f }, { "lowDirtMix", 0.0f },
            { "sag", 0.45f }, { "powerDrive", 0.45f }, { "gate", -52.0f },
            { "cabOn", 1.0f }, { "cabType", 1 }, { "cabBlend", 0.0f },
            { "outPunch", 0.0f }, { "outLoud", 0.0f }, { "master", -6.0f }
        }},
        { "Doom / Sludge", {
            { "channel", 1 }, { "tonestack", 2 }, { "inputTrim", 0.0f },
            { "gain", 0.7f }, { "push", 0.4f }, { "tight", 0.28f }, { "superCut", 0.3f }, { "bias", 0.02f },
            { "bass", 0.66f }, { "mid", 0.5f }, { "treble", 0.42f },
            { "chug", 0.2f }, { "lowDirtDrive", 0.5f }, { "lowDirtMix", 0.35f },
            { "sag", 0.6f }, { "powerDrive", 0.6f }, { "gate", -58.0f },
            { "cabOn", 1.0f }, { "cabType", 1 }, { "cabBlend", 0.0f },
            { "outPunch", 0.0f }, { "outLoud", 1.0f }, { "master", -6.0f }
        }},
        { "Clean (Fender)", {
            { "channel", 0 }, { "tonestack", 1 }, { "inputTrim", 0.0f },
            { "gain", 0.2f }, { "push", 0.0f }, { "tight", 0.25f }, { "superCut", 0.0f }, { "bias", 0.02f },
            { "bass", 0.6f }, { "mid", 0.5f }, { "treble", 0.6f },
            { "chug", 0.0f }, { "lowDirtDrive", 0.0f }, { "lowDirtMix", 0.0f },
            { "sag", 0.2f }, { "powerDrive", 0.2f }, { "gate", -68.0f },
            { "cabOn", 1.0f }, { "cabType", 3 }, { "cabBlend", 0.0f },
            { "outPunch", 0.0f }, { "outLoud", 0.0f }, { "master", -6.0f }
        }},
    };
}

inline void apply (juce::AudioProcessorValueTreeState& apvts, const Preset& preset)
{
    for (const auto& [id, value] : preset.values)
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
}
} // namespace apexpresets
