// Copyright (C) 2026 Henrik Juhl. Part of Progrezzor, licensed under the GNU AGPLv3 (see LICENSE).
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

// The preview sounds: small built-in synths, no samples. Pad (the first): two slightly detuned saw waves (one in each ear)
// and a soft sine an octave below, through a low-pass filter, with a slow attack and a long release. Then Strings (a wider,
// slower saw ensemble), Keys (two-operator FM), Organ (a few sine drawbars), Stab (short and bright) and Pluck (a saw
// whose brightness dies away). They are only there so that you hear what you play; the MIDI is the real output.
// No JUCE, no allocation: safe on the audio thread.
class PadSynth
{
public:
    enum Voice_ { Pad, Strings, Keys, Organ, Stab, Pluck, numVoices, Off = numVoices }; // Off: no preview sound at all

    // The voice that new notes get. Notes that already sound keep theirs until they end; Off lets them fade out quickly.
    void setVoice (int index)
    {
        const int wanted = std::max (0, std::min ((int) Off, index));
        if (wanted == Off && voiceType != Off)
            allNotesOff();
        voiceType = wanted;
    }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        attackStep = (float) (1.0 / (0.08 * sampleRate));
        releaseStep = (float) (1.0 / (0.6 * sampleRate));
        fastReleaseStep = (float) (1.0 / (0.01 * sampleRate));
        reset();
    }

    void reset()
    {
        for (auto& v : voices)
            v = Voice();
        restrikes = freshStrikes = 0;
    }

    void noteOn (int note, int velocity)
    {
        if (note < 0 || note > 127 || voiceType == Off)
            return;
        auto& v = voices[(size_t) note];
        const bool wasSilent = ! v.active;
        v.active = true;
        v.gate = true;
        v.type = voiceType;
        const float decayBefore = v.decay; // a note that rings keeps how far it has faded (see below)
        v.decay = 1.0f;
        v.releaseRate = releaseStep;
        v.attackRate = attackStep;
        v.decayRate = 1.0f;
        v.gain = 0.09f * (0.25f + 0.75f * (float) velocity / 127.0f);
        v.increment = 440.0 * std::pow (2.0, (note - 69) / 12.0) / sampleRate;
        // The cutoff follows the pitch, so that high notes are not duller than low ones
        double cutoff = std::min (0.45 * sampleRate, 700.0 + 5.0 * v.increment * sampleRate);
        const float vel = (float) velocity / 127.0f;
        switch (v.type)
        {
            case Strings:
                v.attackRate = (float) (1.0 / (0.30 * sampleRate));
                v.releaseRate = (float) (1.0 / (0.9 * sampleRate));
                v.gain = 0.09f * (0.25f + 0.75f * vel);
                cutoff = std::min (0.45 * sampleRate, 2600.0 + 8.0 * v.increment * sampleRate); // brighter than the Soft Pad
                break;
            case Keys:
                v.attackRate = (float) (1.0 / (0.004 * sampleRate));
                v.releaseRate = (float) (1.0 / (0.35 * sampleRate));
                v.decayRate = (float) std::exp (-1.0 / (1.1 * sampleRate)); // the bell fades while the key is held
                v.gain = 0.16f * (0.25f + 0.75f * vel);
                v.modIndex = 1.2f + 2.2f * vel;
                break;
            case Organ:
                v.attackRate = (float) (1.0 / (0.012 * sampleRate));
                v.releaseRate = (float) (1.0 / (0.07 * sampleRate));
                v.gain = 0.075f;
                break;
            case Stab:
                v.attackRate = (float) (1.0 / (0.003 * sampleRate));
                v.releaseRate = (float) (1.0 / (0.12 * sampleRate));
                v.decayRate = (float) std::exp (-1.0 / (0.22 * sampleRate));
                v.gain = 0.11f * (0.25f + 0.75f * vel);
                cutoff = std::min (0.45 * sampleRate, 2600.0 + 8.0 * v.increment * sampleRate);
                break;
            case Pluck:
                v.attackRate = (float) (1.0 / (0.002 * sampleRate));
                v.releaseRate = (float) (1.0 / (0.25 * sampleRate));
                v.decayRate = (float) std::exp (-1.0 / (0.75 * sampleRate));
                v.gain = 0.13f * (0.25f + 0.75f * vel);
                cutoff = std::min (0.45 * sampleRate, 3200.0 + 8.0 * v.increment * sampleRate);
                break;
            default: break;
        }
        v.filterCoefficient = (float) (1.0 - std::exp (-2.0 * 3.14159265358979 * cutoff / sampleRate));
        if (wasSilent)
        {
            v.phaseLeft = 0.13;
            v.phaseRight = 0.61;
            v.phaseSub = 0.0;
            v.phaseExtra = 0.37;
            v.level = 0.0f;
            v.dipping = false;
            v.filterLeft = v.filterRight = 0.0f;
        }
        else
        {
            // The same note struck again while it still rings. Whether it dips is decided in render(), when all notes that
            // start together are known: the same chord again dips (so that the second hit is heard), but in a chord change the
            // notes the two chords share just carry on.
            v.decay = decayBefore; // never a jump: the bell is struck anew in render(), after the level has gone down
            v.restruck = true;
            v.dipFrom = v.level;
            ++restrikes;
            return;
        }
        ++freshStrikes;
    }

    void noteOff (int note)
    {
        if (note >= 0 && note <= 127)
            voices[(size_t) note].gate = false;
    }

    // A quick fade, for stop and bypass
    void allNotesOff()
    {
        for (auto& v : voices)
        {
            v.gate = false;
            v.releaseRate = fastReleaseStep;
        }
    }

    // Adds the sound of the voices to samples [start, end) of the two channels
    void render (float* left, float* right, int start, int end)
    {
        if (restrikes > 0 || freshStrikes > 0)
        {
            // Notes that started together, at the same sample, are all known now (see noteOn)
            for (auto& v : voices)
                if (v.restruck)
                {
                    v.restruck = false;
                    if (freshStrikes == 0) // nothing new: the same chord (or note) again
                    {
                        // The slow sounds dip and swell. The short ones (Keys, Stab, Pluck) go down to nothing quickly and are struck
                        // anew there. Always a ramp, never a jump (a jump clicks).
                        const bool short_ = v.decayRate < 1.0f;
                        v.dipTo = short_ ? 0.0f : v.dipFrom * 0.35f;
                        v.dipRate = (v.dipFrom - v.dipTo) / (float) ((short_ ? 0.006 : 0.015) * sampleRate);
                        v.dipping = v.dipRate > 0.0f;
                        v.strikeAfterDip = short_;
                        if (short_ && ! v.dipping) // nothing sounds: strike at once
                            v.decay = 1.0f;
                    }
                }
            restrikes = freshStrikes = 0;
        }
        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            for (int i = start; i < end; ++i)
            {
                if (v.dipping)
                {
                    v.level -= v.dipRate;
                    if (v.level <= v.dipTo || ! v.gate)
                    {
                        v.dipping = false;
                        if (v.strikeAfterDip && v.gate)
                            v.decay = 1.0f; // the bell is struck anew, from nothing
                        v.strikeAfterDip = false;
                    }
                }
                else if (v.gate)
                    v.level = std::min (1.0f, v.level + v.attackRate);
                else
                {
                    v.level -= v.releaseRate;
                    if (v.level <= 0.0f)
                    {
                        v.level = 0.0f;
                        v.active = false;
                        break;
                    }
                }

                v.decay *= v.decayRate;
                if (v.decay < 0.0002f && v.decayRate < 1.0f)
                {
                    v.active = false; // the sound has died away by itself
                    break;
                }

                float outLeft = 0.0f, outRight = 0.0f;
                const double inc = v.increment;
                switch (v.type)
                {
                    case Strings: // three saws a side, spread, and slowly moving
                    {
                        const float a1 = saw (v.phaseLeft, inc * 0.994), a2 = saw (v.phaseExtra, inc * 1.0);
                        const float b1 = saw (v.phaseRight, inc * 1.006), b2 = saw (v.phaseSub, inc * 1.0);
                        v.filterLeft += v.filterCoefficient * (0.6f * a1 + 0.4f * a2 - v.filterLeft);
                        v.filterRight += v.filterCoefficient * (0.6f * b1 + 0.4f * b2 - v.filterRight);
                        outLeft = v.filterLeft;
                        outRight = v.filterRight;
                        advance (v.phaseLeft, inc * 0.994);
                        advance (v.phaseRight, inc * 1.006);
                        advance (v.phaseExtra, inc * 1.0);
                        advance (v.phaseSub, inc * 1.0);
                        break;
                    }
                    case Keys: // two-operator FM: a sine carrier, a sine modulator at the same pitch whose strength fades
                    {
                        const float mod = (float) std::sin (6.283185307179586 * v.phaseSub) * v.modIndex * v.decay;
                        const float tone = (float) std::sin (6.283185307179586 * v.phaseLeft + mod);
                        const float bell = (float) std::sin (6.283185307179586 * v.phaseExtra) * 0.12f * v.decay;
                        outLeft = outRight = tone + bell;
                        advance (v.phaseLeft, inc);
                        advance (v.phaseSub, inc);
                        advance (v.phaseExtra, inc * 4.0);
                        break;
                    }
                    case Organ: // drawbars: the note, an octave up, a fifth above that, two octaves up, and one octave down
                    {
                        const float p = (float) v.phaseLeft;
                        const float tone = (float) (std::sin (6.283185307179586 * p) + 0.7 * std::sin (12.566370614359172 * p)
                                                    + 0.35 * std::sin (18.84955592153876 * p) + 0.25 * std::sin (25.132741228718345 * p)
                                                    + 0.6 * std::sin (6.283185307179586 * v.phaseSub));
                        outLeft = outRight = tone * 0.45f;
                        advance (v.phaseLeft, inc);
                        advance (v.phaseSub, inc * 0.5);
                        break;
                    }
                    case Stab: // two detuned saws that start bright and close quickly
                    case Pluck:
                    {
                        const float sawLeft = saw (v.phaseLeft, inc * 0.998);
                        const float sawRight = saw (v.phaseRight, inc * 1.002);
                        // The filter opens with the decay: bright at the start, soft at the end
                        const float open = 0.25f + 0.75f * v.decay;
                        const float k = v.filterCoefficient * open;
                        v.filterLeft += k * (sawLeft - v.filterLeft);
                        v.filterRight += k * (sawRight - v.filterRight);
                        outLeft = v.filterLeft;
                        outRight = v.filterRight;
                        advance (v.phaseLeft, inc * 0.998);
                        advance (v.phaseRight, inc * 1.002);
                        break;
                    }
                    default: // Pad
                    {
                        const float sawLeft = saw (v.phaseLeft, inc * 0.9965);
                        const float sawRight = saw (v.phaseRight, inc * 1.0035);
                        const float sub = (float) std::sin (6.283185307179586 * v.phaseSub) * 0.5f;
                        v.filterLeft += v.filterCoefficient * (sawLeft + sub - v.filterLeft);
                        v.filterRight += v.filterCoefficient * (sawRight + sub - v.filterRight);
                        outLeft = v.filterLeft;
                        outRight = v.filterRight;
                        advance (v.phaseLeft, inc * 0.9965);
                        advance (v.phaseRight, inc * 1.0035);
                        advance (v.phaseSub, inc * 0.5);
                        break;
                    }
                }

                const float amount = v.gain * v.level * (v.decayRate < 1.0f ? v.decay : 1.0f);
                left[i] += outLeft * amount;
                right[i] += outRight * amount;
            }
        }
    }

    bool isSilent() const
    {
        for (const auto& v : voices)
            if (v.active)
                return false;
        return true;
    }

