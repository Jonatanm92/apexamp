#include "apex/ui/Materials.h"
#include "apex/ui/Theme.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <vector>

namespace apex::ui
{

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float lightAngle = -2.35619449f;   // light from the top-left (atan2, y down)

    inline float clamp01 (float x) noexcept { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

    inline float smoothstep (float e0, float e1, float x) noexcept
    {
        const float t = clamp01 ((x - e0) / (e1 - e0));
        return t * t * (3.0f - 2.0f * t);
    }

    inline float hash01 (int x, int y, int seed) noexcept
    {
        auto h = (std::uint32_t) x * 374761393u + (std::uint32_t) y * 668265263u + (std::uint32_t) seed * 2147483647u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return (float) (h & 0xffffff) / (float) 0x1000000;
    }

    struct Rgba { float r = 0, g = 0, b = 0, a = 0; };

    inline Rgba over (const Rgba& top, const Rgba& bottom) noexcept
    {
        const float a = top.a + bottom.a * (1.0f - top.a);
        if (a <= 1.0e-6f)
            return {};
        const float kb = bottom.a * (1.0f - top.a);
        return { (top.r * top.a + bottom.r * kb) / a,
                 (top.g * top.a + bottom.g * kb) / a,
                 (top.b * top.a + bottom.b * kb) / a, a };
    }

    inline Rgba grey (float i, float a, float tr = 1.0f, float tg = 1.0f, float tb = 1.0f) noexcept
    {
        return { clamp01 (i * tr), clamp01 (i * tg), clamp01 (i * tb), clamp01 (a) };
    }

    template <typename Shader>
    juce::Image render (int w, int h, Shader&& shader)
    {
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const Rgba c = shader ((float) x + 0.5f, (float) y + 0.5f);
                if (c.a > 0.0f)
                    bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (c.r, c.g, c.b, c.a));
            }
        return img;
    }

    // ---- tileable noise helpers -------------------------------------------------
    std::vector<float> whiteNoise (int w, int h, int seed)
    {
        std::vector<float> n ((size_t) (w * h));
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                n[(size_t) (y * w + x)] = hash01 (x, y, seed) * 2.0f - 1.0f;
        return n;
    }

    void boxBlur (std::vector<float>& v, int w, int h, int rx, int ry)
    {
        std::vector<float> tmp (v.size());
        if (rx > 0)
        {
            const float norm = 1.0f / (float) (2 * rx + 1);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    float acc = 0.0f;
                    for (int k = -rx; k <= rx; ++k)
                        acc += v[(size_t) (y * w + ((x + k + w) % w))];
                    tmp[(size_t) (y * w + x)] = acc * norm;
                }
            v.swap (tmp);
        }
        if (ry > 0)
        {
            const float norm = 1.0f / (float) (2 * ry + 1);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    float acc = 0.0f;
                    for (int k = -ry; k <= ry; ++k)
                        acc += v[(size_t) (((y + k + h) % h) * w + x)];
                    tmp[(size_t) (y * w + x)] = acc * norm;
                }
            v.swap (tmp);
        }
    }

    void normalise (std::vector<float>& v)
    {
        double mean = 0.0, sq = 0.0;
        for (float x : v) mean += x;
        mean /= (double) v.size();
        for (float x : v) sq += (x - mean) * (x - mean);
        const auto sd = (float) std::sqrt (sq / (double) v.size()) + 1.0e-9f;
        for (auto& x : v) x = (x - (float) mean) / sd;
    }

    // Deviation -> white (lighten) or black (darken) with alpha.
    juce::Image detailImage (const std::vector<float>& dev, int w, int h, float gain)
    {
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const float d = dev[(size_t) (y * w + x)] * gain;
                const float a = clamp01 (std::abs (d));
                bd.setPixelColour (x, y, d >= 0.0f ? juce::Colours::white.withAlpha (a)
                                                   : juce::Colours::black.withAlpha (a));
            }
        return img;
    }

    // ---- cache ------------------------------------------------------------------
    struct ImageCache : private juce::DeletedAtShutdown
    {
        ~ImageCache() override { clearSingletonInstance(); }

        juce::Image brushed, tolex, powder, grille;
        std::map<std::int64_t, juce::Image> images;

        juce::Image& slot (std::int64_t key)
        {
            if (images.size() > 96)
                images.clear();   // bounded; everything is regenerated on demand
            return images[key];
        }

        JUCE_DECLARE_SINGLETON_SINGLETHREADED_INLINE (ImageCache, false)
    };

    // ---- knob shaders ---------------------------------------------------------
    juce::Image renderSpunKnob (int d)
    {
        const int s = juce::roundToInt ((float) d * 1.5f);
        const float c = (float) s * 0.5f, R = (float) d * 0.5f, Rc = R * 0.76f;

        return render (s, s, [=] (float x, float y)
        {
            const float dx = x - c, dy = y - c;
            const float dist = std::sqrt (dx * dx + dy * dy);
            const float th = std::atan2 (dy, dx);
            const float phi = th - lightAngle;

            // soft shadow cast down-right
            const float sdx = dx - 0.06f * R, sdy = dy - 0.13f * R;
            const float sd = std::sqrt (sdx * sdx + sdy * sdy);
            Rgba out { 0, 0, 0, 0.7f * (1.0f - smoothstep (0.78f * R, 1.34f * R, sd)) };

            // knurled skirt
            if (dist < R + 1.0f)
            {
                const float aa = clamp01 (R - dist + 0.5f);
                const float lit = 0.5f + 0.5f * std::cos (phi);
                const float ridge = 0.5f + 0.5f * std::cos (60.0f * th);
                float i = 0.05f + 0.08f * lit + 0.05f * ridge * (0.3f + 0.7f * lit);
                const float rim = std::exp (-((dist - (R - 1.4f)) * (dist - (R - 1.4f))) / 0.9f);
                i += rim * 0.22f * lit * lit;
                // the cap's shadow on the lower-right of the skirt
                const float capShadow = smoothstep (Rc + 0.09f * R, Rc, dist) * (0.5f - 0.5f * std::cos (phi));
                i *= 1.0f - 0.55f * capShadow;
                out = over (grey (i, aa, 0.97f, 0.98f, 1.03f), out);
            }

            // spun aluminium cap
            if (dist < Rc + 1.0f)
            {
                const float aa = clamp01 (Rc - dist + 0.5f);
                const float rn = dist / Rc;
                float i = 0.60f + 0.21f * std::cos (2.0f * phi) + 0.05f * std::cos (4.0f * phi + 0.7f);
                i += (hash01 ((int) (dist * 1.3f), 7, 11) - 0.5f) * 0.05f;   // lathe rings
                i += (hash01 ((int) x, (int) y, 3) - 0.5f) * 0.015f;           // grain
                i -= 0.07f * rn * rn;
                const float bevel = smoothstep (0.85f, 0.97f, rn);
                const float bevelI = 0.5f + 0.42f * std::cos (phi);
                i = i * (1.0f - bevel) + bevelI * bevel;
                const float lip = smoothstep (0.965f, 1.0f, rn);
                i *= 1.0f - 0.45f * lip;
                out = over (grey (i, aa, 0.95f, 0.965f, 1.0f), out);
            }
            return out;
        });
    }

    juce::Image renderPedalKnob (int d)
    {
        const int s = juce::roundToInt ((float) d * 1.5f);
        const float c = (float) s * 0.5f, R = (float) d * 0.5f, Rc = R * 0.80f;

        return render (s, s, [=] (float x, float y)
        {
            const float dx = x - c, dy = y - c;
            const float dist = std::sqrt (dx * dx + dy * dy);
            const float th = std::atan2 (dy, dx);
            const float phi = th - lightAngle;

            const float sdx = dx - 0.05f * R, sdy = dy - 0.12f * R;
            const float sd = std::sqrt (sdx * sdx + sdy * sdy);
            Rgba out { 0, 0, 0, 0.75f * (1.0f - smoothstep (0.75f * R, 1.3f * R, sd)) };

            if (dist < R + 1.0f)
            {
                const float aa = clamp01 (R - dist + 0.5f);
                const float lit = 0.5f + 0.5f * std::cos (phi);
                const float ridge = 0.5f + 0.5f * std::cos (40.0f * th);
                float i = 0.045f + 0.06f * lit + 0.035f * ridge * lit;
                const float rim = std::exp (-((dist - (R - 1.2f)) * (dist - (R - 1.2f))) / 0.8f);
                i += rim * 0.16f * lit * lit;
                out = over (grey (i, aa), out);
            }

            if (dist < Rc + 1.0f)
            {
                const float aa = clamp01 (Rc - dist + 0.5f);
                // gentle dome: soft highlight toward the light
                const float hx = dx + 0.38f * Rc, hy = dy + 0.38f * Rc;
                const float hl = std::exp (-(hx * hx + hy * hy) / (2.0f * (0.42f * Rc) * (0.42f * Rc)));
                float i = 0.085f + 0.13f * hl;
                const float edge = smoothstep (0.86f, 1.0f, dist / Rc);
                i = i * (1.0f - edge) + (0.06f + 0.12f * (0.5f + 0.5f * std::cos (phi))) * edge;
                i += (hash01 ((int) x, (int) y, 5) - 0.5f) * 0.012f;
                out = over (grey (i, aa, 1.0f, 1.0f, 1.04f), out);
            }
            return out;
        });
    }

    juce::Image renderFootswitch (int d, bool pressed)
    {
        const int s = juce::roundToInt ((float) d * 1.3f);
        const float c = (float) s * 0.5f, Rn = (float) d * 0.5f, Rw = Rn * 0.80f, Rcap = Rn * (pressed ? 0.56f : 0.6f);

        return render (s, s, [=] (float x, float y)
        {
            const float dx = x - c, dy = y - c;
            const float dist = std::sqrt (dx * dx + dy * dy);
            const float th = std::atan2 (dy, dx);
            const float phi = th - lightAngle;

            const float sdx = dx - 0.05f * Rn, sdy = dy - 0.1f * Rn;
            const float sd = std::sqrt (sdx * sdx + sdy * sdy);
            Rgba out { 0, 0, 0, 0.6f * (1.0f - smoothstep (0.8f * Rn, 1.25f * Rn, sd)) };

            // hex nut: distance to a hexagon, shading per facet
            const float sector = juce::MathConstants<float>::twoPi / 6.0f;
            const float facet = std::floor ((th + sector * 0.5f) / sector) * sector;
            const float hexDist = dist * std::cos (th - facet);
            const float hexR = Rn * 0.95f;
            if (hexDist < hexR + 1.0f)
            {
                const float aa = clamp01 (hexR - hexDist + 0.5f);
                float i = 0.30f + 0.32f * std::cos (facet - lightAngle);
                i += (hash01 ((int) x, (int) y, 9) - 0.5f) * 0.02f;
                out = over (grey (i, aa, 0.97f, 0.98f, 1.02f), out);
            }

            // washer
            if (dist < Rw + 1.0f)
            {
                const float aa = clamp01 (Rw - dist + 0.5f);
                float i = 0.52f + 0.30f * std::cos (phi) + 0.06f * std::cos (2.0f * phi);
                i += (hash01 ((int) (dist * 2.0f), 1, 2) - 0.5f) * 0.06f;
                out = over (grey (i, aa), out);
            }

            // domed chrome cap
            if (dist < Rcap + 1.0f)
            {
                const float aa = clamp01 (Rcap - dist + 0.5f);
                const float rn = dist / Rcap;
                const float hx = dx + 0.32f * Rcap, hy = dy + 0.36f * Rcap;
                const float spec = std::exp (-(hx * hx + hy * hy) / (2.0f * (0.2f * Rcap) * (0.2f * Rcap)));
                float i = 0.22f + 0.45f * std::sqrt (std::max (0.0f, 1.0f - rn * rn)) * (0.55f + 0.45f * std::cos (phi));
                i += 0.9f * spec;
                i += 0.22f * smoothstep (0.7f, 0.92f, rn) * (0.5f - 0.5f * std::cos (phi));   // floor reflection
                i *= 1.0f - 0.5f * smoothstep (0.93f, 1.0f, rn);
                if (pressed) i *= 0.82f;
                out = over (grey (i, aa, 0.98f, 0.99f, 1.02f), out);
            }
            return out;
        });
    }

    juce::Image cached (std::int64_t key, std::function<juce::Image()> make)
    {
        auto& img = ImageCache::getInstance()->slot (key);
        if (! img.isValid())
            img = make();
        return img;
    }

    float physicalScale (juce::Graphics& g)
    {
        return juce::jmax (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
    }
}

