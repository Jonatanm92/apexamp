#pragma once

#include "PluginProcessor.h"
#include "apex/ui/Abyss.h"
#include "apex/ui/EditorBase.h"

/**
 * ThallbyssalEditor
 * -----------------
 * The ApexAmp editor in the abyss visual language. Basalt and ember art is
 * rendered once (abyss::Backdrop); every control on top is live and bound to
 * a parameter with undo:
 *
 *   title + preset + compare / undo         meters, tuner, settings
 *   INPUT MATCH   |  module panel           |  CAB CHAMBER
 *   (Auto Input)  |  (Amp, Drop, Gate,      |  (IR, cab mix,
 *                 |   Boost, Shape, Void)   |   low / high cut)
 *   signal chain: input > drop > gate > boost > amp > shape > cab > fx > output
 *   status: amp pressure, scope, depth, output level, hot signal
 */
class ThallbyssalEditor : public apex::ui::EditorBase
{
public:
    explicit ThallbyssalEditor (ApexAmpProcessor&);
    ~ThallbyssalEditor() override;

    static constexpr int designWidth = 1200, designHeight = 900;

protected:
    void tick() override;
    float getInputPeak() override;
    float getOutputPeak() override;
    void addSettingsItems (juce::PopupMenu&) override;
    void handleSettingsItem (int) override;

private:
    enum Module { drop, gate, boost, amp, shape, cab, fx, numModules };

    struct Panel;
    struct TextField;
    struct StatusCell;
    struct MatchStatus;

    apex::ui::abyss::Knob& addKnob (juce::Component& parent, const juce::String& paramId, const juce::String& label,
                                    juce::Point<float> centre, float diameter, const juce::String& tip);
    apex::ui::abyss::GlowButton& addToggle (juce::Component& parent, const juce::String& paramId, const juce::String& text,
                                            juce::Rectangle<float> bounds, const juce::String& tip);
    void buildHeader();
    void buildInputMatch();
    void buildModules();
    void buildCabChamber();
    void buildChain();
    void buildStatus();

    void selectModule (int);
    void toggleModule (int);
    void selectRig (int position);
    void stepParam (const juce::String& paramId, int delta, int minValue, int maxValue);
    void setParam (const juce::String& paramId, float value, const juce::String& undoName = {});
    float param (const char* id) const;
    void bindEchoTime();
    void tapTempo();
    void loadRig();
    void loadIr();
    void showTargetMenu();

    ApexAmpProcessor& proc;
    apex::ui::abyss::LookAndFeel abyssLookAndFeel;
    std::unique_ptr<apex::ui::abyss::Backdrop> backdrop;

    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> buttonAttachments;

    // header
    TextField* presetName = nullptr;
    apex::ui::abyss::GlowButton *saveButton = nullptr, *aButton = nullptr, *bButton = nullptr,
                                *undoButton = nullptr, *redoButton = nullptr, *tunerButton = nullptr, *settingsButton = nullptr;
    apex::ui::abyss::VMeter *inMeter = nullptr, *outMeter = nullptr;

    // input match
    apex::ui::abyss::VMeter* diMeter = nullptr;
    apex::ui::abyss::Radar* radar = nullptr;
    MatchStatus* matchStatus = nullptr;
    TextField* targetZone = nullptr;
    apex::ui::abyss::GlowButton* calibrate = nullptr;

    // modules
    std::array<Panel*, numModules> panels {};
    std::array<apex::ui::abyss::ChainBlock*, numModules> blocks {};
    std::array<apex::ui::abyss::ChainLink*, numModules + 1> links {};
    apex::ui::abyss::Jack *inputJack = nullptr, *outputJack = nullptr;
    int selected = amp;
    std::array<apex::ui::abyss::GlowButton*, 5> rigButtons {};
    std::array<apex::ui::abyss::Knob*, 3> blendTrims {};
    apex::ui::abyss::Knob* echoTimeKnob = nullptr;
    std::unique_ptr<juce::SliderParameterAttachment> echoTimeAttachment;
    std::unique_ptr<juce::ParameterAttachment> syncWatcher;
    apex::ui::abyss::HBar* punchBar = nullptr;
    std::vector<double> taps;
    float lastCabMix = 100.0f;
    bool fxRememberEcho = false, fxRememberAbyss = true;

    // cab chamber
    apex::ui::abyss::Portal* portal = nullptr;
    TextField* irName = nullptr;
    apex::ui::abyss::LinearSlider* cabMix = nullptr;
    StatusCell *cabValues = nullptr, *lowCutCaption = nullptr, *highCutCaption = nullptr;

    // status bar
    apex::ui::abyss::Gauge *pressureGauge = nullptr, *outputGauge = nullptr;
    apex::ui::abyss::HBar *pressureBar = nullptr, *depthBar = nullptr;
    apex::ui::abyss::Scope* scope = nullptr;
    apex::ui::abyss::Portal* ring = nullptr;
    StatusCell *pressureCell = nullptr, *signalCell = nullptr, *depthCell = nullptr, *outputCell = nullptr, *hotCell = nullptr;

    // metering state
    float inDb = -100.0f, outDb = -100.0f, matchPeak = -100.0f, hotPeak = -100.0f;
    int silentFrames = 1000, hotFrames = 0, frameCounter = 0;
    juce::String panelSignature;

    std::unique_ptr<juce::FileChooser> chooser;
};
