#include "apex/ui/Abyss.h"
#include "apex/ui/Theme.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <thread>

namespace apex::ui::abyss
{

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;

    //==========================================================================
    // Noise
    inline std::uint32_t hash (int x, int y, std::uint32_t s) noexcept
    {
        std::uint32_t h = (std::uint32_t) x * 0x8da6b343u ^ (std::uint32_t) y * 0xd8163841u ^ (s + 1u) * 0xcb1ab31fu;
        h ^= h >> 15; h *= 0x2c1b3c6du;
        h ^= h >> 12; h *= 0x297a2d39u;
        h ^= h >> 15;
        return h;
    }

    inline float rnd (int x, int y, std::uint32_t s) noexcept { return (float) (hash (x, y, s) >> 8) * (1.0f / 16777216.0f); }
    inline float smooth (float t) noexcept { return t * t * (3.0f - 2.0f * t); }
    inline float smoothstep (float a, float b, float x) noexcept { return smooth (juce::jlimit (0.0f, 1.0f, (x - a) / (b - a))); }

    float valueNoise (float x, float y, std::uint32_t s) noexcept
    {
        const float fx = std::floor (x), fy = std::floor (y);
        const int ix = (int) fx, iy = (int) fy;
        const float tx = smooth (x - fx), ty = smooth (y - fy);
        const float a = rnd (ix, iy, s), b = rnd (ix + 1, iy, s), c = rnd (ix, iy + 1, s), d = rnd (ix + 1, iy + 1, s);
        return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty;
    }

    float fbm (float x, float y, std::uint32_t s, int octaves) noexcept
    {
        float sum = 0.0f, amp = 0.5f, norm = 0.0f, f = 1.0f;
        for (int i = 0; i < octaves; ++i)
        {
            sum += amp * valueNoise (x * f, y * f, s + (std::uint32_t) i * 17u);
            norm += amp;
            amp *= 0.5f;
            f *= 2.03f;
        }
        return sum / norm;
    }

    /** Distances to the nearest and second-nearest feature point. */
    void voronoi (float x, float y, std::uint32_t s, float& f1, float& f2) noexcept
    {
        const int ix = (int) std::floor (x), iy = (int) std::floor (y);
        float d1 = 9.0f, d2 = 9.0f;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int cx = ix + dx, cy = iy + dy;
                const float px = (float) cx + 0.1f + 0.8f * rnd (cx, cy, s);
                const float py = (float) cy + 0.1f + 0.8f * rnd (cx, cy, s + 7u);
                const float d = (px - x) * (px - x) + (py - y) * (py - y);
                if (d < d1) { d2 = d1; d1 = d; }
                else if (d < d2) d2 = d;
            }
        f1 = std::sqrt (d1);
        f2 = std::sqrt (d2);
    }

    struct Rgb { float r, g, b; };

    Rgb ramp (float t) noexcept
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        // deep red -> orange -> amber -> white-hot
        const Rgb stops[] = { { 0.22f, 0.02f, 0.0f }, { 0.78f, 0.13f, 0.02f }, { 1.0f, 0.38f, 0.07f },
                              { 1.0f, 0.62f, 0.25f }, { 1.0f, 0.90f, 0.70f } };
        const float p = t * 4.0f;
        const int i = juce::jmin (3, (int) p);
        const float f = p - (float) i;
        return { stops[i].r + (stops[i + 1].r - stops[i].r) * f,
                 stops[i].g + (stops[i + 1].g - stops[i].g) * f,
                 stops[i].b + (stops[i + 1].b - stops[i].b) * f };
    }

    template <typename Fn>
    void parallelRows (int rows, Fn&& fn)
    {
        const int threads = juce::jlimit (1, 16, (int) std::thread::hardware_concurrency());
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t)
            pool.emplace_back ([&, t]
            {
                for (int y = t; y < rows; y += threads)
                    fn (y);
            });
        for (auto& th : pool)
            th.join();
    }

    /** Three box passes approximate a gaussian. Operates in place on a W x H plane. */
    void blurPlane (std::vector<float>& plane, int w, int h, float radius)
    {
        const int r = juce::jmax (1, (int) std::lround (radius / 1.7f));
        std::vector<float> tmp ((size_t) w * (size_t) h);
        auto horizontal = [&] (const std::vector<float>& src, std::vector<float>& dst)
        {
            parallelRows (h, [&] (int y)
            {
                const float* s = src.data() + (size_t) y * (size_t) w;
                float* d = dst.data() + (size_t) y * (size_t) w;
                float acc = 0.0f;
                for (int x = -r; x <= r; ++x) acc += s[juce::jlimit (0, w - 1, x)];
                const float inv = 1.0f / (float) (2 * r + 1);
                for (int x = 0; x < w; ++x)
                {
                    d[x] = acc * inv;
                    acc += s[juce::jmin (w - 1, x + r + 1)] - s[juce::jmax (0, x - r)];
                }
            });
        };
        auto vertical = [&] (const std::vector<float>& src, std::vector<float>& dst)
        {
            parallelRows (w, [&] (int x)
            {
                float acc = 0.0f;
                for (int y = -r; y <= r; ++y) acc += src[(size_t) juce::jlimit (0, h - 1, y) * (size_t) w + (size_t) x];
                const float inv = 1.0f / (float) (2 * r + 1);
                for (int y = 0; y < h; ++y)
                {
                    dst[(size_t) y * (size_t) w + (size_t) x] = acc * inv;
                    acc += src[(size_t) juce::jmin (h - 1, y + r + 1) * (size_t) w + (size_t) x]
                         - src[(size_t) juce::jmax (0, y - r) * (size_t) w + (size_t) x];
                }
            });
        };
        for (int pass = 0; pass < 3; ++pass)
        {
            horizontal (plane, tmp);
            vertical (tmp, plane);
        }
    }

    juce::Colour toColour (Rgb c, float alpha = 1.0f)
    {
        return juce::Colour::fromFloatRGBA (c.r, c.g, c.b, alpha);
    }
}

