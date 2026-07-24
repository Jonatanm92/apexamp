#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "MainComponent.h"

// Simple JUCE Application for testing the NAM DSP Core
class NamAppTestClient : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override       { return "NAM App Test Client"; }
    const juce::String getApplicationVersion() override    { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String& commandLine) override {
        mainWindow.reset (new MainWindow (getApplicationName()));
    }

    void shutdown() override {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override {
        quit();
    }

private:
    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow (juce::String name) : DocumentWindow (name, juce::Colours::darkgrey, DocumentWindow::allButtons) {
            setUsingNativeTitleBar (true);
            
            auto* content = new namapp::gui::MainComponent();
            setContentOwned(content, true);

            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (NamAppTestClient)