private:
    struct Voice
    {
        bool active = false;
        bool gate = false;
        float level = 0.0f;
        float gain = 0.0f;
        float releaseRate = 0.0f;
        float filterCoefficient = 0.1f;
        float filterLeft = 0.0f, filterRight = 0.0f;
        double increment = 0.0;
        double phaseLeft = 0.0, phaseRight = 0.0, phaseSub = 0.0, phaseExtra = 0.0;
        int type = 0;
        float decay = 1.0f, decayRate = 1.0f; // sounds that die away by themselves (Keys, Stab, Pluck)
        float attackRate = 0.001f;
        float modIndex = 1.5f;
        bool dipping = false; // a note struck again while it rings: the level glides down first, then swells
        float dipTo = 0.0f, dipRate = 0.0f, dipFrom = 0.0f;
        bool strikeAfterDip = false; // Keys, Stab and Pluck: the decay starts over when the dip has reached nothing
        bool restruck = false; // struck again while it rang, in the batch that render() has not seen yet
    };

    // A saw wave with the PolyBLEP correction at its jump, so that it does not alias much
    static float saw (double phase, double increment)
    {
        float value = (float) (2.0 * phase - 1.0);
        if (phase < increment)
        {
            const double t = phase / increment;
            value -= (float) (t + t - t * t - 1.0);
        }
        else if (phase > 1.0 - increment)
        {
            const double t = (phase - 1.0) / increment;
            value -= (float) (t * t + t + t + 1.0);
        }
        return value;
    }

    static void advance (double& phase, double increment)
    {
        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;
    }

    int voiceType = Pad;
    int restrikes = 0, freshStrikes = 0; // notes struck since the last render, already ringing / from silence
    double sampleRate = 44100.0;
    float attackStep = 0.001f, releaseStep = 0.001f, fastReleaseStep = 0.01f;
    std::array<Voice, 128> voices {}; // one voice per MIDI note: a note is never on twice
};