//==============================================================================
namespace detail
{
    struct Cache : private juce::DeletedAtShutdown
    {
        ~Cache() override { clearSingletonInstance(); }
        std::map<juce::String, juce::Image> images;
        JUCE_DECLARE_SINGLETON_SINGLETHREADED_INLINE (Cache, false)
    };

    juce::Image& cachedImage (const juce::String& key)
    {
        auto& images = Cache::getInstance()->images;
        if (images.size() > 64)
            images.clear();   // bounded; everything is regenerated on demand
        return images[key];
    }
}

//==============================================================================
namespace fonts
{
    juce::Font title (float h)                  { return Fonts::get (Fonts::Face::serifDecorative, h, 0.02f); }
    juce::Font serif (float h, float k)         { return Fonts::get (Fonts::Face::serifBold, h, k); }
    juce::Font serifLight (float h, float k)    { return Fonts::get (Fonts::Face::serifSemiBold, h, k); }
    juce::Font label (float h, float k)         { return Fonts::get (Fonts::Face::labelSemiBold, h, k); }
    juce::Font labelBold (float h, float k)     { return Fonts::get (Fonts::Face::labelBold, h, k); }
    juce::Font value (float h)                  { return Fonts::get (Fonts::Face::labelMedium, h, 0.04f); }
}

juce::Colour emberRamp (float intensity)
{
    return toColour (ramp (intensity));
}

void glowStroke (juce::Graphics& g, const juce::Path& p, float width, juce::Colour core, float intensity,
                 juce::PathStrokeType::JointStyle joint)
{
    const juce::Graphics::ScopedSaveState saved (g);
    intensity = juce::jlimit (0.0f, 1.5f, intensity);
    const auto halo = core.withMultipliedSaturation (1.1f);
    const float widths[] = { 9.0f, 5.5f, 3.2f, 1.9f };
    const float alphas[] = { 0.05f, 0.09f, 0.16f, 0.28f };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (halo.withAlpha (juce::jmin (1.0f, alphas[i] * intensity)));
        g.strokePath (p, juce::PathStrokeType (width * widths[i], joint, juce::PathStrokeType::rounded));
    }
    g.setColour (core.withAlpha (juce::jmin (1.0f, 0.55f + 0.45f * intensity)));
    g.strokePath (p, juce::PathStrokeType (width, joint, juce::PathStrokeType::rounded));
    if (intensity > 0.35f)
    {
        g.setColour (core.brighter (0.9f).withAlpha (juce::jmin (1.0f, (intensity - 0.35f) * 0.9f)));
        g.strokePath (p, juce::PathStrokeType (width * 0.45f, joint, juce::PathStrokeType::rounded));
    }
}

void glowText (juce::Graphics& g, const juce::String& text, const juce::Font& font, juce::Rectangle<float> area,
               juce::Justification j, juce::Colour core, float intensity)
{
    const juce::Graphics::ScopedSaveState saved (g);
    g.setFont (font);
    if (intensity > 0.02f)
    {
        const float r = font.getHeight() * 0.09f;
        for (int ring = 2; ring >= 1; --ring)
        {
            g.setColour (core.withAlpha (juce::jmin (1.0f, 0.10f * intensity / (float) ring)));
            for (int k = 0; k < 8; ++k)
            {
                const float a = (float) k * pi * 0.25f;
                g.drawText (text, area.translated (std::cos (a) * r * (float) ring, std::sin (a) * r * (float) ring), j, false);
            }
        }
    }
    g.setColour (core);
    g.drawText (text, area, j, false);
}

void glowSpot (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour colour, float alpha)
{
    if (alpha <= 0.0f || radius <= 0.0f)
        return;
    const juce::Graphics::ScopedSaveState saved (g);
    juce::ColourGradient grad (colour.withAlpha (juce::jmin (1.0f, alpha)), c.x, c.y, colour.withAlpha (0.0f), c.x + radius, c.y, true);
    grad.addColour (0.25, colour.withAlpha (juce::jmin (1.0f, alpha * 0.55f)));
    grad.addColour (0.6, colour.withAlpha (juce::jmin (1.0f, alpha * 0.15f)));
    g.setGradientFill (grad);
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c));
}

