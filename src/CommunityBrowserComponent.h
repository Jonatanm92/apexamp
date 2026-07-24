#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CloudSyncClient.h"
#include "Theme.h"

namespace namapp {
namespace gui {

class CommunityBrowserComponent : public juce::Component {
public:
    CommunityBrowserComponent() {
        mSearchBox.setTextToShowWhenEmpty("Search presets, genres (e.g., 'djent'), or artists...", juce::Colours::grey);
        mSearchBox.setColour(juce::TextEditor::backgroundColourId, Theme::BackgroundDark);
        mSearchBox.setColour(juce::TextEditor::textColourId, Theme::TextPrimary);
        addAndMakeVisible(mSearchBox);

        mSearchButton.setButtonText("Search Cloud");
        mSearchButton.setColour(juce::TextButton::buttonColourId, Theme::AccentNeonCyan);
        mSearchButton.onClick = [this] { performSearch(); };
        addAndMakeVisible(mSearchButton);
    }

    void resized() override {
        auto area = getLocalBounds().reduced(10);
        auto topRow = area.removeFromTop(30);
        mSearchBox.setBounds(topRow.removeFromLeft(getWidth() - 120));
        topRow.removeFromLeft(10);
        mSearchButton.setBounds(topRow);
        
        // ListBox bounds would go here
    }

private:
    void performSearch() {
        mCloudClient.searchCommunityPresets(mSearchBox.getText().toStdString(), [this](auto results) {
            // Update UI list (mock)
        });
    }

    juce::TextEditor mSearchBox;
    juce::TextButton mSearchButton;
    // juce::ListBox mResultsList;
    
    namapp::cloud::CloudSyncClient mCloudClient;
};

} // namespace gui
} // namespace namapp