//==============================================================================
namespace materials
{
    const juce::Image& brushedMetal()
    {
        auto* cache = ImageCache::getInstance();
        if (! cache->brushed.isValid())
        {
            const int w = 512, h = 256;
            auto n = whiteNoise (w, h, 17);
            boxBlur (n, w, h, 30, 0);
            boxBlur (n, w, h, 30, 0);
            auto rows = whiteNoise (1, h, 23);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    n[(size_t) (y * w + x)] = n[(size_t) (y * w + x)] * 1.0f + rows[(size_t) y] * 0.35f;
            normalise (n);
            cache->brushed = detailImage (n, w, h, 0.15f);
        }
        return cache->brushed;
    }

    const juce::Image& tolexGrain()
    {
        auto* cache = ImageCache::getInstance();
        if (! cache->tolex.isValid())
        {
            const int w = 256, h = 256;
            auto a = whiteNoise (w, h, 31);
            boxBlur (a, w, h, 2, 2);
            boxBlur (a, w, h, 1, 1);
            auto b = whiteNoise (w, h, 37);
            boxBlur (b, w, h, 6, 6);
            std::vector<float> height ((size_t) (w * h));
            for (size_t i = 0; i < height.size(); ++i)
                height[i] = a[i] + 1.6f * b[i];

            std::vector<float> shade ((size_t) (w * h));
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    auto hAt = [&] (int xx, int yy) { return height[(size_t) (((yy + h) % h) * w + ((xx + w) % w))]; };
                    const float gx = hAt (x + 1, y) - hAt (x - 1, y);
                    const float gy = hAt (x, y + 1) - hAt (x, y - 1);
                    shade[(size_t) (y * w + x)] = -(gx + gy);
                }
            normalise (shade);
            cache->tolex = detailImage (shade, w, h, 0.2f);
        }
        return cache->tolex;
    }

    const juce::Image& powderGrain()
    {
        auto* cache = ImageCache::getInstance();
        if (! cache->powder.isValid())
        {
            const int w = 256, h = 256;
            auto n = whiteNoise (w, h, 41);
            boxBlur (n, w, h, 1, 1);
            normalise (n);
            cache->powder = detailImage (n, w, h, 0.22f);
        }
        return cache->powder;
    }

    const juce::Image& grilleCloth()
    {
        auto* cache = ImageCache::getInstance();
        if (! cache->grille.isValid())
        {
            // Basket weave: alternating over/under threads with a little slub.
            const int w = 192, h = 192, cell = 6;
            std::vector<float> v ((size_t) (w * h));
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const int cx = x / cell, cy = y / cell;
                    const float fx = (float) (x % cell) / (float) cell, fy = (float) (y % cell) / (float) cell;
                    const bool horizontalOnTop = ((cx + cy) & 1) == 0;
                    // thread profile across its width: bright crown, dark gaps
                    const float across = horizontalOnTop ? fy : fx;
                    const float crown = std::sin (across * pi);
                    const float along = horizontalOnTop ? fx : fy;
                    float val = crown * crown * (0.75f + 0.25f * std::sin (along * pi));
                    val += (hash01 (x / 2, y / 2, 53) - 0.5f) * 0.25f;
                    if (crown < 0.25f) val -= 0.6f;       // gaps between threads
                    v[(size_t) (y * w + x)] = val;
                }
            normalise (v);
            cache->grille = detailImage (v, w, h, 0.55f);
        }
        return cache->grille;
    }

    juce::Image knob (KnobStyle style, int diameterPx)
    {
        diameterPx = juce::jlimit (8, 1024, diameterPx);
        const auto key = (std::int64_t) ((int) style + 1) * 100000 + diameterPx;
        return cached (key, [=]
        {
            return style == KnobStyle::pedal ? renderPedalKnob (diameterPx) : renderSpunKnob (diameterPx);
        });
    }

    juce::Image footswitch (int diameterPx, bool pressed)
    {
        diameterPx = juce::jlimit (8, 1024, diameterPx);
        const auto key = (std::int64_t) 900000 + diameterPx * 2 + (pressed ? 1 : 0);
        return cached (key, [=] { return renderFootswitch (diameterPx, pressed); });
    }
}

