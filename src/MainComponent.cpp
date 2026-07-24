#include "MainComponent.h"
#include <vector>

namespace namapp {
namespace gui {

MainComponent::MainComponent() {
    juce::LookAndFeel::setDefaultLookAndFeel(&mThallLookAndFeel);

    // Setup GUI elements
    mTitleLabel.setText("NAM THALL AMP", juce::dontSendNotification);
    mTitleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    mTitleLabel.setColour(juce::Label::textColourId, Theme::AccentNeonRed);
    mTitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mTitleLabel);

    mGateLabel.setText("GATE THRESHOLD", juce::dontSendNotification);
    mGateLabel.setColour(juce::Label::textColourId, Theme::TextPrimary);
    mGateLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mGateLabel);

    mGateThresholdSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mGateThresholdSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    mGateThresholdSlider.setRange(-100.0, 0.0, 0.1);
    mGateThresholdSlider.setValue(-60.0);
    mGateThresholdSlider.onValueChange = [this]() {
        mDspCore.setNoiseGateThreshold(static_cast<float>(mGateThresholdSlider.getValue()));
    };
    addAndMakeVisible(mGateThresholdSlider);

    mTunerLabel.setText("Tuner: -- Hz (--)", juce::dontSendNotification);
    mTunerLabel.setColour(juce::Label::textColourId, Theme::MeterGreen);
    addAndMakeVisible(mTunerLabel);

    mMetronomeToggle.setButtonText("Metronome");
    mMetronomeToggle.onClick = [this] {
        mDspCore.getMetronome().setPlaying(mMetronomeToggle.getToggleState());
    };
    addAndMakeVisible(mMetronomeToggle);

    mTempoSlider.setRange(40.0, 300.0, 1.0);
    mTempoSlider.setValue(120.0);
    mTempoSlider.onValueChange = [this] {
        mDspCore.getMetronome().setTempo(static_cast<float>(mTempoSlider.getValue()));
    };
    addAndMakeVisible(mTempoSlider);

    addAndMakeVisible(mCommunityBrowser);

    setSize(800, 600);
    startTimerHz(30); // 30 FPS UI updates

    // Request audio device access
    if (juce::RuntimePermissions::isRequired(juce::RuntimePermissions::recordAudio)
        && !juce::RuntimePermissions::isGranted(juce::RuntimePermissions::recordAudio)) {
        juce::RuntimePermissions::request(juce::RuntimePermissions::recordAudio,
                                          [&](bool granted) { if (granted) setAudioChannels(2, 2); });
    } else {
        setAudioChannels(2, 2);
    }
}

MainComponent::~MainComponent() {
    stopTimer();
    shutdownAudio();
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void MainComponent::timerCallback() {
    // Update Tuner UI
    float hz = mDspCore.getTuner().getCurrentPitchHz();
    std::string note = mDspCore.getTuner().getClosestNote();
    if (hz > 0.1f) {
        mTunerLabel.setText(juce::String::formatted("Tuner: %.1f Hz (%s)", hz, note.c_str()), juce::dontSendNotification);
    }
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate) {
    mDspCore.prepareToPlay(sampleRate, samplesPerBlockExpected);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) {
    auto numSamples = bufferToFill.numSamples;
    auto* inBufferL = bufferToFill.buffer->getReadPointer(0, bufferToFill.startSample);
    auto* inBufferR = bufferToFill.buffer->getNumChannels() > 1 ? bufferToFill.buffer->getReadPointer(1, bufferToFill.startSample) : inBufferL;
    auto* outBufferL = bufferToFill.buffer->getWritePointer(0, bufferToFill.startSample);
    auto* outBufferR = bufferToFill.buffer->getNumChannels() > 1 ? bufferToFill.buffer->getWritePointer(1, bufferToFill.startSample) : outBufferL;

    // Convert stereo to mono for guitar processing
    std::vector<float> processBuffer(numSamples);
    float maxIn = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        processBuffer[i] = (inBufferL[i] + inBufferR[i]) * 0.5f;
        maxIn = std::max(maxIn, std::abs(processBuffer[i]));
    }
    mCurrentInputLevel = maxIn;

    // Run DSP
    mDspCore.processBlock(processBuffer, processBuffer);

    float maxOut = 0.0f;
    // Write back to stereo output
    for (int i = 0; i < numSamples; ++i) {
        outBufferL[i] = processBuffer[i];
        if (bufferToFill.buffer->getNumChannels() > 1) {
            outBufferR[i] = processBuffer[i];
        }
        maxOut = std::max(maxOut, std::abs(processBuffer[i]));
    }
    mCurrentOutputLevel = maxOut;
}

void MainComponent::releaseResources() {
}

void MainComponent::paint(juce::Graphics& g) {
    // Background
    g.fillAll(Theme::BackgroundDark);
    
    // Panel background
    g.setColour(Theme::BackgroundPanel);
    g.fillRoundedRectangle(20, 80, getWidth() - 40, 150, 10.0f);

    // Draw signal chain mock outline
    g.setColour(Theme::AccentNeonCyan);
    g.drawRoundedRectangle(20, 80, getWidth() - 40, 150, 10.0f, 2.0f);
}

void MainComponent::resized() {
    auto bounds = getLocalBounds();
    
    mTitleLabel.setBounds(0, 20, getWidth(), 40);

    // Signal chain blocks
    auto panelArea = juce::Rectangle<int>(20, 80, getWidth() - 40, 150);
    auto blockWidth = panelArea.getWidth() / 5;

    // Gate
    auto gateArea = panelArea.withWidth(blockWidth).translated(blockWidth, 0);
    mGateLabel.setBounds(gateArea.removeFromTop(30));
    mGateThresholdSlider.setBounds(gateArea.reduced(10));

    // Bottom section for extras
    auto bottomArea = bounds.removeFromBottom(300).reduced(20);
    
    auto leftControls = bottomArea.removeFromLeft(200);
    mTunerLabel.setBounds(leftControls.removeFromTop(40));
    mMetronomeToggle.setBounds(leftControls.removeFromTop(40));
    mTempoSlider.setBounds(leftControls.removeFromTop(40));

    // Community Browser takes remaining space
    mCommunityBrowser.setBounds(bottomArea.reduced(10));
}

} // namespace gui
} // namespace namapp