//==============================================================================
juce::Path iconPath (Icon icon, juce::Rectangle<float> area)
{
    juce::Path p;
    auto line = [&p] (float x0, float y0, float x1, float y1) { p.startNewSubPath (x0, y0); p.lineTo (x1, y1); };
    auto chevronUp = [&] (float cx, float y, float w, float h) { p.startNewSubPath (cx - w, y + h); p.lineTo (cx, y); p.lineTo (cx + w, y + h); };

    switch (icon)
    {
        case Icon::drop:
            line (0.5f, 0.05f, 0.5f, 0.78f);
            p.startNewSubPath (0.28f, 0.56f); p.lineTo (0.5f, 0.8f); p.lineTo (0.72f, 0.56f);
            p.startNewSubPath (0.28f, 0.34f); p.lineTo (0.5f, 0.58f); p.lineTo (0.72f, 0.34f);
            line (0.2f, 0.95f, 0.8f, 0.95f);
            break;
        case Icon::gate:
            line (0.36f, 0.12f, 0.36f, 0.84f); line (0.64f, 0.12f, 0.64f, 0.84f);
            chevronUp (0.36f, 0.12f, 0.12f, 0.14f); chevronUp (0.64f, 0.12f, 0.12f, 0.14f);
            line (0.2f, 0.84f, 0.8f, 0.84f);
            break;
        case Icon::boost:
            chevronUp (0.5f, 0.12f, 0.3f, 0.24f); chevronUp (0.5f, 0.38f, 0.3f, 0.24f); chevronUp (0.5f, 0.64f, 0.3f, 0.24f);
            break;
        case Icon::amp: case Icon::trident:
            line (0.5f, 0.08f, 0.5f, 0.98f);
            p.startNewSubPath (0.18f, 0.12f);
            p.lineTo (0.18f, 0.42f);
            p.quadraticTo (0.2f, 0.6f, 0.5f, 0.62f);
            p.quadraticTo (0.8f, 0.6f, 0.82f, 0.42f);
            p.lineTo (0.82f, 0.12f);
            line (0.18f, 0.12f, 0.1f, 0.24f); line (0.82f, 0.12f, 0.9f, 0.24f);
            line (0.5f, 0.08f, 0.42f, 0.2f); line (0.5f, 0.08f, 0.58f, 0.2f);
            line (0.38f, 0.8f, 0.62f, 0.8f);
            break;
        case Icon::shape:
            p.startNewSubPath (0.04f, 0.62f); p.lineTo (0.22f, 0.62f); p.lineTo (0.3f, 0.1f); p.lineTo (0.38f, 0.86f);
            p.quadraticTo (0.46f, 0.5f, 0.56f, 0.66f); p.quadraticTo (0.66f, 0.56f, 0.74f, 0.63f); p.lineTo (0.96f, 0.62f);
            break;
        case Icon::cab:
            p.addRoundedRectangle (0.12f, 0.12f, 0.76f, 0.76f, 0.08f);
            p.addEllipse (0.26f, 0.26f, 0.48f, 0.48f);
            p.addEllipse (0.42f, 0.42f, 0.16f, 0.16f);
            break;
        case Icon::fx: case Icon::spiral: case Icon::abyss:
        {
            const int n = 90;
            for (int i = 0; i <= n; ++i)
            {
                const float t = (float) i / (float) n;
                const float a = t * pi * 5.2f, r = 0.04f + 0.42f * t;
                const float x = 0.5f + std::cos (a) * r, y = 0.5f + std::sin (a) * r;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            break;
        }
        case Icon::eq:
        {
            p.startNewSubPath (0.02f, 0.5f);
            for (int i = 1; i <= 60; ++i)
            {
                const float t = (float) i / 60.0f;
                const float env = std::exp (-std::pow ((t - 0.5f) * 4.0f, 2.0f));
                p.lineTo (0.02f + 0.96f * t, 0.5f - 0.4f * env * std::sin (t * pi * 9.0f));
            }
            break;
        }
        case Icon::link:
            p.addRoundedRectangle (0.06f, 0.36f, 0.5f, 0.28f, 0.14f);
            p.addRoundedRectangle (0.44f, 0.36f, 0.5f, 0.28f, 0.14f);
            p.applyTransform (juce::AffineTransform::rotation (-pi * 0.25f, 0.5f, 0.5f));
            break;
        case Icon::sun:
            p.addEllipse (0.3f, 0.3f, 0.4f, 0.4f);
            for (int i = 0; i < 12; ++i)
            {
                const float a = (float) i * pi / 6.0f, r0 = 0.3f, r1 = i % 2 == 0 ? 0.48f : 0.4f;
                line (0.5f + std::cos (a) * r0, 0.5f + std::sin (a) * r0, 0.5f + std::cos (a) * r1, 0.5f + std::sin (a) * r1);
            }
            break;
        case Icon::check:
            p.addEllipse (0.06f, 0.06f, 0.88f, 0.88f);
            p.startNewSubPath (0.28f, 0.52f); p.lineTo (0.44f, 0.68f); p.lineTo (0.74f, 0.34f);
            break;
        case Icon::warning:
            p.startNewSubPath (0.5f, 0.08f); p.lineTo (0.95f, 0.88f); p.lineTo (0.05f, 0.88f); p.closeSubPath();
            line (0.5f, 0.36f, 0.5f, 0.62f); line (0.5f, 0.74f, 0.5f, 0.76f);
            break;
        case Icon::target:
            p.addEllipse (0.12f, 0.12f, 0.76f, 0.76f);
            p.addEllipse (0.36f, 0.36f, 0.28f, 0.28f);
            line (0.5f, 0.0f, 0.5f, 0.3f); line (0.5f, 0.7f, 0.5f, 1.0f); line (0.0f, 0.5f, 0.3f, 0.5f); line (0.7f, 0.5f, 1.0f, 0.5f);
            break;
        case Icon::previous:
            p.startNewSubPath (0.7f, 0.15f); p.lineTo (0.3f, 0.5f); p.lineTo (0.7f, 0.85f); p.closeSubPath();
            break;
        case Icon::next:
            p.startNewSubPath (0.3f, 0.15f); p.lineTo (0.7f, 0.5f); p.lineTo (0.3f, 0.85f); p.closeSubPath();
            break;
        case Icon::save:
            p.startNewSubPath (0.12f, 0.1f); p.lineTo (0.72f, 0.1f); p.lineTo (0.9f, 0.28f); p.lineTo (0.9f, 0.9f);
            p.lineTo (0.12f, 0.9f); p.closeSubPath();
            p.addRectangle (0.28f, 0.1f, 0.4f, 0.24f);
            p.addRectangle (0.26f, 0.56f, 0.48f, 0.34f);
            break;
        case Icon::undo: case Icon::redo:
        {
            p.addCentredArc (0.5f, 0.52f, 0.34f, 0.34f, 0.0f, -pi * 0.7f, pi * 0.75f, true);
            const float a = -pi * 0.7f;
            const float x = 0.5f + std::sin (a) * 0.34f, y = 0.52f - std::cos (a) * 0.34f;
            p.startNewSubPath (x - 0.02f, y - 0.2f); p.lineTo (x, y); p.lineTo (x + 0.2f, y + 0.02f);
            if (icon == Icon::redo)
                p.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f, 0.5f, 0.5f));
            break;
        }
        case Icon::tuner:
            p.startNewSubPath (0.3f, 0.06f); p.lineTo (0.3f, 0.5f); p.quadraticTo (0.3f, 0.66f, 0.5f, 0.66f);
            p.quadraticTo (0.7f, 0.66f, 0.7f, 0.5f); p.lineTo (0.7f, 0.06f);
            line (0.5f, 0.66f, 0.5f, 0.96f);
            break;
        case Icon::settings:
        {
            const int teeth = 8;
            for (int i = 0; i <= teeth * 4; ++i)
            {
                const float a = (float) i / (float) (teeth * 4) * 2.0f * pi;
                const float r = (i % 4 == 1 || i % 4 == 2) ? 0.46f : 0.34f;
                const float x = 0.5f + std::cos (a) * r, y = 0.5f + std::sin (a) * r;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            p.addEllipse (0.36f, 0.36f, 0.28f, 0.28f);
            break;
        }
        case Icon::load:
            line (0.5f, 0.08f, 0.5f, 0.62f);
            p.startNewSubPath (0.3f, 0.42f); p.lineTo (0.5f, 0.62f); p.lineTo (0.7f, 0.42f);
            p.startNewSubPath (0.12f, 0.62f); p.lineTo (0.12f, 0.9f); p.lineTo (0.88f, 0.9f); p.lineTo (0.88f, 0.62f);
            break;
        case Icon::echo:
            for (int i = 0; i < 3; ++i)
                p.addCentredArc (0.2f, 0.5f, 0.2f + 0.24f * (float) i, 0.2f + 0.24f * (float) i, 0.0f, pi * 0.3f, pi * 0.7f, true);
            break;
    }

    const float s = juce::jmin (area.getWidth(), area.getHeight());
    p.applyTransform (juce::AffineTransform::scale (s).translated (area.getCentreX() - s * 0.5f, area.getCentreY() - s * 0.5f));
    return p;
}

