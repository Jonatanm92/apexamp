#include "apex/ui/Theme.h"
#include "ApexUiAssets.h"

namespace apex::ui
{

namespace
{
    // Deleted with the rest of JUCE's shutdown-owned objects, before the font
    // engine goes away (a plain static would be destroyed too late on unload).
    struct TypefaceSet : private juce::DeletedAtShutdown
    {
        TypefaceSet()
        {
            using namespace ApexUiAssets;
            faces[0] = juce::Typeface::createSystemTypefaceFor (BarlowCondensedMedium_ttf,   BarlowCondensedMedium_ttfSize);
            faces[1] = juce::Typeface::createSystemTypefaceFor (BarlowCondensedSemiBold_ttf, BarlowCondensedSemiBold_ttfSize);
            faces[2] = juce::Typeface::createSystemTypefaceFor (BarlowCondensedBold_ttf,     BarlowCondensedBold_ttfSize);
            faces[3] = juce::Typeface::createSystemTypefaceFor (BigShouldersDisplayExtraBold_ttf, BigShouldersDisplayExtraBold_ttfSize);
            faces[4] = juce::Typeface::createSystemTypefaceFor (JetBrainsMonoMedium_ttf,     JetBrainsMonoMedium_ttfSize);
            faces[5] = juce::Typeface::createSystemTypefaceFor (JetBrainsMonoBold_ttf,       JetBrainsMonoBold_ttfSize);
        }

        ~TypefaceSet() override { clearSingletonInstance(); }

        juce::Typeface::Ptr faces[6];

        JUCE_DECLARE_SINGLETON_INLINE (TypefaceSet, false)
    };
}

juce::Font Fonts::get (Face face, float height, float kerning)
{
    auto* set = TypefaceSet::getInstance();
    auto options = juce::FontOptions (set->faces[(int) face]).withHeight (height);
    if (kerning != 0.0f)
        options = options.withKerningFactor (kerning);
    return juce::Font (options);
}

} // namespace apex::ui
