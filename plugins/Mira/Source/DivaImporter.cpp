#include "DivaImporter.h"

namespace diva
{
namespace
{
// Mira choice indices
enum MiraShape { Saw = 0, Pulse = 1, Triangle = 2, Sine = 3 };
enum MiraFilter { LP24 = 0, LP12 = 1, BP12 = 2, HP24 = 3 };
enum MiraLfo { LSine = 0, LTriangle = 1, LSaw = 2, LSquare = 3, LSampleHold = 4 };

// Diva modulation-source indices (the "#ms=" list in every file).
constexpr int srcEnv1 = 14, srcEnv2 = 15, srcLfo1 = 16, srcLfo2 = 17;

float clampf (float v, float lo, float hi) { return juce::jlimit (lo, hi, v); }

/** Diva envelope knobs are 0..100 on a steep curve; this approximates the time in seconds. */
float envTime (float v)
{
    const float x = clampf (v, 0.0f, 100.0f) / 100.0f;
    return clampf (0.001f + 12.0f * x * x * x, 0.001f, 10.0f);
}

/** Diva cutoff knobs are in semitones on the MIDI-note scale (30..150). */
float cutoffHz (float v) { return clampf (440.0f * std::exp2 ((v - 69.0f) / 12.0f), 20.0f, 20000.0f); }

struct Osc
{
    int shape = Saw;
    float level = 0.0f;
    float tune = 0.0f;   // semitones relative to the played note
    float pulseWidth = -1.0f;
    bool active() const { return level > 0.005f; }
};

/** Triple VCO waveform knob: 1 triangle, 3 shark-tooth, 5 saw, 7 square, 9 narrow pulse. */
Osc moogShape (float v)
{
    Osc o;
    if (v < 2.0f)      o.shape = Triangle;
    else if (v < 6.0f) o.shape = Saw;
    else
    {
        o.shape = Pulse;
        o.pulseWidth = clampf (0.5f - (v - 7.0f) * 0.2f, 0.1f, 0.5f);
    }
    return o;
}

/** "Eco" models' waveform knob 1..4. */
int ecoShape (float v)
{
    if (v < 1.5f) return Saw;
    if (v < 2.5f) return Pulse;
    if (v < 3.5f) return Triangle;
    return Sine;
}

/** Digital oscillator types: Multisaw, TriWrap, Noise, Feedback, Pulse, Saw, Triangle. */
int digitalShape (int type, bool& isNoise)
{
    isNoise = (type == 2);
    switch (type)
    {
        case 1:  return Triangle;
        case 4:  return Pulse;
        case 6:  return Triangle;
        default: return Saw;
    }
}

float lfoRateHz (int sync, float rate)
{
    // Tempo-synced values assume 120 BPM (Mira's LFO is free-running).
    constexpr float beatSeconds = 0.5f;
    float periodSeconds = 1.0f;

    static const float straight[] = { 1.0f / 16, 1.0f / 8, 1.0f / 4, 1.0f / 2, 1.0f, 2.0f, 4.0f };          // 1/64 .. 1/1 in beats
    static const float dotted[]   = { 1.0f / 8 * 1.5f, 1.0f / 4 * 1.5f, 1.0f / 2 * 1.5f, 1.5f, 3.0f };      // 1/32. .. 1/2.
    static const float triplet[]  = { 1.0f / 4 * 2 / 3, 1.0f / 2 * 2 / 3, 2.0f / 3, 4.0f / 3, 8.0f / 3 };  // 1/16t .. 1/1t

    if (sync == 0)                    periodSeconds = 0.1f;
    else if (sync == 1)               periodSeconds = 1.0f;
    else if (sync == 2)               periodSeconds = 10.0f;
    else if (sync >= 3 && sync <= 9)  periodSeconds = straight[sync - 3] * beatSeconds;
    else if (sync >= 10 && sync <= 14) periodSeconds = dotted[sync - 10] * beatSeconds;
    else if (sync >= 15 && sync <= 19) periodSeconds = triplet[sync - 15] * beatSeconds;

    return clampf ((1.0f / periodSeconds) * std::exp2 (clampf (rate, -5.0f, 5.0f)), 0.05f, 20.0f);
}

int lfoShape (int diva)
{
    switch (diva)
    {
        case 0:  return LSine;
        case 1:  return LTriangle;
        case 2:
        case 3:  return LSaw;
        case 4:
        case 5:  return LSquare;
        default: return LSampleHold;
    }
}
} // namespace

float H2P::num (const juce::String& section, const juce::String& key, float fallback) const
{
    auto s = sections.find (section);
    if (s == sections.end())
        return fallback;
    auto k = s->second.find (key);
    if (k == s->second.end() || k->second.isEmpty())
        return fallback;
    return k->second.getFloatValue();
}

juce::String H2P::str (const juce::String& section, const juce::String& key) const
{
    auto s = sections.find (section);
    if (s == sections.end())
        return {};
    auto k = s->second.find (key);
    return k == s->second.end() ? juce::String() : k->second.unquoted();
}

H2P parse (const juce::String& text)
{
    H2P result;
    juce::String section;
    bool inMeta = false;

    for (auto line : juce::StringArray::fromLines (text))
    {
        line = line.trim();

        if (line.startsWith ("$$$$") || line.startsWith ("// Section for ugly"))
            break; // the rest is u-he's binary blob

        if (line.startsWith ("/*")) { inMeta = ! line.contains ("*/"); continue; }
        if (inMeta)
        {
            if (line.contains ("*/"))
                inMeta = false;
            else if (line.isNotEmpty() && ! line.endsWithChar (':') && result.author.isEmpty())
                result.author = line.unquoted();
            continue;
        }

        // Extended h2p files carry "// comment" after values.
        const int comment = line.indexOf ("//");
        if (comment >= 0)
            line = line.substring (0, comment).trim();
        if (line.isEmpty())
            continue;

        if (line.startsWith ("#cm="))
        {
            section = line.substring (4).trim();
            result.sections[section];
            continue;
        }

        const int eq = line.indexOfChar ('=');
        if (eq <= 0)
            continue;

        const auto key = line.substring (0, eq).trim();
        const auto value = line.substring (eq + 1).trim();

        if (key.startsWithChar ('#'))
            result.globals[key.substring (1)] = value;
        else if (section.isNotEmpty())
            result.sections[section][key] = value;
    }

    return result;
}

juce::String guessCategory (const juce::String& name)
{
    const auto n = name.toUpperCase();
    auto starts = [&n] (std::initializer_list<const char*> tags)
    {
        for (auto* t : tags)
        {
            const juce::String tag (t);
            if (n.startsWith (tag + " ") || n.startsWith (tag + "_") || n.startsWith (tag + "-") || n.startsWith (tag + ":"))
                return true;
        }
        return false;
    };
    auto contains = [&n] (const char* word) { return n.contains (word); };

    if (starts ({ "BS", "BA", "BASS", "SB", "LOW" }) || contains ("BASS"))   return "Bass";
    if (starts ({ "LD", "LEAD" }) || contains ("LEAD"))                        return "Lead";
    if (starts ({ "PD", "PAD" }) || contains ("PAD"))                          return "Pad";
    if (starts ({ "PL", "PLK", "PLUCK" }) || contains ("PLUCK"))               return "Pluck";
    if (starts ({ "KY", "KEY", "KEYS", "EP", "ORG", "PN" }) || contains ("KEYS")) return "Keys";
    if (starts ({ "BR", "BRASS" }) || contains ("BRASS"))                      return "Brass";
    if (starts ({ "ST", "STR" }) || contains ("STRING"))                       return "Strings";
    if (starts ({ "SQ", "SEQ", "ARP" }) || contains ("ARP"))                   return "Sequence";
    if (starts ({ "FX", "SFX", "NOISE" }) || contains ("FX"))                  return "FX";
    if (starts ({ "DR", "PERC", "DRUM" }) || contains ("DRUM") || contains ("KICK")) return "Drums";
    return "Synth";
}

Converted convert (const H2P& p, const juce::String& name)
{
    Converted out;
    out.name = name;
    out.author = p.author;
    out.category = guessCategory (name);

    if (! p.has ("OSC") || ! p.has ("VCF1"))
    {
        out.error = "Not a Diva preset (no OSC/VCF sections)";
        return out;
    }

    auto& v = out.values;
    auto set = [&v] (const char* id, float value) { v.emplace_back (id, value); };

    // ---- Voice --------------------------------------------------------------------------
    const int vccMode = (int) p.num ("VCC", "Mode", 0);      // poly, mono, legato, duo, poly2
    set ("voicemode", vccMode == 2 ? 2.0f : (vccMode == 1 || vccMode == 3) ? 1.0f : 0.0f);
    const float porta = p.num ("VCC", "Porta", 0.0f);
    set ("glide", porta <= 0.0f ? 0.0f : clampf (2.0f * std::pow (porta / 100.0f, 2.0f), 0.0f, 2.0f));
    const float transpose = p.num ("VCC", "Trsp", 0.0f) + p.num ("VCC", "FTun", 0.0f) / 100.0f;

    // ---- Oscillators --------------------------------------------------------------------
    const int model = (int) p.num ("OSC", "Model", 0);
    const float t1 = p.num ("OSC", "Tune1", 0.0f) + transpose;
    const float t2 = p.num ("OSC", "Tune2", 0.0f) + transpose;
    const float t3 = p.num ("OSC", "Tune3", 0.0f) + transpose;
    const float mix = clampf (p.num ("OSC", "OscMix", 50.0f) / 100.0f, 0.0f, 1.0f);
    const float mixLevel1 = juce::jmin (1.0f, 2.0f * (1.0f - mix));
    const float mixLevel2 = juce::jmin (1.0f, 2.0f * mix);
    auto on = [&p] (const char* key) { return p.num ("OSC", key, 0.0f) > 0.5f; };

    Osc o1, o2;
    float subLevel = 0.0f;
    float noise = clampf (p.num ("OSC", "Noise", 0.0f) / 100.0f, 0.0f, 1.0f);
    float pulseWidth = 0.05f + 0.9f * clampf (p.num ("OSC", "PW", 50.0f) / 100.0f, 0.0f, 1.0f);

    if (model == 0) // Triple VCO
    {
        o1 = moogShape (p.num ("OSC", "Shape1", 5.0f));
        o2 = moogShape (p.num ("OSC", "Shape2", 5.0f));
        Osc o3 = moogShape (p.num ("OSC", "Shape3", 5.0f));
        o1.level = p.num ("OSC", "Vol1", 100.0f) / 100.0f; o1.tune = t1;
        o2.level = p.num ("OSC", "Vol2", 0.0f) / 100.0f;   o2.tune = t2;
        o3.level = p.num ("OSC", "Vol3", 0.0f) / 100.0f;   o3.tune = t3;

        // Mira has two oscillators + a sub: fold the third VCO in where it fits best.
        if (o3.active())
        {
            if (std::abs (o3.tune - (o1.tune - 12.0f)) < 0.3f)
                subLevel = o3.level;
            else if (! o2.active() || o3.level > o2.level)
                o2 = o3;
            else if (! o1.active())
                o1 = o3;
        }
        for (auto* o : { &o1, &o2 })
            if (o->shape == Pulse && o->pulseWidth > 0.0f)
                { pulseWidth = o->pulseWidth; break; }
    }
    else if (model == 1) // Dual VCO: waveform switches
    {
        o1.shape = on ("Saw1") ? Saw : on ("PWM1") ? Pulse : Triangle;
        o1.level = (on ("Saw1") || on ("PWM1") || on ("Tri1")) ? mixLevel1 : 0.0f;
        if (on ("Noise1"))
            noise = juce::jmax (noise, 0.5f * mixLevel1);
        o2.shape = on ("Saw2") ? Saw : on ("PW2") ? Pulse : on ("Tri2") ? Triangle : Sine;
        o2.level = (on ("Saw2") || on ("PW2") || on ("Tri2") || on ("Sin2")) ? mixLevel2 : 0.0f;
        o1.tune = t1;
        o2.tune = t2;
    }
    else if (model == 2) // DCO: pulse + saw from one oscillator, plus sub
    {
        const bool pulseOn = p.num ("OSC", "PWShp", 1.0f) > 0.5f;
        const bool sawOn = p.num ("OSC", "SawShp", 0.0f) > 0.5f;
        o1.shape = pulseOn ? Pulse : Saw;
        o1.level = (pulseOn || sawOn) ? 0.8f : 0.0f;
        o1.tune = t1;
        if (pulseOn && sawOn)
        {
            o2.shape = Saw;
            o2.level = 0.8f;
            o2.tune = t1;
        }
        subLevel = clampf (p.num ("OSC", "Vol3", 0.0f) / 100.0f, 0.0f, 1.0f); // sub volume
    }
    else if (model == 3) // Dual VCO Eco
    {
        o1.shape = ecoShape (p.num ("OSC", "Wave1", 1.0f));
        o2.shape = ecoShape (p.num ("OSC", "Wave2", 1.0f));
        o1.level = mixLevel1; o1.tune = t1;
        o2.level = mixLevel2; o2.tune = t2;
    }
    else // Digital
    {
        bool noise1 = false, noise2 = false;
        o1.shape = digitalShape ((int) p.num ("OSC", "DgtlTp1", 0), noise1);
        o2.shape = digitalShape ((int) p.num ("OSC", "DgtlTp2", 0), noise2);
        o1.level = mixLevel1; o1.tune = t1;
        o2.level = mixLevel2; o2.tune = t2;
        if (noise1) { noise = juce::jmax (noise, o1.level); o1.level = 0.0f; }
        if (noise2) { noise = juce::jmax (noise, o2.level); o2.level = 0.0f; }
    }

    // Keep the louder oscillator in slot 1 when slot 1 is silent (slot 1 has octave-only tuning).
    if (! o1.active() && o2.active())
        std::swap (o1, o2);

    set ("osc1shape", (float) o1.shape);
    set ("osc1oct", clampf (std::round (o1.tune / 12.0f), -2.0f, 2.0f));
    set ("osc1lvl", clampf (o1.level, 0.0f, 1.0f) * 0.8f);

    {
        const float total = clampf (o2.tune, -36.0f, 36.0f);
        const float oct = clampf (std::round (total / 12.0f), -2.0f, 2.0f);
        const float rem = total - oct * 12.0f;
        const float semi = clampf (std::round (rem), -12.0f, 12.0f);
        set ("osc2shape", (float) o2.shape);
        set ("osc2oct", oct);
        set ("osc2semi", semi);
        set ("osc2fine", clampf ((rem - semi) * 100.0f, -50.0f, 50.0f));
        set ("osc2lvl", clampf (o2.level, 0.0f, 1.0f) * 0.8f);
    }

    set ("sublvl", clampf (subLevel, 0.0f, 1.0f) * 0.8f);
    set ("noiselvl", noise * 0.6f);
    set ("pw", clampf (pulseWidth, 0.05f, 0.95f));
    set ("drift", clampf (p.num ("OSC", "Drift", 35.0f) / 100.0f, 0.0f, 1.0f));

    // ---- Filter -------------------------------------------------------------------------
    const int vcf = (int) p.num ("VCF1", "Model", 0); // Ladder, Cascade, Multimode, BITE, Uhbie
    int mode = LP24;
    if (vcf == 0)      mode = p.num ("VCF1", "LMode", 0) > 0.5f ? LP12 : LP24;
    else if (vcf == 2)
    {
        static const int svf[] = { LP24, LP12, HP24, BP12 };
        mode = svf[juce::jlimit (0, 3, (int) p.num ("VCF1", "SvfMode", 0))];
    }
    else if (vcf == 3) mode = LP12;
    else if (vcf == 4) mode = p.num ("VCF1", "BpUbie", 0) > 0.5f ? BP12 : LP24;
    set ("fmode", (float) mode);
    set ("cutoff", cutoffHz (p.num ("VCF1", "Freq", 150.0f)));
    set ("res", clampf (p.num ("VCF1", "Res", 0.0f) / 100.0f * 0.97f, 0.0f, 1.0f));
    set ("keytrack", clampf (p.num ("VCF1", "KeyScl", 0.0f) / 100.0f, 0.0f, 1.0f));

    float drive = 0.15f + 0.6f * clampf (p.num ("VCF1", "Fback", 0.0f) / 100.0f, 0.0f, 1.0f);
    if (vcf == 0 && p.num ("VCF1", "LColor", 0) > 0.5f)
        drive += 0.1f;
    set ("drive", clampf (drive, 0.0f, 1.0f));

    // Filter modulation: envelope amount (semitones -> Mira's +-6 octaves), LFO amount.
    float envSemis = 0.0f, lfoFilterSemis = 0.0f;
    int filterEnvSource = srcEnv2, filterLfo = -1;
    for (auto [srcKey, dptKey] : { std::pair { "FMSrc", "FMDpt" }, std::pair { "FM2Src", "FM2Dpt" } })
    {
        const int src = (int) p.num ("VCF1", srcKey, 0);
        const float depth = p.num ("VCF1", dptKey, 0.0f);
        if ((src == srcEnv1 || src == srcEnv2) && std::abs (depth) > std::abs (envSemis))
        {
            envSemis = depth;
            filterEnvSource = src;
        }
        else if ((src == srcLfo1 || src == srcLfo2) && std::abs (depth) > std::abs (lfoFilterSemis))
        {
            lfoFilterSemis = depth;
            filterLfo = src == srcLfo1 ? 1 : 2;
        }
    }
    set ("envamt", clampf (envSemis / 72.0f, -1.0f, 1.0f));

    // ---- Envelopes ----------------------------------------------------------------------
    auto envelope = [&p, &set] (const juce::String& section, const char* a, const char* d, const char* s, const char* r)
    {
        const float dec = envTime (p.num (section, "Dec", 50.0f));
        const bool adsModel = (int) p.num (section, "Model", 1) == 0; // ADS: release = decay
        set (a, envTime (p.num (section, "Atk", 0.0f)));
        set (d, dec);
        set (s, clampf (p.num (section, "Sus", 100.0f) / 100.0f, 0.0f, 1.0f));
        set (r, adsModel ? dec : envTime (p.num (section, "Rel", 20.0f)));
    };

    if (p.num ("VCA1", "VCA", 1) > 0.5f)
        envelope ("ENV1", "aatt", "adec", "asus", "arel");
    else
    {
        set ("aatt", 0.001f); set ("adec", 0.1f); set ("asus", 1.0f); set ("arel", 0.01f); // gate
    }
    envelope (filterEnvSource == srcEnv1 ? "ENV1" : "ENV2", "fatt", "fdec", "fsus", "frel");
    set ("velsens", clampf (p.num ("ENV1", "Vel", 0.0f) / 100.0f, 0.0f, 1.0f));

    // ---- LFO (Mira has one; pick the one doing the most audible job) ---------------------
    float pitchSemis = 0.0f, pwmDepth = 0.0f;
    int pitchLfo = -1, pwmLfo = -1;
    for (auto [srcKey, dptKey] : { std::pair { "TM1Src", "TM1Dpt" }, std::pair { "TM2Src", "TM2Dpt" } })
    {
        const int src = (int) p.num ("OSC", srcKey, 0);
        const float depth = p.num ("OSC", dptKey, 0.0f);
        if ((src == srcLfo1 || src == srcLfo2) && std::abs (depth) > std::abs (pitchSemis))
        {
            pitchSemis = depth;
            pitchLfo = src == srcLfo1 ? 1 : 2;
        }
    }
    {
        const int src = (int) p.num ("OSC", "PWMSrc", 0);
        if (src == srcLfo1 || src == srcLfo2)
        {
            pwmDepth = std::abs (p.num ("OSC", "PWMDpt", 0.0f)) / 50.0f;
            pwmLfo = src == srcLfo1 ? 1 : 2;
        }
    }

    int lfo = 1;
    if (filterLfo > 0 && std::abs (lfoFilterSemis) > 0.5f) lfo = filterLfo;
    else if (pwmLfo > 0 && pwmDepth > 0.01f)               lfo = pwmLfo;
    else if (pitchLfo > 0 && std::abs (pitchSemis) > 0.01f) lfo = pitchLfo;
    const juce::String lfoSection = lfo == 1 ? "LFO1" : "LFO2";

    set ("lfoshape", (float) lfoShape ((int) p.num (lfoSection, "Wave", 1)));
    set ("lforate", lfoRateHz ((int) p.num (lfoSection, "Sync", 1), p.num (lfoSection, "Rate", 0.0f)));
    set ("lfofilter", filterLfo == lfo ? clampf (std::abs (lfoFilterSemis) / 36.0f, 0.0f, 1.0f) : 0.0f);
    set ("lfopitch", pitchLfo == lfo ? clampf (std::abs (pitchSemis) / 2.0f, 0.0f, 1.0f) : 0.0f);
    set ("lfopw", pwmLfo == lfo ? clampf (pwmDepth, 0.0f, 1.0f) : 0.0f);

    // ---- Output -------------------------------------------------------------------------
    const float vol = juce::jmax (1.0f, p.num ("VCA1", "Vol", 50.0f));
    set ("volume", clampf (20.0f * std::log10 (vol / 50.0f) - 6.0f, -36.0f, 6.0f));

    const int panSrc = (int) p.num ("VCA1", "PanSrc", 0);
    const bool voicePan = panSrc == 11 || panSrc == 12 || panSrc == 13; // Random, StackVoice, VoiceMap
    set ("spread", voicePan ? clampf (std::abs (p.num ("VCA1", "PanDpt", 0.0f)) / 100.0f, 0.0f, 1.0f) : 0.25f);

    // ---- FX: Mira's chorus stands in for Diva's chorus slots ---------------------------
    int chorus = 0;
    for (int slot = 1; slot <= 2; ++slot)
    {
        const auto slotName = juce::String (slot);
        const auto enabled = p.globals.find ("FX" + slotName);
        if (enabled != p.globals.end() && enabled->second.getIntValue() == 0)
            continue;
        if (p.str ("FX" + slotName, "Module").startsWith ("Chorus"))
        {
            const auto section = "Chrs" + slotName;
            if (p.num (section, "Wet", 100.0f) < 5.0f)
                continue;
            const int type = (int) p.num (section, "Type", 0);
            const float rate = p.num (section, "Rate", 50.0f);
            chorus = rate > 70.0f ? 3 : (type == 0 ? 1 : 2);
        }
    }
    set ("chorus", (float) chorus);

    out.ok = true;
    return out;
}

Converted convertFile (const juce::File& file)
{
    Converted out;
    if (! file.existsAsFile())
    {
        out.error = "File not found";
        return out;
    }

    juce::MemoryBlock data;
    if (! file.loadFileAsData (data))
    {
        out.error = "Could not read file";
        return out;
    }

    // The readable part is plain ASCII; stop at the binary blob and drop any non-ASCII
    // bytes (from author names etc.) so odd encodings never break parsing.
    std::string ascii;
    ascii.reserve (data.getSize());
    const auto* bytes = static_cast<const char*> (data.getData());
    for (size_t i = 0; i < data.getSize(); ++i)
    {
        if (i + 3 < data.getSize() && bytes[i] == '$' && bytes[i + 1] == '$' && bytes[i + 2] == '$' && bytes[i + 3] == '$')
            break;
        const auto c = (unsigned char) bytes[i];
        ascii.push_back (c < 128 ? (char) c : '?');
    }
    const juce::String text (ascii);

    if (! text.contains ("#AM=Diva"))
    {
        out.error = "Not a Diva preset";
        return out;
    }

    return convert (parse (text), file.getFileNameWithoutExtension().trim());
}
} // namespace diva
