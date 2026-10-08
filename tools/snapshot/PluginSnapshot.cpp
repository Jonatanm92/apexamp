// Renders a plugin editor headlessly to PNG for UI review.
//   <tool> out.png [scale] [--set paramId=value ...] [--prop name=value ...] [--preset index] [--tuner]
// --prop sets a property on the state tree before the editor opens (e.g. the
// selected module of an editor).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include APEX_SNAPSHOT_HEADER
#include "apex/ui/Shell.h"

static void findAndOpenTuner (juce::Component& c)
{
    if (auto* t = dynamic_cast<apex::ui::TunerOverlay*> (&c))
    {
        t->open();
        return;
    }
    for (auto* child : c.getChildren())
        findAndOpenTuner (*child);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::String out = "editor.png";
    float scale = 1.0f;
    bool tuner = false;

    auto proc = std::make_unique<APEX_SNAPSHOT_PROCESSOR>();
    proc->setPlayConfigDetails (2, 2, 48000.0, 512);
    proc->prepareToPlay (48000.0, 512);

    for (int i = 1; i < argc; ++i)
    {
        const juce::String a (argv[i]);
        if (a == "--set" && i + 1 < argc)
        {
            const juce::String kv (argv[++i]);
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc->apvts.getParameter (kv.upToFirstOccurrenceOf ("=", false, false))))
                p->setValueNotifyingHost (p->convertTo0to1 (kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
        }
        else if (a == "--prop" && i + 1 < argc)
        {
            const juce::String kv (argv[++i]);
            proc->apvts.state.setProperty (kv.upToFirstOccurrenceOf ("=", false, false),
                                           kv.fromFirstOccurrenceOf ("=", false, false).getIntValue(), nullptr);
        }
        else if (a == "--preset" && i + 1 < argc) proc->presets.loadPreset (juce::String (argv[++i]).getIntValue());
        else if (a == "--tuner") tuner = true;
        else if (i == 1) out = a;
        else scale = a.getFloatValue();
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
    if (tuner)
        findAndOpenTuner (*editor);

    // Let timers, attachments and async updates run, and feed the meters.
    for (int i = 0; i < 20; ++i)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        for (int s = 0; s < 512; ++s)
        {
            const float v = 0.3f * std::sin (0.0108f * (float) (i * 512 + s));
            buffer.setSample (0, s, v);
            buffer.setSample (1, s, v);
        }
        juce::MidiBuffer midi;
        proc->processBlock (buffer, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    }

    const auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
    juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (out));
    file.deleteFile();
    juce::FileOutputStream os (file);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", file.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    editor.reset();
    return 0;
}