//==============================================================================
namespace draw
{
    void knob (juce::Graphics& g, juce::Rectangle<float> area, float angle, KnobStyle style)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        area = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), area.getHeight()),
                                           juce::jmin (area.getWidth(), area.getHeight()));
        const auto centre = area.getCentre();
        const float R = area.getWidth() * 0.5f;

        if (style == KnobStyle::chickenHead)
        {
            // Bakelite chicken-head: a round skirt with a long tapered pointer.
            juce::Path skirt;
            skirt.addEllipse (-R * 0.58f, -R * 0.58f, R * 1.16f, R * 1.16f);
            juce::Path pointer;
            pointer.startNewSubPath (0.0f, -R * 1.02f);
            pointer.cubicTo (R * 0.12f, -R * 1.0f, R * 0.40f, -R * 0.2f, R * 0.42f, R * 0.18f);
            pointer.cubicTo (R * 0.44f, R * 0.62f, -R * 0.44f, R * 0.62f, -R * 0.42f, R * 0.18f);
            pointer.cubicTo (-R * 0.40f, -R * 0.2f, -R * 0.12f, -R * 1.0f, 0.0f, -R * 1.02f);
            pointer.closeSubPath();

            const auto rot = juce::AffineTransform::rotation (angle).translated (centre);
            juce::Path shadowShape (skirt);
            shadowShape.addPath (pointer);
            shadowShape.applyTransform (rot);
            juce::DropShadow (juce::Colours::black.withAlpha (0.8f), juce::roundToInt (R * 0.32f),
                              { juce::roundToInt (R * 0.07f), juce::roundToInt (R * 0.16f) }).drawForPath (g, shadowShape);

            skirt.applyTransform (juce::AffineTransform::translation (centre));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2c30), centre.x - R * 0.5f, centre.y - R * 0.5f,
                                                     juce::Colour (0xff050506), centre.x + R * 0.5f, centre.y + R * 0.5f, false));
            g.fillPath (skirt);

            pointer.applyTransform (rot);
            const auto pb = pointer.getBounds();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff57575e), pb.getX(), pb.getY(),
                                                     juce::Colour (0xff111113), pb.getRight(), pb.getBottom(), false));
            g.fillPath (pointer);
            // soft specular along the upper-left flank + rim light
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.30f), centre.x - R * 0.45f, centre.y - R * 0.45f,
                                                     juce::Colours::transparentWhite, centre.x + R * 0.1f, centre.y + R * 0.1f, false));
            g.fillPath (pointer);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.strokePath (pointer, juce::PathStrokeType (juce::jmax (0.6f, R * 0.025f)));
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.strokePath (pointer, juce::PathStrokeType (juce::jmax (0.5f, R * 0.02f)), juce::AffineTransform::translation (-0.5f, -0.7f));

            // white inlay line along the pointer
            juce::Path ip;
            ip.startNewSubPath (0.0f, -R * 0.88f);
            ip.lineTo (0.0f, -R * 0.12f);
            g.setColour (colours::bone);
            g.strokePath (ip, juce::PathStrokeType (juce::jmax (1.0f, R * 0.075f), juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded), rot);
            return;
        }

        const float scale = physicalScale (g);
        const auto img = materials::knob (style, juce::roundToInt (area.getWidth() * scale));
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.drawImage (img, area.withSizeKeepingCentre (area.getWidth() * 1.5f, area.getHeight() * 1.5f));

        const auto rot = juce::AffineTransform::rotation (angle).translated (centre);
        if (style == KnobStyle::spunAluminium)
        {
            const float Rc = R * 0.76f;
            juce::Path line;
            line.startNewSubPath (0.0f, -Rc * 0.92f);
            line.lineTo (0.0f, -Rc * 0.30f);
            const float w = juce::jmax (1.2f, R * 0.07f);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.strokePath (line, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                          rot.translated (0.45f, 0.6f));
            g.setColour (juce::Colour (0xff111113));
            g.strokePath (line, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), rot);
        }
        else
        {
            const float Rc = R * 0.80f;
            juce::Path line;
            line.startNewSubPath (0.0f, -R * 0.98f);
            line.lineTo (0.0f, -Rc * 0.36f);
            g.setColour (colours::bone.withAlpha (0.95f));
            g.strokePath (line, juce::PathStrokeType (juce::jmax (1.2f, R * 0.085f), juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded), rot);
        }
    }

    void knobScale (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour,
                    bool withNumbers, int numTicks)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        const float start = -0.75f * pi, end = 0.75f * pi;
        const float tickLen = radius * 0.1f;
        g.setColour (colour);
        for (int i = 0; i < numTicks; ++i)
        {
            const float a = start + (end - start) * (float) i / (float) (numTicks - 1);
            const float sx = std::sin (a), sy = -std::cos (a);
            const bool major = withNumbers || i == 0 || i == numTicks - 1 || i == numTicks / 2;
            const float r0 = radius, r1 = radius + (major ? tickLen * 1.4f : tickLen);
            g.drawLine (centre.x + sx * r0, centre.y + sy * r0, centre.x + sx * r1, centre.y + sy * r1,
                        juce::jmax (0.8f, radius * (major ? 0.028f : 0.02f)));

            if (withNumbers)
            {
                const float rt = radius + tickLen * 3.0f;
                const auto font = Fonts::label (radius * 0.2f, 0.0f);
                g.setFont (font);
                const juce::String txt (juce::roundToInt (10.0f * (float) i / (float) (numTicks - 1)));
                const float tw = radius * 0.5f, th = radius * 0.24f;
                g.drawText (txt, juce::Rectangle<float> (centre.x + sx * rt - tw * 0.5f, centre.y + sy * rt - th * 0.5f, tw, th),
                            juce::Justification::centred, false);
            }
        }
    }

    void footswitch (juce::Graphics& g, juce::Rectangle<float> area, bool pressed)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        area = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), area.getHeight()),
                                           juce::jmin (area.getWidth(), area.getHeight()));
        const float scale = physicalScale (g);
        const auto img = materials::footswitch (juce::roundToInt (area.getWidth() * scale), pressed);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.drawImage (img, area.withSizeKeepingCentre (area.getWidth() * 1.3f, area.getHeight() * 1.3f));
    }

    void led (juce::Graphics& g, juce::Rectangle<float> area, bool on, juce::Colour colour)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        const auto c = area.getCentre();
        const float R = area.getWidth() * 0.5f;

        if (on)
        {
            juce::ColourGradient glow (colour.withAlpha (0.42f), c.x, c.y, colour.withAlpha (0.0f), c.x + R * 3.2f, c.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (area.withSizeKeepingCentre (R * 6.4f, R * 6.4f));
        }

        // chrome bezel
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9d9dd), c.x - R, c.y - R,
                                                 juce::Colour (0xff3a3a3e), c.x + R, c.y + R, false));
        g.fillEllipse (area);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (area.reduced (0.25f), 0.6f);

        const auto lens = area.reduced (R * 0.24f);
        if (on)
        {
            juce::ColourGradient core (juce::Colours::white, c.x - R * 0.15f, c.y - R * 0.2f,
                                       colour, c.x + R * 0.6f, c.y + R * 0.6f, true);
            core.addColour (0.35, colour.brighter (0.6f));
            g.setGradientFill (core);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (colours::ledOff.brighter (0.4f), c.x - R * 0.3f, c.y - R * 0.3f,
                                                     colours::ledOff.darker (0.6f), c.x + R * 0.5f, c.y + R * 0.5f, true));
        }
        g.fillEllipse (lens);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.6f : 0.35f));
        g.fillEllipse (lens.getX() + lens.getWidth() * 0.22f, lens.getY() + lens.getHeight() * 0.16f,
                       lens.getWidth() * 0.3f, lens.getHeight() * 0.22f);
    }

    void screw (juce::Graphics& g, juce::Point<float> c, float r, float slotAngle)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - r + r * 0.15f, c.y - r + r * 0.25f, r * 2.0f, r * 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8d8d93), c.x - r * 0.7f, c.y - r * 0.8f,
                                                 juce::Colour (0xff1c1c1f), c.x + r * 0.7f, c.y + r * 0.8f, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        juce::Path slot;
        slot.addRectangle (-r * 0.78f, -r * 0.13f, r * 1.56f, r * 0.26f);
        slot.applyTransform (juce::AffineTransform::rotation (slotAngle).translated (c));
        g.setColour (juce::Colour (0xff0b0b0c));
        g.fillPath (slot);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawEllipse (c.x - r + 0.4f, c.y - r + 0.4f, r * 2.0f - 0.8f, r * 2.0f - 0.8f, 0.6f);
    }

    void jewel (juce::Graphics& g, juce::Rectangle<float> area, bool on, juce::Colour colour)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        const auto c = area.getCentre();
        const float R = area.getWidth() * 0.5f;

        if (on)
        {
            juce::ColourGradient glow (colour.withAlpha (0.5f), c.x, c.y, colour.withAlpha (0.0f), c.x + R * 3.4f, c.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (area.withSizeKeepingCentre (R * 6.8f, R * 6.8f));
        }

        // chrome bezel ring
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (area.expanded (R * 0.2f).translated (R * 0.06f, R * 0.14f));
        juce::ColourGradient bezel (juce::Colour (0xffeeeef0), c.x - R, c.y - R, juce::Colour (0xff2e2f33), c.x + R, c.y + R, false);
        bezel.addColour (0.5, juce::Colour (0xff8c8d92));
        g.setGradientFill (bezel);
        g.fillEllipse (area.expanded (R * 0.18f));

        // faceted lens: alternating light / dark wedges around a bright core
        const auto lens = area.reduced (R * 0.08f);
        const auto base = on ? colour : colours::ledOff.brighter (0.2f);
        g.setColour (base.darker (on ? 0.25f : 0.5f));
        g.fillEllipse (lens);
        const int facets = 12;
        for (int i = 0; i < facets; ++i)
        {
            const float a0 = juce::MathConstants<float>::twoPi * (float) i / (float) facets;
            const float a1 = a0 + juce::MathConstants<float>::twoPi / (float) facets;
            juce::Path wedge;
            wedge.startNewSubPath (c);
            wedge.lineTo (c.x + std::sin (a0) * R * 0.92f, c.y - std::cos (a0) * R * 0.92f);
            wedge.lineTo (c.x + std::sin (a1) * R * 0.92f, c.y - std::cos (a1) * R * 0.92f);
            wedge.closeSubPath();
            const float lit = 0.5f + 0.5f * std::cos ((a0 + a1) * 0.5f + 0.8f);
            g.setColour ((i % 2 == 0 ? base.brighter (on ? 0.25f : 0.1f) : base.darker (0.15f)).withAlpha (0.12f + 0.22f * lit));
            g.fillPath (wedge);
        }
        {
            juce::ColourGradient dome (juce::Colours::transparentBlack, c.x - R * 0.2f, c.y - R * 0.25f,
                                       juce::Colours::black.withAlpha (0.55f), c.x + R, c.y + R, true);
            g.setGradientFill (dome);
            g.fillEllipse (lens);
        }
        if (on)
        {
            juce::ColourGradient core (juce::Colours::white.withAlpha (0.9f), c.x, c.y, base.withAlpha (0.0f), c.x + R * 0.55f, c.y, true);
            core.addColour (0.4, base.brighter (0.6f).withAlpha (0.6f));
            g.setGradientFill (core);
            g.fillEllipse (lens);
        }
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillEllipse (c.x - R * 0.55f, c.y - R * 0.62f, R * 0.42f, R * 0.28f);
    }

    void batToggle (juce::Graphics& g, juce::Rectangle<float> area, bool up)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        const auto c = area.getCentre();
        const float s = juce::jmin (area.getWidth(), area.getHeight() * 0.5f);

        // hex nut + washer
        juce::Path nut;
        nut.addPolygon (c, 6, s * 0.42f, 0.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillPath (nut, juce::AffineTransform::translation (s * 0.04f, s * 0.08f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffcfd0d4), c.x - s * 0.4f, c.y - s * 0.4f,
                                                 juce::Colour (0xff44454a), c.x + s * 0.4f, c.y + s * 0.4f, false));
        g.fillPath (nut);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeeeeef), c.x - s * 0.2f, c.y - s * 0.2f,
                                                 juce::Colour (0xff5a5b60), c.x + s * 0.2f, c.y + s * 0.25f, false));
        g.fillEllipse (c.x - s * 0.24f, c.y - s * 0.24f, s * 0.48f, s * 0.48f);

        // bat lever (tapered, foreshortened toward the viewer)
        const float dir = up ? -1.0f : 1.0f;
        const float len = s * 0.95f;
        juce::Path lever;
        lever.startNewSubPath (c.x - s * 0.11f, c.y);
        lever.lineTo (c.x - s * 0.17f, c.y + dir * len);
        lever.lineTo (c.x + s * 0.17f, c.y + dir * len);
        lever.lineTo (c.x + s * 0.11f, c.y);
        lever.closeSubPath();
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), juce::roundToInt (s * 0.18f),
                          { juce::roundToInt (s * 0.08f), juce::roundToInt (s * 0.16f) }).drawForPath (g, lever);
        juce::ColourGradient chrome (juce::Colour (0xff6d6e73), c.x - s * 0.17f, c.y,
                                     juce::Colour (0xff55565b), c.x + s * 0.17f, c.y, false);
        chrome.addColour (0.35, juce::Colour (0xfff4f4f6));
        chrome.addColour (0.55, juce::Colour (0xffb9babf));
        g.setGradientFill (chrome);
        g.fillPath (lever);
        const auto tip = juce::Rectangle<float> (s * 0.42f, s * 0.42f).withCentre ({ c.x, c.y + dir * len });
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, tip.getX() + tip.getWidth() * 0.3f, tip.getY() + tip.getHeight() * 0.25f,
                                                 juce::Colour (0xff505156), tip.getRight(), tip.getBottom(), true));
        g.fillEllipse (tip);
    }

    void silkscreen (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                     juce::Rectangle<float> area, juce::Justification just, juce::Colour colour)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        g.setFont (font);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawText (text, area.translated (0.0f, 1.0f), just, false);
        g.setColour (colour);
        g.drawText (text, area, just, false);
    }

    void chromeText (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                     juce::Rectangle<float> area, juce::Justification just)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        juce::GlyphArrangement ga;
        ga.addFittedText (font, text, area.getX(), area.getY(), area.getWidth(), area.getHeight(), just, 1, 1.0f);
        juce::Path p;
        ga.createPath (p);
        const auto b = p.getBounds();

        juce::DropShadow (juce::Colours::black.withAlpha (0.85f), juce::roundToInt (b.getHeight() * 0.12f),
                          { 0, juce::roundToInt (juce::jmax (1.0f, b.getHeight() * 0.05f)) }).drawForPath (g, p);

        juce::ColourGradient chrome (juce::Colour (0xfffdfdfd), b.getX(), b.getY(),
                                     juce::Colour (0xffd8d8dc), b.getX(), b.getBottom(), false);
        chrome.addColour (0.42, juce::Colour (0xffb7b8bd));
        chrome.addColour (0.50, juce::Colour (0xff4a4b50));
        chrome.addColour (0.62, juce::Colour (0xff8e8f95));
        chrome.addColour (0.85, juce::Colour (0xfff0f0f2));
        g.setGradientFill (chrome);
        g.fillPath (p);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.strokePath (p, juce::PathStrokeType (juce::jmax (0.6f, b.getHeight() * 0.018f)));
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (juce::jmax (0.4f, b.getHeight() * 0.008f)),
                      juce::AffineTransform::translation (0.0f, -0.6f));
    }

    void displayGlass (juce::Graphics& g, juce::Rectangle<float> area, float r)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        // bezel
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff050506), area.getX(), area.getY(),
                                                 juce::Colour (0xff2a2a2e), area.getX(), area.getBottom(), false));
        g.fillRoundedRectangle (area.expanded (1.5f), r + 1.5f);
        // glass body
        g.setColour (juce::Colour (0xff060606));
        g.fillRoundedRectangle (area, r);
        // inner shadow
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.16f));
            g.drawRoundedRectangle (area.reduced ((float) i * 0.8f), r, 1.2f);
        }
    }

    namespace
    {
        // Segment bits: a b c d e f g = 0..6
        int segmentsFor (juce::juce_wchar ch)
        {
            switch (ch)
            {
                case '0': return 0b0111111; case '1': return 0b0000110; case '2': return 0b1011011;
                case '3': return 0b1001111; case '4': return 0b1100110; case '5': return 0b1101101;
                case '6': return 0b1111101; case '7': return 0b0000111; case '8': return 0b1111111;
                case '9': return 0b1101111; case '-': return 0b1000000;
                case 'A': return 0b1110111; case 'b': return 0b1111100; case 'C': return 0b0111001;
                case 'd': return 0b1011110; case 'E': return 0b1111001; case 'F': return 0b1110001;
                case 'G': return 0b0111101; case 'H': return 0b1110110; case 'L': return 0b0111000;
                case 'n': return 0b1010100; case 'o': return 0b1011100; case 'P': return 0b1110011;
                case 'r': return 0b1010000; case 't': return 0b1111000; case 'U': return 0b0111110;
                default:  return 0;
            }
        }

        juce::Path segmentPath (int index, juce::Rectangle<float> cell, float t)
        {
            const float x0 = cell.getX(), y0 = cell.getY(), w = cell.getWidth(), h = cell.getHeight();
            const float gap = t * 0.18f, hm = h * 0.5f;
            auto horizontal = [&] (float y)
            {
                juce::Path p;
                const float l = x0 + gap, r = x0 + w - gap;
                p.startNewSubPath (l, y);
                p.lineTo (l + t * 0.5f, y - t * 0.5f);
                p.lineTo (r - t * 0.5f, y - t * 0.5f);
                p.lineTo (r, y);
                p.lineTo (r - t * 0.5f, y + t * 0.5f);
                p.lineTo (l + t * 0.5f, y + t * 0.5f);
                p.closeSubPath();
                return p;
            };
            auto vertical = [&] (float x, float top, float bottom)
            {
                juce::Path p;
                const float tp = top + gap, bt = bottom - gap;
                p.startNewSubPath (x, tp);
                p.lineTo (x + t * 0.5f, tp + t * 0.5f);
                p.lineTo (x + t * 0.5f, bt - t * 0.5f);
                p.lineTo (x, bt);
                p.lineTo (x - t * 0.5f, bt - t * 0.5f);
                p.lineTo (x - t * 0.5f, tp + t * 0.5f);
                p.closeSubPath();
                return p;
            };
            const float yt = y0 + t * 0.5f, yb = y0 + h - t * 0.5f, ym = y0 + hm;
            const float xl = x0 + t * 0.5f, xr = x0 + w - t * 0.5f;
            switch (index)
            {
                case 0: return horizontal (yt);
                case 1: return vertical (xr, yt, ym);
                case 2: return vertical (xr, ym, yb);
                case 3: return horizontal (yb);
                case 4: return vertical (xl, ym, yb);
                case 5: return vertical (xl, yt, ym);
                default: return horizontal (ym);
            }
        }
    }

    void sevenSegment (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                       juce::Colour lit, int numCells)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        const float cellW = area.getWidth() / (float) numCells;
        const float digitW = cellW * 0.78f;
        const float t = digitW * 0.17f;
        const float skew = 0.1f;
        const auto shear = juce::AffineTransform::shear (-skew, 0.0f).translated (skew * area.getCentreY(), 0.0f);

        // right-align the text into the cells
        juce::String s = text.substring (juce::jmax (0, text.length() - numCells));
        s = juce::String::repeatedString (" ", numCells - s.length()) + s;

        const auto ghost = lit.interpolatedWith (juce::Colours::black, 0.9f).withAlpha (0.55f);
        juce::Path litPath;

        for (int i = 0; i < numCells; ++i)
        {
            const auto cell = juce::Rectangle<float> (area.getX() + cellW * (float) i + (cellW - digitW) * 0.5f,
                                                      area.getY(), digitW, area.getHeight());
            const auto ch = s[i];
            int bits = segmentsFor (ch);
            for (int seg = 0; seg < 7; ++seg)
            {
                auto p = segmentPath (seg, cell, t);
                p.applyTransform (shear);
                if (bits & (1 << seg))
                    litPath.addPath (p);
                else
                {
                    g.setColour (ghost);
                    g.fillPath (p);
                }
            }
            if (ch == '+')
            {
                // horizontal bar plus a short vertical bar through the middle
                auto bar = segmentPath (6, cell, t);
                juce::Path vbar;
                vbar.addRoundedRectangle (cell.getCentreX() - t * 0.5f, cell.getCentreY() - cell.getWidth() * 0.32f,
                                          t, cell.getWidth() * 0.64f, t * 0.3f);
                bar.addPath (vbar);
                bar.applyTransform (shear);
                litPath.addPath (bar);
            }
        }

        juce::DropShadow (lit.withAlpha (0.85f), juce::roundToInt (t * 2.2f), {}).drawForPath (g, litPath);
        const auto b = litPath.getBounds();
        g.setGradientFill (juce::ColourGradient (lit.brighter (0.35f), b.getX(), b.getY(),
                                                 lit, b.getX(), b.getBottom(), false));
        g.fillPath (litPath);
    }

    void cable (juce::Graphics& g, const juce::Path& path, float thickness)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        juce::Path stroke;
        juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath (stroke, path);
        juce::DropShadow (juce::Colours::black.withAlpha (0.65f), juce::roundToInt (thickness * 1.4f),
                          { juce::roundToInt (thickness * 0.4f), juce::roundToInt (thickness * 1.1f) }).drawForPath (g, stroke);
        g.setColour (juce::Colour (0xff0c0c0d));
        g.fillPath (stroke);
        g.setColour (juce::Colour (0xff3c3c42).withAlpha (0.8f));
        g.strokePath (path, juce::PathStrokeType (thickness * 0.28f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (-thickness * 0.12f, -thickness * 0.22f));
    }

    void jackPlug (juce::Graphics& g, juce::Point<float> tip, float h, int direction)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        const float d = (float) direction;
        // barrel (chrome) then boot (black)
        const float barrelLen = h * 1.3f, bootLen = h * 1.7f;
        auto barrel = juce::Rectangle<float> (barrelLen, h * 0.86f).withCentre ({ tip.x - d * barrelLen * 0.5f, tip.y });
        auto boot = juce::Rectangle<float> (bootLen, h).withCentre ({ tip.x - d * (barrelLen + bootLen * 0.5f), tip.y });

        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), juce::roundToInt (h * 0.5f), { 0, juce::roundToInt (h * 0.35f) })
            .drawForRectangle (g, boot.getUnion (barrel).toNearestInt());

        juce::ColourGradient chrome (juce::Colour (0xffeeeeef), 0.0f, barrel.getY(), juce::Colour (0xff3d3e42), 0.0f, barrel.getBottom(), false);
        chrome.addColour (0.3, juce::Colour (0xffb8b9be));
        chrome.addColour (0.6, juce::Colour (0xff6c6d72));
        g.setGradientFill (chrome);
        g.fillRoundedRectangle (barrel, h * 0.12f);

        juce::ColourGradient rubber (juce::Colour (0xff3a3a3f), 0.0f, boot.getY(), juce::Colour (0xff0a0a0b), 0.0f, boot.getBottom(), false);
        g.setGradientFill (rubber);
        g.fillRoundedRectangle (boot, h * 0.3f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        for (int i = 1; i < 4; ++i)
        {
            const float x = boot.getX() + boot.getWidth() * (float) i / 4.0f;
            g.drawLine (x, boot.getY() + h * 0.12f, x, boot.getBottom() - h * 0.12f, 0.8f);
        }
    }

    void contactShadow (juce::Graphics& g, juce::Rectangle<float> b, float spread, float opacity)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        const auto area = b.expanded (spread, spread * 0.5f).translated (0.0f, spread * 0.55f);
        juce::ColourGradient grad (juce::Colours::black.withAlpha (opacity), area.getCentreX(), area.getCentreY(),
                                   juce::Colours::transparentBlack, area.getRight(), area.getCentreY(), true);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (area, spread);
    }

    void texturedFill (juce::Graphics& g, const juce::Path& shape, juce::Colour base,
                       const juce::Image& texture, float opacity, float textureScale)
    {
        const juce::Graphics::ScopedSaveState saved (g);
        g.setColour (base);
        g.fillPath (shape);
        // Grain is defined in physical pixels: finer (more real) on high-DPI
        // screens instead of getting blown up with the UI scale.
        const float phys = physicalScale (g);
        juce::FillType ft (texture, juce::AffineTransform::scale (textureScale / phys));
        ft.setOpacity (opacity);
        g.setFillType (ft);
        g.fillPath (shape);
    }
}

} // namespace apex::ui