void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour, float glow)
{
    const auto p = iconPath (icon, area);
    const float w = juce::jmax (1.0f, juce::jmin (area.getWidth(), area.getHeight()) / 15.0f);
    if (glow > 0.02f)
        glowStroke (g, p, w, colour, glow);
    else
    {
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

juce::Path runePath (int seed, juce::Rectangle<float> area)
{
    juce::Path p;
    const auto u = (std::uint32_t) seed;
    auto r = [u] (int k) { return rnd (k, 3, u); };
    // a stem, then two or three strokes from a small vocabulary of rune parts
    p.startNewSubPath (0.5f, 0.0f);
    p.lineTo (0.5f, 1.0f);
    const int strokes = 2 + (int) (r (0) * 2.0f);
    for (int i = 0; i < strokes; ++i)
    {
        const int kind = (int) (r (i + 1) * 7.0f);
        const float y = 0.15f + 0.6f * r (i + 10);
        switch (kind)
        {
            case 0: p.startNewSubPath (0.5f, y); p.lineTo (0.9f, y + 0.25f); break;
            case 1: p.startNewSubPath (0.5f, y); p.lineTo (0.1f, y + 0.25f); break;
            case 2: p.startNewSubPath (0.5f, y + 0.25f); p.lineTo (0.9f, y); break;
            case 3: p.startNewSubPath (0.15f, y); p.lineTo (0.85f, y); break;
            case 4: p.startNewSubPath (0.5f, y); p.lineTo (0.85f, y + 0.18f); p.lineTo (0.5f, y + 0.36f); break;
            case 5: p.startNewSubPath (0.1f, y); p.lineTo (0.5f, y + 0.22f); p.lineTo (0.9f, y); break;
            default: p.startNewSubPath (0.5f, y); p.lineTo (0.15f, y - 0.15f); break;
        }
    }
    p.applyTransform (juce::AffineTransform::scale (area.getWidth(), area.getHeight()).translated (area.getX(), area.getY()));
    return p;
}

void sigil (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, float alpha, int seed)
{
    const juce::Graphics::ScopedSaveState saved (g);
    g.setColour (colour.withAlpha (alpha));
    const float w = juce::jmax (0.6f, r * 0.008f);
    for (float k : { 1.0f, 0.93f, 0.6f, 0.2f })
        g.drawEllipse (juce::Rectangle<float> (2.0f * r * k, 2.0f * r * k).withCentre (c), w);

    // seven-point star {7/3}
    juce::Path star;
    for (int i = 0; i <= 7; ++i)
    {
        const float a = -pi * 0.5f + (float) (i * 3 % 7) * 2.0f * pi / 7.0f;
        const auto pt = c + juce::Point<float> (std::cos (a), std::sin (a)) * (r * 0.93f);
        if (i == 0) star.startNewSubPath (pt); else star.lineTo (pt);
    }
    g.strokePath (star, juce::PathStrokeType (w));
    g.drawLine (c.x - r, c.y, c.x + r, c.y, w);
    g.drawLine (c.x, c.y - r * 1.15f, c.x, c.y + r * 1.15f, w);

    // runes in the outer band
    const int count = 18;
    for (int i = 0; i < count; ++i)
    {
        const float a = (float) i / (float) count * 2.0f * pi;
        const float rr = r * 0.965f, s = r * 0.045f;
        auto rune = runePath (seed * 31 + i, { -s * 0.5f, -s, s, s * 2.0f });
        rune.applyTransform (juce::AffineTransform::rotation (a + pi * 0.5f).translated (c + juce::Point<float> (std::cos (a), std::sin (a)) * rr));
        g.strokePath (rune, juce::PathStrokeType (w * 0.9f));
    }
}

//==============================================================================
juce::Path archPanel (juce::Rectangle<float> b, float archHeight, float corner)
{
    juce::Path p;
    const float l = b.getX(), r = b.getRight(), t = b.getY(), btm = b.getBottom(), cx = b.getCentreX(), w = b.getWidth();
    const float shoulder = t + archHeight;
    p.startNewSubPath (l, btm - corner);
    p.lineTo (l, shoulder);
    p.cubicTo (l + w * 0.02f, shoulder - archHeight * 0.55f, cx - w * 0.22f, t + archHeight * 0.18f, cx - w * 0.06f, t + archHeight * 0.1f);
    p.quadraticTo (cx - w * 0.015f, t + archHeight * 0.06f, cx, t);
    p.quadraticTo (cx + w * 0.015f, t + archHeight * 0.06f, cx + w * 0.06f, t + archHeight * 0.1f);
    p.cubicTo (cx + w * 0.22f, t + archHeight * 0.18f, r - w * 0.02f, shoulder - archHeight * 0.55f, r, shoulder);
    p.lineTo (r, btm - corner);
    p.quadraticTo (r, btm, r - corner, btm);
    p.lineTo (l + corner, btm);
    p.quadraticTo (l, btm, l, btm - corner);
    p.closeSubPath();
    return p;
}

juce::Path lensPlate (juce::Rectangle<float> b)
{
    juce::Path p;
    const float tip = b.getHeight() * 0.55f;
    p.startNewSubPath (b.getX(), b.getCentreY());
    p.lineTo (b.getX() + tip, b.getY());
    p.lineTo (b.getRight() - tip, b.getY());
    p.lineTo (b.getRight(), b.getCentreY());
    p.lineTo (b.getRight() - tip, b.getBottom());
    p.lineTo (b.getX() + tip, b.getBottom());
    p.closeSubPath();
    return p.createPathWithRoundedCorners (b.getHeight() * 0.12f);
}

void ornateFrame (juce::Graphics& art, juce::Graphics& glow, const juce::Path& outline, int seed, float heat, float weight)
{
    const juce::Graphics::ScopedSaveState s1 (art);
    const juce::Graphics::ScopedSaveState s2 (glow);
    const float w = weight;
    const auto useed = (std::uint32_t) seed;

    // the stone rim
    art.setColour (juce::Colours::black.withAlpha (0.55f));
    art.strokePath (outline, juce::PathStrokeType (11.0f * w));
    art.setColour (juce::Colour (0xff221c19));
    art.strokePath (outline, juce::PathStrokeType (5.5f * w));
    art.setColour (colours::rimLight.withAlpha (0.55f));
    art.strokePath (outline, juce::PathStrokeType (0.9f * w), juce::AffineTransform::translation (-0.6f * w, -0.8f * w));

    // sample the outline
    const float length = outline.getLength();
    if (length < 10.0f)
        return;
    const float step = 2.5f;
    const int n = (int) (length / step);
    std::vector<juce::Point<float>> pts ((size_t) n + 1), normals ((size_t) n + 1);
    for (int i = 0; i <= n; ++i)
        pts[(size_t) i] = outline.getPointAlongPath (juce::jmin (length, (float) i * step));
    for (int i = 0; i <= n; ++i)
    {
        const auto a = pts[(size_t) juce::jmax (0, i - 2)], b = pts[(size_t) juce::jmin (n, i + 2)];
        auto t = b - a;
        const float len = juce::jmax (0.001f, t.getDistanceFromOrigin());
        normals[(size_t) i] = { -t.y / len, t.x / len };
    }

    const juce::Colour vine (0xff3b322d), vineLight (0xff75675d);
    juce::Path vines[3];
    std::vector<float> offsets ((size_t) n + 1);
    for (int k = 0; k < 3; ++k)
    {
        for (int i = 0; i <= n; ++i)
        {
            const float sArc = (float) i * step;
            const float o = ((float) k - 1.0f) * 2.0f * w
                          + 2.8f * w * std::sin (sArc * (0.07f + 0.02f * (float) k) + (float) k * 2.1f + (float) seed)
                          + 3.2f * w * (valueNoise (sArc * 0.025f, (float) k * 3.7f, useed) - 0.5f);
            const auto pt = pts[(size_t) i] + normals[(size_t) i] * o;
            if (i == 0) vines[k].startNewSubPath (pt); else vines[k].lineTo (pt);
            if (k == 1) offsets[(size_t) i] = o;
        }
        art.setColour (vine);
        art.strokePath (vines[k], juce::PathStrokeType ((k == 1 ? 2.3f : 1.6f) * w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        art.setColour (vineLight.withAlpha (0.45f));
        art.strokePath (vines[k], juce::PathStrokeType (0.6f * w), juce::AffineTransform::translation (-0.4f * w, -0.6f * w));
    }

    // thorns and knots
    float next = 8.0f;
    int thorn = 0;
    for (int i = 2; i < n - 2; ++i)
    {
        const float sArc = (float) i * step;
        if (sArc < next)
            continue;
        next = sArc + 12.0f + 22.0f * rnd (thorn, 1, useed);
        const float side = rnd (thorn, 2, useed) > 0.5f ? 1.0f : -1.0f;
        const float len = (3.5f + 6.5f * rnd (thorn, 3, useed)) * w;
        const float lean = (rnd (thorn, 4, useed) - 0.5f) * 1.2f;
        const auto nrm = normals[(size_t) i] * side;
        const juce::Point<float> tangent (nrm.y, -nrm.x);
        const auto base = pts[(size_t) i] + normals[(size_t) i] * offsets[(size_t) i];
        const auto tip = base + (nrm + tangent * lean) * len;
        juce::Path t;
        t.startNewSubPath (base - tangent * (1.4f * w));
        t.lineTo (tip);
        t.lineTo (base + tangent * (1.4f * w));
        t.closeSubPath();
        art.setColour (vine);
        art.fillPath (t);
        art.setColour (vineLight.withAlpha (0.35f));
        art.strokePath (t, juce::PathStrokeType (0.4f * w));

        if (rnd (thorn, 5, useed) > 0.82f)
        {
            const float kr = (1.6f + 1.4f * rnd (thorn, 6, useed)) * w;
            art.setColour (juce::Colour (0xff2a231f));
            art.fillEllipse (juce::Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (base));
            art.setColour (vineLight.withAlpha (0.4f));
            art.drawEllipse (juce::Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (base), 0.5f * w);
        }
        ++thorn;
    }

    // ember veins inside the roots, where the noise along the frame runs hot
    if (heat > 0.0f)
    {
        juce::Path hot;
        bool open = false;
        for (int i = 0; i <= n; ++i)
        {
            const float sArc = (float) i * step;
            const float m = smoothstep (0.62f, 0.85f, valueNoise (sArc / 60.0f, 9.1f, useed + 5u)) * heat;
            const auto pt = pts[(size_t) i] + normals[(size_t) i] * offsets[(size_t) i];
            if (m > 0.15f)
            {
                if (! open) { hot.startNewSubPath (pt); open = true; }
                else hot.lineTo (pt);
            }
            else open = false;
        }
        glow.setColour (colours::ember.withAlpha (juce::jlimit (0.0f, 1.0f, 0.9f * heat)));
        glow.strokePath (hot, juce::PathStrokeType (3.0f * w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        art.setColour (colours::emberHot.withAlpha (juce::jlimit (0.0f, 1.0f, 0.85f * heat)));
        art.strokePath (hot, juce::PathStrokeType (0.9f * w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

//==============================================================================
Backdrop::Backdrop (BackdropSpec s) : spec (std::move (s))
{
    setOpaque (true);
    setInterceptsMouseClicks (false, true);
}

void Backdrop::paint (juce::Graphics& g)
{
    // Rendered images are shared by every editor showing the same art at the
    // same resolution, so reopening an editor is instant.

    const float phys = g.getInternalContext().getPhysicalPixelScaleFactor();
    const float unitScale = (float) getWidth() / (float) juce::jmax (1, spec.designWidth);
    // quantised, so a window being resized does not re-render on every pixel
    const float wanted = juce::jlimit (0.5f, 4.0f, std::ceil (phys * unitScale * 4.0f) / 4.0f);

    if (cache.isNull() || std::abs (cacheScale - wanted) > 0.01f)
    {
        const auto key = juce::String (spec.seed) + ":" + juce::String (spec.designWidth) + "x" + juce::String (spec.designHeight)
                       + "@" + juce::String (wanted, 2);
        auto& shared = detail::cachedImage (key);
        if (shared.isValid())
        {
            cache = shared;
            cacheScale = wanted;
        }
        else if (cache.isNull())
        {
            cache = render (spec, wanted);
            cacheScale = wanted;
            shared = cache;
        }
        else if (std::abs (pendingScale - wanted) > 0.01f || ! isTimerRunning())
        {
            // mid-resize: keep drawing the old image scaled, render once it settles
            pendingScale = wanted;
            startTimer (300);
        }
    }
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (cache, getLocalBounds().toFloat());
}

void Backdrop::timerCallback()
{
    stopTimer();
    cache = {};   // the next paint renders (or finds) the image at the settled size
    repaint();
}

juce::Image Backdrop::render (const BackdropSpec& spec, float ppu)
{
    const int W = juce::jmax (1, (int) std::ceil ((float) spec.designWidth * ppu));
    const int H = juce::jmax (1, (int) std::ceil ((float) spec.designHeight * ppu));
    const auto transform = juce::AffineTransform::scale (ppu);
    const auto seed = (std::uint32_t) spec.seed;

    // 1) ornament art + glow layers, recess mask
    juce::Image art (juce::Image::ARGB, W, H, true, juce::SoftwareImageType());
    juce::Image glowLayer (juce::Image::ARGB, W, H, true, juce::SoftwareImageType());
    juce::Image mask (juce::Image::SingleChannel, W, H, true, juce::SoftwareImageType());
    {
        juce::Graphics gm (mask);
        gm.addTransform (transform);
        gm.setColour (juce::Colours::white);
        for (const auto& p : spec.recesses)
            gm.fillPath (p);
    }
    if (spec.ornaments)
    {
        juce::Graphics ga (art), gg (glowLayer);
        ga.addTransform (transform);
        gg.addTransform (transform);
        spec.ornaments (ga, gg);
    }

    // 2) stone, cracks and heat per pixel
    const size_t count = (size_t) W * (size_t) H;
    std::vector<float> base (count * 3), emberLine (count);
    {
        const juce::Image::BitmapData md (mask, juce::Image::BitmapData::readOnly);
        parallelRows (H, [&] (int y)
        {
            for (int x = 0; x < W; ++x)
            {
                const float u = ((float) x + 0.5f) / ppu, v = ((float) y + 0.5f) / ppu;
                const float recess = (float) md.getPixelPointer (x, y)[0] / 255.0f;

                const float mottle = fbm (u / 70.0f, v / 70.0f, seed, 4);
                const float grain  = fbm (u / 7.0f, v / 7.0f, seed + 3u, 2);

                // big fissures (domain-warped cells) and a fine basalt / scale texture
                const float wu = u / 26.0f + (fbm (u / 45.0f, v / 45.0f, seed + 5u, 2) - 0.5f) * 1.4f;
                const float wv = v / 26.0f + (fbm (u / 45.0f + 7.3f, v / 45.0f, seed + 6u, 2) - 0.5f) * 1.4f;
                float f1, f2;
                voronoi (wu, wv, seed + 9u, f1, f2);
                const float bigCrack = 1.0f - smoothstep (0.0f, 0.06f, f2 - f1);
                const float cellLight = 0.5f + 0.5f * smoothstep (0.0f, 0.45f, f2 - f1);   // cells domed slightly
                voronoi (u / 7.5f + 0.3f * (grain - 0.5f), v / 7.5f, seed + 13u, f1, f2);
                const float ridge = 1.0f - smoothstep (0.0f, 0.12f, f2 - f1);
                const float scaleDome = smoothstep (0.0f, 0.5f, f2 - f1);

                float L = 0.024f + 0.045f * mottle + 0.016f * (grain - 0.5f);
                L *= 0.7f + 0.3f * cellLight;
                L *= 0.82f + 0.18f * scaleDome;
                L += 0.022f * ridge * (0.5f + mottle);          // raised ridges between the scales
                L *= 1.0f - 0.75f * bigCrack;

                float heat = smoothstep (0.6f, 0.86f, fbm (u / 240.0f, v / 240.0f, seed + 11u, 3)) * 0.7f;
                for (const auto& [c, radius] : spec.hotspots)
                {
                    const float dx = (u - c.x) / radius, dy = (v - c.y) / radius;
                    heat += 0.75f * std::exp (-(dx * dx + dy * dy));
                }
                heat = juce::jmin (1.1f, heat);

                L *= 1.0f - 0.4f * recess;
                heat *= 1.0f - 0.88f * recess;

                const size_t i = (size_t) y * (size_t) W + (size_t) x;
                base[i * 3 + 0] = L * 1.2f;
                base[i * 3 + 1] = L * 0.96f;
                base[i * 3 + 2] = L * 0.84f;
                emberLine[i] = (bigCrack * (0.55f + 0.45f * grain) + 0.12f * ridge) * heat;
            }
        });
    }

    // 3) glow planes: the cracks, and the ornament glow layer (premultiplied RGB)
    std::vector<float> crackGlowNear (emberLine), crackGlowFar (emberLine);
    blurPlane (crackGlowNear, W, H, 3.0f * ppu);
    blurPlane (crackGlowFar, W, H, 12.0f * ppu);

    std::vector<float> glowPlanes[3];
    {
        const juce::Image::BitmapData gd (glowLayer, juce::Image::BitmapData::readOnly);
        for (auto& p : glowPlanes)
            p.assign (count, 0.0f);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const auto c = gd.getPixelColour (x, y);   // un-premultiplied
                const float a = c.getFloatAlpha();
                const size_t i = (size_t) y * (size_t) W + (size_t) x;
                glowPlanes[0][i] = c.getFloatRed() * a;
                glowPlanes[1][i] = c.getFloatGreen() * a;
                glowPlanes[2][i] = c.getFloatBlue() * a;
            }
    }
    std::vector<float> glowFar[3];
    for (int c = 0; c < 3; ++c)
    {
        glowFar[c] = glowPlanes[c];
        blurPlane (glowPlanes[c], W, H, 2.5f * ppu);
        blurPlane (glowFar[c], W, H, 10.0f * ppu);
    }

    // 4) composite
    juce::Image out (juce::Image::RGB, W, H, false, juce::SoftwareImageType());
    {
        const juce::Image::BitmapData ad (art, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData od (out, juce::Image::BitmapData::writeOnly);
        const Rgb crackGlowColour = ramp (0.5f);
        parallelRows (H, [&] (int y)
        {
            for (int x = 0; x < W; ++x)
            {
                const size_t i = (size_t) y * (size_t) W + (size_t) x;
                float r = base[i * 3], g = base[i * 3 + 1], b = base[i * 3 + 2];

                // ornaments over the stone (art is premultiplied ARGB)
                const auto px = ad.getPixelColour (x, y);
                const float a = px.getFloatAlpha();
                r = r * (1.0f - a) + px.getFloatRed() * a;
                g = g * (1.0f - a) + px.getFloatGreen() * a;
                b = b * (1.0f - a) + px.getFloatBlue() * a;

                // molten cracks (hidden under ornaments) and their glow
                const float e = emberLine[i] * (1.0f - a);
                const auto hot = ramp (juce::jmin (1.0f, e * 1.1f));
                r += hot.r * e; g += hot.g * e; b += hot.b * e;
                const float cg = crackGlowNear[i] * 0.6f + crackGlowFar[i] * 0.45f;
                r += crackGlowColour.r * cg; g += crackGlowColour.g * cg; b += crackGlowColour.b * cg;

                r += glowPlanes[0][i] * 0.75f + glowFar[0][i] * 0.6f;
                g += glowPlanes[1][i] * 0.75f + glowFar[1][i] * 0.6f;
                b += glowPlanes[2][i] * 0.75f + glowFar[2][i] * 0.6f;

                // vignette
                const float vx = ((float) x / (float) W - 0.5f) * 2.0f, vy = ((float) y / (float) H - 0.5f) * 2.0f;
                const float vig = 1.0f - 0.35f * juce::jmin (1.0f, (vx * vx * 0.6f + vy * vy * 0.8f));
                r *= vig; g *= vig; b *= vig;

                od.setPixelColour (x, y, juce::Colour::fromFloatRGBA (juce::jmin (1.0f, r), juce::jmin (1.0f, g), juce::jmin (1.0f, b), 1.0f));
            }
        });
    }
    return out;
}

} // namespace apex::ui::abyss
