#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "DSPCore.h"
#include "Theme.h"
#include "CommunityBrowserComponent.h"

namespace namapp {
namespace gui {

class MainComponent : public juce::AudioAppComponent, public juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;

    // Audio lifecycle
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // GUI lifecycle
    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    namapp::dsp::DSPCore mDspCore;
    Theme::ThallLookAndFeel mThallLookAndFeel;

    // Signal chain blocks
    juce::Label mTitleLabel;
    juce::Slider mGateThresholdSlider;
    juce::Label mGateLabel;

    // Extra features
    juce::Label mTunerLabel;
    juce::ToggleButton mMetronomeToggle;
    juce::Slider mTempoSlider;
    CommunityBrowserComponent mCommunityBrowser;

    // Input/Output meters (mockup)
    float mCurrentInputLevel{0.0f};
    float mCurrentOutputLevel{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace gui
} // namespace namapp
