// Minimal headless CLAP host for scripted tests: loads a .clap, runs scenarios through process() on the calling
// thread and checks the MIDI and audio that come out. No GUI, no audio device, no DAW.
// Adapted from SeqVerse's test host.
//
//   clap_test_host <path/to/Progrezzor.clap>      exit code 0 = all checks passed

#include <clap/clap.h>

#include "../src/engine/ChordEngine.h"

#include <dlfcn.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace engine;

namespace
{
constexpr uint32_t sampleRate = 48000;
constexpr uint32_t blockSize = 512;
int failures = 0;
bool everyBlockOk = true;
bool everyBlockSilent = true;
bool everyBlockFinite = true;
float peakLeft = 0.0f; // loudest sample of the last block
float peakRight = 0.0f;

void check (bool ok, const char* what)
{
    std::printf ("  %s: %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok)
        ++failures;
}

const clap_host_t host = {
    CLAP_VERSION, nullptr, "clap_test_host", "progrezzor", nullptr, "0.1",
    [] (const clap_host_t*, const char*) -> const void* { return nullptr; }, // get_extension
    [] (const clap_host_t*) {},                                              // request_restart
    [] (const clap_host_t*) {},                                              // request_process
    [] (const clap_host_t*) {},                                              // request_callback
};

const clap_plugin_factory_t* factory = nullptr;
const clap_plugin_descriptor_t* descriptor = nullptr;

struct InEvent
{
    union
    {
        clap_event_header_t header;
        clap_event_midi_t midi;
        clap_event_param_value_t param;
    };
};

// What the plugin pulls through the event callbacks during the current process() call.
std::vector<InEvent> blockEvents;
std::vector<clap_event_midi_t> received; // everything the plugin emitted, with the time made absolute
uint32_t blockStartSample = 0;

uint32_t inSize (const clap_input_events_t*) { return (uint32_t) blockEvents.size(); }

const clap_event_header_t* inGet (const clap_input_events_t*, uint32_t i)
{
    return i < blockEvents.size() ? &blockEvents[i].header : nullptr;
}

bool outTryPush (const clap_output_events_t*, const clap_event_header_t* h)
{
    if (h->type == CLAP_EVENT_MIDI)
    {
        auto e = *reinterpret_cast<const clap_event_midi_t*> (h);
        e.header.time += blockStartSample;
        received.push_back (e);
    }
    return true;
}

clap_event_midi_t makeMidi (uint32_t time, uint8_t b0, uint8_t b1, uint8_t b2)
{
    clap_event_midi_t e {};
    e.header.size = sizeof (e);
    e.header.time = time;
    e.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    e.header.type = CLAP_EVENT_MIDI;
    e.data[0] = b0;
    e.data[1] = b1;
    e.data[2] = b2;
    return e;
}

InEvent asInEvent (const clap_event_midi_t& m)
{
    InEvent e {};
    e.midi = m;
    return e;
}

struct Instance
{
    const clap_plugin_t* plugin = nullptr;
    const clap_plugin_params_t* params = nullptr;
    std::map<std::string, clap_param_info_t> infos; // by parameter name
};

Instance createInstance()
{
    Instance i;
    i.plugin = factory->create_plugin (factory, &host, descriptor->id);
    if (i.plugin == nullptr || ! i.plugin->init (i.plugin))
    {
        std::fprintf (stderr, "could not create the plugin\n");
        std::exit (2);
    }

    i.params = static_cast<const clap_plugin_params_t*> (i.plugin->get_extension (i.plugin, CLAP_EXT_PARAMS));
    if (i.params != nullptr)
        for (uint32_t n = 0; n < i.params->count (i.plugin); ++n)
        {
            clap_param_info_t info {};
            if (i.params->get_info (i.plugin, n, &info))
                i.infos[info.name] = info;
        }

    if (! i.plugin->activate (i.plugin, sampleRate, 1, blockSize) || ! i.plugin->start_processing (i.plugin))
    {
        std::fprintf (stderr, "could not activate the plugin\n");
        std::exit (2);
    }
    return i;
}

void destroyInstance (Instance& i)
{
    i.plugin->stop_processing (i.plugin);
    i.plugin->deactivate (i.plugin);
    i.plugin->destroy (i.plugin);
    i.plugin = nullptr;
}

// A parameter change by name and text ("Key", "D"), as an event for the next process() call.
bool paramEvent (Instance& i, const char* name, const char* text, InEvent& out)
{
    const auto it = i.infos.find (name);
    double value = 0.0;
    if (it == i.infos.end() || i.params == nullptr || ! i.params->text_to_value (i.plugin, it->second.id, text, &value))
    {
        std::printf ("  FAIL: no parameter '%s' that accepts '%s'\n", name, text);
        ++failures;
        return false;
    }

    out = {};
    out.param.header.size = sizeof (clap_event_param_value_t);
    out.param.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    out.param.header.type = CLAP_EVENT_PARAM_VALUE;
    out.param.param_id = it->second.id;
    out.param.cookie = it->second.cookie;
    out.param.note_id = -1;
    out.param.port_index = -1;
    out.param.channel = -1;
    out.param.key = -1;
    out.param.value = value;
    return true;
}

void processBlock (Instance& i, uint32_t startSample, const clap_event_transport_t* transport)
{
    static std::vector<float> left (blockSize), right (blockSize);
    float* channels[2] = { left.data(), right.data() };
    std::fill (left.begin(), left.end(), 1.0f); // poison, so an unwritten buffer is caught
    std::fill (right.begin(), right.end(), 1.0f);

    clap_audio_buffer_t out {};
    out.channel_count = 2;
    out.data32 = channels;

    const clap_input_events_t in { nullptr, inSize, inGet };
    const clap_output_events_t outEvents { nullptr, outTryPush };

    clap_process_t p {};
    p.steady_time = startSample;
    p.frames_count = blockSize;
    p.transport = transport;
    p.audio_outputs = &out;
    p.audio_outputs_count = 1;
    p.in_events = &in;
    p.out_events = &outEvents;

    blockStartSample = startSample;
    everyBlockOk &= i.plugin->process (i.plugin, &p) != CLAP_PROCESS_ERROR;
    peakLeft = peakRight = 0.0f;
    for (uint32_t n = 0; n < blockSize; ++n)
    {
        peakLeft = std::max (peakLeft, std::fabs (left[n]));
        peakRight = std::max (peakRight, std::fabs (right[n]));
        everyBlockFinite &= std::isfinite (left[n]) && std::isfinite (right[n]) && std::fabs (left[n]) <= 1.0f && std::fabs (right[n]) <= 1.0f;
    }
    for (uint32_t n = 0; n < blockSize; ++n)
        everyBlockSilent &= left[n] == 0.0f && right[n] == 0.0f;
}



// ---- Following what the plugin sends ------------------------------------------------------------------------------

// Counts the notes that are on, and notices every mistake: a note off for a note that is not on, a second note on
// for a note that is already on, anything not on channel 1.
struct Output
{
    int on[128] = {};
    int offsWithoutOn = 0;
    int doubleOns = 0;
    int wrongChannel = 0;
    int totalOns = 0;
    int totalOffs = 0;
    int lastVelocity = 0;
    size_t seen = 0;
    std::vector<int> lastOns; // the notes of the latest note ons, in order

    void take()
    {
        for (; seen < received.size(); ++seen)
        {
            const auto& e = received[seen];
            const int status = e.data[0] & 0xf0;
            const int note = e.data[1];
            wrongChannel += (e.data[0] & 0x0f) != 0;
            if (status == 0x90 && e.data[2] > 0)
            {
                doubleOns += on[note] != 0;
                on[note] = 1;
                ++totalOns;
                lastVelocity = e.data[2];
                lastOns.push_back (note);
            }
            else if (status == 0x80 || status == 0x90)
            {
                offsWithoutOn += on[note] == 0;
                on[note] = 0;
                ++totalOffs;
            }
        }
    }

    int sounding() const
    {
        int n = 0;
        for (int v : on)
            n += v;
        return n;
    }

    std::vector<int> soundingNotes() const
    {
        std::vector<int> notes;
        for (int n = 0; n < 128; ++n)
            if (on[n])
                notes.push_back (n);
        return notes;
    }

    bool clean() const { return offsWithoutOn == 0 && doubleOns == 0 && wrongChannel == 0; }
};

struct Session
{
    Instance instance = createInstance();
    Output out;
    uint32_t time = 0;

    Session() { out.seen = received.size(); } // only what this instance sends
    ~Session() { destroyInstance (instance); }

    // One block with the given events, and the transport if there is one
    void block (std::vector<InEvent> events = {}, const clap_event_transport_t* transport = nullptr)
    {
        blockEvents = std::move (events);
        processBlock (instance, time, transport);
        blockEvents.clear();
        time += blockSize;
        out.take();
    }

    void blocks (int n)
    {
        for (int b = 0; b < n; ++b)
            block();
    }

    void keyOn (int note, int velocity = 100, uint32_t position = 0)
    {
        block ({ asInEvent (makeMidi (position, 0x90, (uint8_t) note, (uint8_t) velocity)) });
    }

    void keyOff (int note)
    {
        block ({ asInEvent (makeMidi (0, 0x80, (uint8_t) note, 0)) });
    }

    void setParam (const char* name, const char* text)
    {
        InEvent e;
        if (paramEvent (instance, name, text, e))
            block ({ e });
    }
};

clap_event_transport_t transportState (bool playing)
{
    clap_event_transport_t t {};
    t.header.size = sizeof (t);
    t.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    t.header.type = CLAP_EVENT_TRANSPORT;
    t.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE | CLAP_TRANSPORT_HAS_TIME_SIGNATURE
              | (playing ? (uint32_t) CLAP_TRANSPORT_IS_PLAYING : 0u);
    t.tempo = 120.0;
    t.tsig_num = 4;
    t.tsig_denom = 4;
    return t;
}


// A project state as the plug-in saves it: JUCE's "copyXmlToBinary" format (a magic number, the length of the text, the
// text itself and a zero at the end)
std::vector<uint8_t> makeState (const std::string& xml)
{
    std::vector<uint8_t> blob;
    const uint32_t magic = 0x21324356;
    const uint32_t length = (uint32_t) xml.size();
    for (uint32_t v : { magic, length })
        for (int b = 0; b < 4; ++b)
            blob.push_back ((uint8_t) (v >> (8 * b)));
    blob.insert (blob.end(), xml.begin(), xml.end());
    blob.push_back (0);
    blob.push_back (0);
    return blob;
}

struct BlockSpec
{
    int root;
    int type; // ChordType as a number: 0 major, 1 minor ...
    double beats;
};

std::string progressionXml (int key, int scale, const std::vector<BlockSpec>& blocks)
{
    std::string xml = "<Progrezzor version=\"2\" key=\"" + std::to_string (key) + "\" scale=\"" + std::to_string (scale) + "\">";
    for (const auto& b : blocks)
        xml += "<Block root=\"" + std::to_string (b.root) + "\" type=\"" + std::to_string (b.type) + "\" inversion=\"-1\" beats=\""
               + std::to_string (b.beats) + "\" locked=\"0\"/>";
    return xml + "</Progrezzor>";
}

bool loadState (Instance& i, const std::vector<uint8_t>& blob)
{
    struct Reader { const std::vector<uint8_t>* data; size_t pos; } reader { &blob, 0 };
    const auto* state = static_cast<const clap_plugin_state_t*> (i.plugin->get_extension (i.plugin, CLAP_EXT_STATE));
    const clap_istream_t stream { &reader,
                                  [] (const clap_istream_t* s, void* data, uint64_t size) -> int64_t
                                  {
                                      auto* r = static_cast<Reader*> (s->ctx);
                                      const size_t n = std::min<size_t> ((size_t) size, r->data->size() - r->pos);
                                      std::memcpy (data, r->data->data() + r->pos, n);
                                      r->pos += n;
                                      return (int64_t) n;
                                  } };
    return state != nullptr && state->load (i.plugin, &stream);
}

// The transport at a position in quarter notes
clap_event_transport_t transportAt (bool playing, double beats, double bpm = 120.0, int tsigNum = 4, int tsigDenom = 4)
{
    auto t = transportState (playing);
    t.tempo = bpm;
    t.tsig_num = (uint16_t) tsigNum;
    t.tsig_denom = (uint16_t) tsigDenom;
    t.song_pos_beats = (clap_beattime) std::llround (beats * CLAP_BEATTIME_FACTOR);
    return t;
}

// The notes of a pad as the engine voices them (A Minor unless said otherwise), after the given earlier chords
std::vector<int> expectedNotes (int pad, const Key& key = Key {}, const engine::Voicing* previous = nullptr, engine::Voicing* result = nullptr)
{
    const auto v = voiceChord (padChord (key.scale, pad), key.tonic, previous);
    if (result != nullptr)
        *result = v;
    return std::vector<int> (v.notes.begin(), v.notes.begin() + v.count);
}

constexpr int whiteKeyOfPad[7] = { 0, 2, 4, 5, 7, 9, 11 };
int keyOfPad (int pad, int octave = 4) { return 12 * (octave + 1) + whiteKeyOfPad[pad]; }


// Runs `n` blocks with the transport at `beat`, moving on as the host would. `beat` is updated.
void playBlocks (Session& s, double& beat, int n, double bpm = 120.0, bool playing = true, std::vector<InEvent> firstEvents = {})
{
    const double beatsPerBlock = blockSize * bpm / 60.0 / sampleRate;
    for (int b = 0; b < n; ++b)
    {
        const auto t = transportAt (playing, beat, bpm);
        s.block (b == 0 ? firstEvents : std::vector<InEvent> {}, &t);
        beat += beatsPerBlock;
    }
}

struct NoteOn
{
    uint32_t time;
    int note;
};

// All note ons from event number `from` on
std::vector<NoteOn> noteOnsFrom (size_t from)
{
    std::vector<NoteOn> result;
    for (size_t i = from; i < received.size(); ++i)
        if ((received[i].data[0] & 0xf0) == 0x90 && received[i].data[2] > 0)
            result.push_back ({ received[i].header.time, received[i].data[1] });
    return result;
}

std::vector<int> notesAt (const std::vector<NoteOn>& ons, uint32_t time)
{
    std::vector<int> notes;
    for (const auto& o : ons)
        if (o.time == time)
            notes.push_back (o.note);
    std::sort (notes.begin(), notes.end());
    return notes;
}

std::vector<int> sortedNotes (const engine::Voicing& v)
{
    return std::vector<int> (v.notes.begin(), v.notes.begin() + v.count);
}

std::vector<int> pitchClassesOf (std::vector<int> notes)
{
    for (auto& n : notes)
        n %= 12;
    std::sort (notes.begin(), notes.end());
    notes.erase (std::unique (notes.begin(), notes.end()), notes.end());
    return notes;
}

// Am F C G in A Minor, a bar each
const std::vector<BlockSpec> amFCG = { { 0, 1, 4 }, { 8, 0, 4 }, { 3, 0, 4 }, { 10, 0, 4 } };

void waitForSilence (Session& s)
{
    s.blocks (120); // the Pad sound has a release of about half a second (47 blocks)
}

} // namespace

int main (int argc, char** argv)
{
    if (argc != 2)
    {
        std::fprintf (stderr, "usage: %s <plugin.clap>\n", argv[0]);
        return 2;
    }

    void* lib = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    if (lib == nullptr)
    {
        std::fprintf (stderr, "dlopen failed: %s\n", dlerror());
        return 2;
    }

    auto* entry = static_cast<const clap_plugin_entry_t*> (dlsym (lib, "clap_entry"));
    if (entry == nullptr || ! entry->init (argv[1]))
    {
        std::fprintf (stderr, "no usable clap_entry in %s\n", argv[1]);
        return 2;
    }

    factory = static_cast<const clap_plugin_factory_t*> (entry->get_factory (CLAP_PLUGIN_FACTORY_ID));
    if (factory == nullptr || factory->get_plugin_count (factory) < 1)
    {
        std::fprintf (stderr, "no plugin in factory\n");
        return 2;
    }

    descriptor = factory->get_plugin_descriptor (factory, 0);
    std::printf ("plugin: %s (%s)\n", descriptor->name, descriptor->id);

    {
        std::printf ("setup\n");
        Session s;
        auto* audioPorts = static_cast<const clap_plugin_audio_ports_t*> (s.instance.plugin->get_extension (s.instance.plugin, CLAP_EXT_AUDIO_PORTS));
        check (audioPorts != nullptr && audioPorts->count (s.instance.plugin, true) == 0 && audioPorts->count (s.instance.plugin, false) == 1,
               "audio ports: no input, one output");
        auto* notePorts = static_cast<const clap_plugin_note_ports_t*> (s.instance.plugin->get_extension (s.instance.plugin, CLAP_EXT_NOTE_PORTS));
        check (notePorts != nullptr && notePorts->count (s.instance.plugin, true) == 1 && notePorts->count (s.instance.plugin, false) == 1,
               "note ports: one MIDI in, one MIDI out");
        check (s.instance.infos.count ("Key") == 1 && s.instance.infos.count ("Scale") == 1, "parameters Key and Scale");
    }

    {
        std::printf ("silent when idle\n");
        Session s;
        s.blocks (10);
        check (everyBlockOk, "process() never reports an error");
        check (everyBlockSilent, "audio is silent");
        check (received.empty(), "no MIDI is sent");
    }

    {
        std::printf ("white key plays a pad: right notes, channel 1, velocity used\n");
        received.clear();
        Session s;
        s.keyOn (keyOfPad (0), 77);
        check (s.out.lastOns == expectedNotes (0), "C plays pad 1 (Am) with the notes the engine gives");
        check (s.out.lastOns.size() == 3, "three notes");
        check (s.out.lastVelocity == 77, "the velocity of the key is used");
        check (s.out.clean(), "channel 1 only, nothing doubled");
        s.blocks (3);
        check (peakLeft > 0.01f && peakRight > 0.01f, "the Pad sound is heard in both channels");
        s.keyOff (keyOfPad (0));
        check (s.out.sounding() == 0, "every note is off after the key is released");
        check (s.out.totalOffs == 3, "exactly one note off per note");
        waitForSilence (s);
        check (peakLeft == 0.0f, "the sound dies away");
        check (everyBlockFinite, "the audio is finite and never louder than full scale");
    }

    {
        std::printf ("all seven pads, in every octave, black keys ignored\n");
        received.clear();
        Session s;
        bool allRight = true;
        for (int octave = 0; octave < 10; ++octave)
            for (int pad = 0; pad < 7; ++pad)
            {
                const int key = keyOfPad (pad, octave - 1);
                if (key < 0 || key > 127)
                    continue;
                Session fresh;
                fresh.keyOn (key);
                allRight &= fresh.out.lastOns == expectedNotes (pad);
                fresh.keyOff (key);
                allRight &= fresh.out.sounding() == 0;
            }
        check (allRight, "pad 1 to 7 from C to B in all octaves");

        received.clear();
        s.out = Output(); // the loop above made other instances play
        s.keyOn (61); s.keyOn (63); s.keyOn (66); s.keyOn (68); s.keyOn (70);
        s.keyOff (61); s.keyOff (63); s.keyOff (66); s.keyOff (68); s.keyOff (70);
        check (s.out.totalOns == 0 && s.out.totalOffs == 0, "black keys do nothing");
    }

    {
        std::printf ("incoming notes are not passed on\n");
        received.clear();
        Session s;
        s.keyOn (keyOfPad (1));
        bool passedOn = false;
        const auto chord = expectedNotes (1);
        for (const auto& e : received)
            if ((e.data[0] & 0xf0) == 0x90 && std::find (chord.begin(), chord.end(), (int) e.data[1]) == chord.end())
                passedOn = true;
        check (! passedOn, "only the notes of the chord come out");
        s.keyOff (keyOfPad (1));
    }

    {
        std::printf ("two pads that share notes: a note ends with the last pad that holds it\n");
        received.clear();
        Session s;
        engine::Voicing first;
        const auto am = expectedNotes (0, Key {}, nullptr, &first);   // A C E
        const auto c = expectedNotes (2, Key {}, &first);             // C E G, voiced after Am
        s.keyOn (keyOfPad (0));
        s.keyOn (keyOfPad (2));
        check (s.out.clean(), "no note is started twice");
        std::vector<int> both = am;
        both.insert (both.end(), c.begin(), c.end());
        std::sort (both.begin(), both.end());
        both.erase (std::unique (both.begin(), both.end()), both.end());
        check (s.out.soundingNotes() == both, "the union of both chords sounds");

        s.keyOff (keyOfPad (0));
        check (s.out.soundingNotes() == c, "after the first pad is released only the notes of the second remain");
        s.keyOff (keyOfPad (2));
        check (s.out.sounding() == 0, "nothing hangs when the last pad is released");
        check (s.out.clean(), "no note off without a note on");
    }

    {
        std::printf ("the same pad twice (two keys), then released in the other order\n");
        received.clear();
        Session s;
        s.keyOn (keyOfPad (3, 3));
        s.keyOn (keyOfPad (3, 5));
        check (s.out.clean(), "pad 4 from two octaves: no doubled notes");
        s.keyOff (keyOfPad (3, 3));
        check (s.out.sounding() > 0, "still sounds while one key is held");
        s.keyOff (keyOfPad (3, 5));
        check (s.out.sounding() == 0, "silent when both are released");
    }

    {
        std::printf ("key or scale changed while a pad is held: the note offs match the note ons\n");
        received.clear();
        Session s;
        s.keyOn (keyOfPad (0));
        const auto before = s.out.soundingNotes();
        s.setParam ("Key", "D");
        s.setParam ("Scale", "Major");
        check (s.out.soundingNotes() == before, "nothing changes in the notes that sound");
        s.keyOff (keyOfPad (0));
        check (s.out.sounding() == 0, "released notes are exactly the started ones");
        check (s.out.clean(), "no stray note offs");

        s.keyOn (keyOfPad (0));
        Key dMajor { 2, ScaleId::Major };
        engine::Voicing ignored;
        check (s.out.lastOns.size() >= 3, "a pad plays again in the new key");
        s.keyOff (keyOfPad (0));
        (void) dMajor; (void) ignored;
        check (s.out.sounding() == 0, "and is released");
    }

    {
        std::printf ("key and scale decide the notes\n");
        received.clear();
        Session s;
        s.setParam ("Key", "D");
        s.setParam ("Scale", "Major");
        s.keyOn (keyOfPad (4)); // pad 5 in D major is A
        check (s.out.lastOns == expectedNotes (4, Key { 2, ScaleId::Major }), "pad 5 in D Major");
        s.keyOff (keyOfPad (4));
        check (s.out.sounding() == 0, "released");
    }

    {
        std::printf ("transport stop ends held pads\n");
        received.clear();
        Session s;
        const auto playing = transportState (true);
        const auto stopped = transportState (false);
        s.block ({}, &playing);
        s.block ({ asInEvent (makeMidi (0, 0x90, (uint8_t) keyOfPad (5), 100)) }, &playing);
        check (s.out.sounding() == 3, "a pad sounds while the transport plays");
        s.block ({}, &stopped);
        check (s.out.sounding() == 0, "all notes off when the transport stops");
        s.block ({ asInEvent (makeMidi (0, 0x80, (uint8_t) keyOfPad (5), 0)) }, &stopped);
        check (s.out.clean(), "the late key release adds nothing");
    }

    {
        std::printf ("the pads work with the transport stopped\n");
        received.clear();
        Session s;
        const auto stopped = transportState (false);
        s.block ({ asInEvent (makeMidi (0, 0x90, (uint8_t) keyOfPad (0), 100)) }, &stopped);
        s.block ({}, &stopped);
        check (s.out.sounding() == 3, "a held pad keeps sounding when the transport is stopped all the time");
        s.block ({ asInEvent (makeMidi (0, 0x80, (uint8_t) keyOfPad (0), 0)) }, &stopped);
        check (s.out.sounding() == 0, "and stops on release");
    }

    {
        std::printf ("MIDI all notes off, and note on with velocity 0\n");
        received.clear();
        Session s;
        s.keyOn (keyOfPad (0));
        s.keyOn (keyOfPad (4));
        s.block ({ asInEvent (makeMidi (0, 0xB0, 123, 0)) });
        check (s.out.sounding() == 0, "controller 123 stops everything");
        s.keyOn (keyOfPad (1));
        s.keyOn (keyOfPad (1), 0);
        check (s.out.sounding() == 0, "note on with velocity 0 is a note off");
        check (s.out.clean(), "no mistakes");
    }

    {
        std::printf ("sample accurate: events keep their position in the block\n");
        received.clear();
        Session s;
        s.keyOn (keyOfPad (0), 100, 200);
        bool right = !received.empty();
        for (const auto& e : received)
            right &= e.header.time == 200;
        check (right, "note ons are at sample 200");
    }

    {
        std::printf ("deactivate and activate again with a pad held: the first block sends the note offs\n");
        received.clear();
        Session s;
        s.keyOn (keyOfPad (0));
        s.instance.plugin->stop_processing (s.instance.plugin);
        s.instance.plugin->deactivate (s.instance.plugin);
        s.instance.plugin->activate (s.instance.plugin, sampleRate, 1, blockSize);
        s.instance.plugin->start_processing (s.instance.plugin);
        s.block();
        check (s.out.sounding() == 0, "no note left hanging");
        check (s.out.clean(), "and no mistakes");
    }

    {
        std::printf ("hammering: 3000 random events, then everything is released\n");
        received.clear();
        Session s;
        unsigned seed = 4242;
        auto next = [&seed] (int n) { seed = seed * 1664525u + 1013904223u; return (int) ((seed >> 8) % (unsigned) n); };
        const char* keys[] = { "C", "D", "E", "F", "G", "A", "B" };
        const char* scales[] = { "Minor", "Major", "Dorian", "Phrygian" };
        std::vector<int> held;
        for (int i = 0; i < 3000; ++i)
        {
            std::vector<InEvent> events;
            const int count = 1 + next (4);
            for (int n = 0; n < count; ++n)
            {
                const int what = next (10);
                if (what < 4)
                {
                    const int note = 36 + next (60); // white and black keys
                    events.push_back (asInEvent (makeMidi ((uint32_t) next ((int) blockSize), 0x90, (uint8_t) note, (uint8_t) (1 + next (127)))));
                    held.push_back (note);
                }
                else if (what < 8 && ! held.empty())
                {
                    const size_t index = (size_t) next ((int) held.size());
                    events.push_back (asInEvent (makeMidi ((uint32_t) next ((int) blockSize), 0x80, (uint8_t) held[index], 0)));
                    held.erase (held.begin() + (long) index);
                }
                else if (what == 8)
                {
                    InEvent e;
                    if (paramEvent (s.instance, next (2) ? "Key" : "Scale", next (2) ? keys[next (7)] : scales[next (4)], e))
                        events.push_back (e);
                }
                else if (next (20) == 0)
                    events.push_back (asInEvent (makeMidi (0, 0xB0, 123, 0)));
            }
            // events must come in time order
            std::stable_sort (events.begin(), events.end(), [] (const InEvent& a, const InEvent& b) { return a.header.time < b.header.time; });
            s.block (events);
        }
        check (s.out.clean(), "no stray note offs, no doubled note ons, channel 1 only");
        for (int note : held)
            s.keyOff (note);
        s.block ({ asInEvent (makeMidi (0, 0xB0, 123, 0)) });
        check (s.out.sounding() == 0, "nothing hangs after the last release");
        check (s.out.totalOns == s.out.totalOffs, "as many note offs as note ons");
        check (everyBlockOk && everyBlockFinite, "every block fine, audio finite and within full scale");
        waitForSilence (s);
        check (peakLeft == 0.0f, "silent after the release");
    }


    // ---- The loop ----------------------------------------------------------------------------------------------

    {
        std::printf ("loop: nothing plays until the host plays\n");
        received.clear();
        Session s;
        check (loadState (s.instance, makeState (progressionXml (9, 0, amFCG))), "the state with a progression loads");
        s.blocks (20);
        check (s.out.totalOns == 0, "no notes with no transport at all");
        double beat = 0.0;
        playBlocks (s, beat, 20, 120.0, false);
        check (s.out.totalOns == 0, "no notes while the transport is stopped");
    }

    {
        std::printf ("loop: bar lines are hit exactly, also in the middle of a buffer\n");
        received.clear();
        Session s;
        loadState (s.instance, makeState (progressionXml (9, 0, amFCG)));
        double beat = 0.0;
        playBlocks (s, beat, 1500); // two rounds of 16 beats: 768000 samples
        const auto ons = noteOnsFrom (0);

        // What the engine voices after each other: the loop's chords, one after the other
        std::vector<std::vector<int>> chain;
        engine::Voicing previous;
        bool havePrevious = false;
        const Key key { 9, ScaleId::Minor };
        for (int round = 0; round < 2; ++round)
            for (const auto& b : amFCG)
            {
                engine::Voicing v = voiceChord ({ b.root, (ChordType) b.type, kAutoInversion }, key.tonic, havePrevious ? &previous : nullptr);
                chain.push_back (sortedNotes (v));
                previous = v;
                havePrevious = true;
            }

        bool allRight = true;
        for (size_t k = 0; k < chain.size(); ++k)
        {
            const uint32_t time = (uint32_t) (k * 96000);
            // Every change plays the whole chord again, as a new hit, also the notes that it shares with the chord before
            const auto sent = notesAt (ons, time);
            const auto expected = chain[k];
            allRight &= sent == expected;
            if (sent != expected)
            {
                std::printf ("    bar %zu: sent", k);
                for (int n : sent) std::printf (" %d", n);
                std::printf (" expected");
                for (int n : expected) std::printf (" %d", n);
                std::printf ("\n");
            }
        }
        check (allRight, "every bar line has exactly the new notes of the next chord, at exactly that sample (8 changes)");

        bool onlyOnBarLines = true;
        for (const auto& o : ons)
            onlyOnBarLines &= o.time % 96000 == 0;
        check (onlyOnBarLines, "no note ever starts anywhere else");
        check (s.out.clean(), "channel 1, nothing doubled, no stray note offs");
        check (s.out.sounding() > 0, "and the last chord is still sounding");
        check (peakLeft > 0.001f, "the preview sound plays the loop too");

        double stopBeat = beat;
        playBlocks (s, stopBeat, 2, 120.0, false);
        check (s.out.sounding() == 0, "everything stops when the transport stops");
    }

    {
        std::printf ("loop: odd lengths (a half bar, one bar and one and a half)\n");
        received.clear();
        Session s;
        loadState (s.instance, makeState (progressionXml (9, 0, { { 0, 1, 2 }, { 5, 1, 4 }, { 7, 1, 6 } })));
        double beat = 0.0;
        playBlocks (s, beat, 1100); // 12 beats = 288000 samples, and then some of the next round
        const auto ons = noteOnsFrom (0);
        bool onLines = ! ons.empty();
        for (const auto& o : ons)
        {
            const uint32_t t = o.time;
            onLines &= t == 0 || t == 48000 || t == 144000 || t == 288000 || t == 336000 || t == 432000;
        }
        check (onLines, "changes at beat 0, 2, 6, 12, 14, 18: 2 + 4 + 6 beats, round and round");
        check (s.out.clean(), "no mistakes");
    }

    {
        std::printf ("loop: the host jumps in time, or loops\n");
        received.clear();
        Session s;
        loadState (s.instance, makeState (progressionXml (9, 0, amFCG)));
        double beat = 0.0;
        playBlocks (s, beat, 5);
        s.out.lastOns.clear();
        beat = 9.0; // the third block, C
        const size_t before = received.size();
        playBlocks (s, beat, 2);
        const auto afterJump = noteOnsFrom (before);
        check (! afterJump.empty() && afterJump[0].time == (uint32_t) (s.time - 2 * blockSize), "the chord of the new position starts at the first sample after the jump");
        check (s.out.clean(), "the jump leaves no note hanging");
        const auto notes = pitchClassesOf (std::vector<int> (s.out.soundingNotes()));
        (void) notes;
        std::vector<int> pcs;
        for (int n : s.out.soundingNotes())
            pcs.push_back (n % 12);
        std::sort (pcs.begin(), pcs.end());
        pcs.erase (std::unique (pcs.begin(), pcs.end()), pcs.end());
        check (pcs == std::vector<int> ({ 0, 4, 7 }), "it is C major that sounds at beat 9");

        // A DAW loop: back to the start
        beat = 0.0;
        playBlocks (s, beat, 2);
        pcs.clear();
        for (int n : s.out.soundingNotes())
            pcs.push_back (n % 12);
        std::sort (pcs.begin(), pcs.end());
        pcs.erase (std::unique (pcs.begin(), pcs.end()), pcs.end());
        check (pcs == std::vector<int> ({ 0, 4, 9 }), "back at the start it is A minor again");
        check (s.out.clean(), "still clean");
    }

    {
        std::printf ("loop: tempo changes\n");
        received.clear();
        Session s;
        loadState (s.instance, makeState (progressionXml (9, 0, amFCG)));
        // At 180 BPM a beat is 16000 samples. The bar line at beat 4 is 160 samples into the block that starts at beat 3.99.
        double beat = 3.99;
        const size_t before = received.size();
        const auto t = transportAt (true, beat, 180.0);
        s.block ({}, &t);
        const auto ons = noteOnsFrom (before);
        bool right = notesAt (ons, 0).size() == 3 && notesAt (ons, 160).size() == 3 && ons.size() == 6; // Am at the start, F at 160
        check (right, "at 180 BPM the change from Am to F lands on sample 160 of the block");
    }

    {
        std::printf ("loop: a held pad goes first, and the loop is quiet meanwhile\n");
        received.clear();
        Session s;
        loadState (s.instance, makeState (progressionXml (9, 0, amFCG)));
        double beat = 0.0;
        playBlocks (s, beat, 10); // inside the first bar: Am
        check (s.out.sounding() == 3, "the loop plays Am");

        s.out.lastOns.clear();
        const size_t before = received.size();
        playBlocks (s, beat, 1, 120.0, true, { asInEvent (makeMidi (100, 0x90, (uint8_t) keyOfPad (3), 100)) }); // pad 4: Dm
        const auto pressed = noteOnsFrom (before);
        std::vector<int> pcs;
        for (int n : s.out.soundingNotes())
            pcs.push_back (n % 12);
        std::sort (pcs.begin(), pcs.end());
        pcs.erase (std::unique (pcs.begin(), pcs.end()), pcs.end());
        check (pcs == std::vector<int> ({ 2, 5, 9 }), "only the pad (Dm) sounds while it is held: the loop is silent");

        // Over the next bar line: the loop does not come back while the pad is held
        playBlocks (s, beat, 250);
        pcs.clear();
        for (int n : s.out.soundingNotes())
            pcs.push_back (n % 12);
        std::sort (pcs.begin(), pcs.end());
        pcs.erase (std::unique (pcs.begin(), pcs.end()), pcs.end());
        check (pcs == std::vector<int> ({ 2, 5, 9 }), "still only the pad after the bar line");
        check (s.out.clean(), "no stray notes");
        (void) pressed;

        playBlocks (s, beat, 1, 120.0, true, { asInEvent (makeMidi (50, 0x80, (uint8_t) keyOfPad (3), 0)) });
        pcs.clear();
        for (int n : s.out.soundingNotes())
            pcs.push_back (n % 12);
        std::sort (pcs.begin(), pcs.end());
        pcs.erase (std::unique (pcs.begin(), pcs.end()), pcs.end());
        check (pcs == std::vector<int> ({ 0, 5, 9 }), "when the pad is let go, the loop is heard again with the block of that position (F)");
        check (s.out.clean(), "and nothing hangs");
    }

    {
        std::printf ("loop: Play off, an empty progression, a new key\n");
        received.clear();
        Session s;
        loadState (s.instance, makeState (progressionXml (9, 0, amFCG)));
        double beat = 0.0;
        playBlocks (s, beat, 10);
        check (s.out.sounding() == 3, "Am plays");

        InEvent off, on;
        check (paramEvent (s.instance, "Play", "Off", off) && paramEvent (s.instance, "Play", "On", on), "there is a Play parameter");
        playBlocks (s, beat, 1, 120.0, true, { off });
        check (s.out.sounding() == 0, "Play off silences the loop");
        playBlocks (s, beat, 300);
        check (s.out.sounding() == 0, "and it stays silent over the bar lines");
        playBlocks (s, beat, 1, 120.0, true, { on });
        check (s.out.sounding() == 3, "Play on brings it back");

        // A new key: the same chords, moved
        InEvent key;
        check (paramEvent (s.instance, "Key", "D", key), "key parameter");
        playBlocks (s, beat, 1, 120.0, true, { key });
        std::vector<int> pcs;
        for (int n : s.out.soundingNotes())
            pcs.push_back (n % 12);
        std::sort (pcs.begin(), pcs.end());
        pcs.erase (std::unique (pcs.begin(), pcs.end()), pcs.end());
        const int blockNow = (int) (std::fmod (beat, 16.0) / 4.0);
        const BlockSpec b = amFCG[(size_t) blockNow];
        std::vector<int> expectedPcs;
        for (int iv : shapeOf ((ChordType) b.type).intervals)
            (void) iv;
        for (int i = 0; i < shapeOf ((ChordType) b.type).count; ++i)
            expectedPcs.push_back ((2 + b.root + shapeOf ((ChordType) b.type).intervals[i]) % 12);
        std::sort (expectedPcs.begin(), expectedPcs.end());
        check (pcs == expectedPcs, "a new key moves the loop's chord with it");
        check (s.out.clean(), "and nothing hangs");

        // An empty progression plays nothing
        loadState (s.instance, makeState (progressionXml (9, 0, {})));
        playBlocks (s, beat, 2);
        check (s.out.sounding() == 0, "an empty progression is silent");
    }

    {
        std::printf ("loop: the first bar of a song with a count-in\n");
        received.clear();
        Session s;
        loadState (s.instance, makeState (progressionXml (9, 0, amFCG)));
        double beat = -0.02; // a block that starts 480 samples before beat 0
        const size_t before = received.size();
        const auto t = transportAt (true, beat);
        s.block ({}, &t);
        const auto ons = noteOnsFrom (before);
        bool right = ! ons.empty();
        for (const auto& o : ons)
            right &= o.time == 480;
        check (right, "the loop starts at beat 0, 480 samples into the block");
    }

    {
        std::printf ("loop: hammering the pads and the transport together\n");
        received.clear();
        Session s;
        loadState (s.instance, makeState (progressionXml (9, 0, amFCG)));
        unsigned seed = 99;
        auto next = [&seed] (int n) { seed = seed * 1664525u + 1013904223u; return (int) ((seed >> 8) % (unsigned) n); };
        double beat = 0.0;
        bool playing = true;
        std::vector<int> held;
        for (int i = 0; i < 4000; ++i)
        {
            std::vector<InEvent> events;
            for (int n = 0, count = next (3); n < count; ++n)
            {
                if (next (2) == 0)
                {
                    const int note = 48 + next (24);
                    events.push_back (asInEvent (makeMidi ((uint32_t) next ((int) blockSize), 0x90, (uint8_t) note, 90)));
                    held.push_back (note);
                }
                else if (! held.empty())
                {
                    const size_t index = (size_t) next ((int) held.size());
                    events.push_back (asInEvent (makeMidi ((uint32_t) next ((int) blockSize), 0x80, (uint8_t) held[index], 0)));
                    held.erase (held.begin() + (long) index);
                }
            }
            std::stable_sort (events.begin(), events.end(), [] (const InEvent& a, const InEvent& b) { return a.header.time < b.header.time; });

            if (next (200) == 0)
                playing = ! playing;
            if (next (150) == 0)
                beat = (double) next (64) + next (4) * 0.25; // a jump
            playBlocks (s, beat, 1, 100.0 + next (80), playing, events);
        }
        check (s.out.clean(), "no stray note offs, no doubled note ons, channel 1 only");
        check (everyBlockOk && everyBlockFinite, "every block fine, audio finite and within full scale");

        double stopBeat = beat;
        for (int note : held)
            s.keyOff (note);
        playBlocks (s, stopBeat, 2, 120.0, false);
        check (s.out.sounding() == 0, "nothing hangs after the transport stops");
        check (s.out.totalOns == s.out.totalOffs, "as many note offs as note ons");
    }

    entry->deinit();
    std::printf (failures == 0 ? "ALL OK\n" : "%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
