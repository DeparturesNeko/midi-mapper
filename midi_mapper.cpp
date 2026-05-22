/*
 * MIDI Mapper v1.1 - MIDI to Keyboard/Mouse Mapper
 * Compile: g++ -O2 -std=c++17 -o midi_mapper.exe midi_mapper.cpp -lwinmm -static
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <mutex>
#include <algorithm>
#include <atomic>
#include <thread>

#pragma comment(lib, "winmm.lib")
using namespace std;

// ==================== Note Mode ====================
enum NoteMode { MODE_HOLD = 0, MODE_TOGGLE, MODE_TAP };

static string noteModeName(NoteMode m) {
    switch (m) {
        case MODE_HOLD: return "hold";
        case MODE_TOGGLE: return "toggle";
        case MODE_TAP: return "tap";
    }
    return "hold";
}

// ==================== Note/CC Names ====================
static const char* NOTE_NAMES[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static string getNoteName(int n) {
    if (n < 0 || n > 127) return "?";
    return string(NOTE_NAMES[n % 12]) + to_string(n / 12 - 1);
}

static string getStdCCName(int cc) {
    switch (cc) {
        case 0: return "Bank Select"; case 1: return "Mod Wheel";
        case 2: return "Breath"; case 4: return "Foot";
        case 7: return "Volume"; case 10: return "Pan";
        case 11: return "Expression"; case 64: return "Sustain";
        case 65: return "Portamento"; case 66: return "Sostenuto";
        default: return "";
    }
}

// ==================== Action Types ====================
enum ActionType {
    ACT_NONE=0, ACT_KEY, ACT_KEY_COMBO,
    ACT_MOUSE_L, ACT_MOUSE_R, ACT_MOUSE_M,
    ACT_MOUSE_MX, ACT_MOUSE_MY, ACT_MOUSE_SCROLL
};

struct Action {
    ActionType type = ACT_NONE;
    vector<WORD> keys;
    string name;
};

// ==================== VK Code Map ====================
static map<string, WORD> buildVKMap() {
    map<string, WORD> m;
    for (char c = 'A'; c <= 'Z'; c++) m["KEY_" + string(1,c)] = (WORD)c;
    for (char c = '0'; c <= '9'; c++) m["KEY_" + string(1,c)] = (WORD)c;
    for (int i = 1; i <= 12; i++) m["KEY_F" + to_string(i)] = VK_F1+i-1;
    m["KEY_SPACE"]=VK_SPACE; m["KEY_ENTER"]=VK_RETURN;
    m["KEY_ESC"]=VK_ESCAPE; m["KEY_TAB"]=VK_TAB;
    m["KEY_BACKSPACE"]=VK_BACK; m["KEY_DELETE"]=VK_DELETE;
    m["KEY_INSERT"]=VK_INSERT; m["KEY_HOME"]=VK_HOME;
    m["KEY_END"]=VK_END; m["KEY_PAGEUP"]=VK_PRIOR; m["KEY_PAGEDOWN"]=VK_NEXT;
    m["KEY_UP"]=VK_UP; m["KEY_DOWN"]=VK_DOWN;
    m["KEY_LEFT"]=VK_LEFT; m["KEY_RIGHT"]=VK_RIGHT;
    m["KEY_SHIFT"]=VK_SHIFT; m["KEY_CTRL"]=VK_CONTROL; m["KEY_ALT"]=VK_MENU;
    m["KEY_LSHIFT"]=VK_LSHIFT; m["KEY_RSHIFT"]=VK_RSHIFT;
    m["KEY_LCTRL"]=VK_LCONTROL; m["KEY_RCTRL"]=VK_RCONTROL;
    m["KEY_LALT"]=VK_LMENU; m["KEY_RALT"]=VK_RMENU;
    return m;
}
static map<string, WORD> VK_MAP = buildVKMap();

// ==================== Input Simulation ====================
namespace InputSim {
    void pressKey(WORD vk) {
        INPUT inp = {}; inp.type = INPUT_KEYBOARD;
        inp.ki.wVk = vk; SendInput(1, &inp, sizeof(INPUT));
    }
    void releaseKey(WORD vk) {
        INPUT inp = {}; inp.type = INPUT_KEYBOARD;
        inp.ki.wVk = vk; inp.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &inp, sizeof(INPUT));
    }
    void pressCombo(const vector<WORD>& keys) {
        for (auto vk : keys) pressKey(vk);
    }
    void releaseCombo(const vector<WORD>& keys) {
        for (int i=(int)keys.size()-1; i>=0; i--) releaseKey(keys[i]);
    }
    void mouseDown(DWORD btn) {
        INPUT inp = {}; inp.type = INPUT_MOUSE; inp.mi.dwFlags = btn;
        SendInput(1, &inp, sizeof(INPUT));
    }
    void mouseUp(DWORD btn) {
        INPUT inp = {}; inp.type = INPUT_MOUSE; inp.mi.dwFlags = btn;
        SendInput(1, &inp, sizeof(INPUT));
    }
    void mouseMove(int dx, int dy) {
        INPUT inp = {}; inp.type = INPUT_MOUSE;
        inp.mi.dx = dx; inp.mi.dy = dy; inp.mi.dwFlags = MOUSEEVENTF_MOVE;
        SendInput(1, &inp, sizeof(INPUT));
    }
    void mouseScroll(int delta) {
        INPUT inp = {}; inp.type = INPUT_MOUSE;
        inp.mi.dwFlags = MOUSEEVENTF_WHEEL; inp.mi.mouseData = delta;
        SendInput(1, &inp, sizeof(INPUT));
    }
}

// ==================== Velocity Handler (placeholder) ====================
// Called on every Note On event. Reserved for future velocity-based features
// e.g. velocity threshold filtering, velocity-to-mouse-distance, dynamic layers
static void onVelocityEvent(int note, int velocity, const string& noteName) {
    (void)note;
    (void)velocity;
    (void)noteName;
    // TODO: implement velocity-based logic here
}

// ==================== Mapping Manager ====================
class MappingManager {
public:
    map<int, Action> noteMap;
    map<int, Action> ccMap;
    map<int, string> ccNames;      // user-defined CC names from [CCNames]
    map<int, bool> toggleState;    // toggle mode state tracking
    int sensitivity = 30;
    int ccDeadzone = 2;
    NoteMode noteMode = MODE_HOLD;
    int tapDuration = 50;          // ms for tap mode
    map<int, int> lastCCVal;

    string getCCDisplayName(int cc) {
        auto it = ccNames.find(cc);
        if (it != ccNames.end()) return it->second;
        string std = getStdCCName(cc);
        if (!std.empty()) return "CC" + to_string(cc) + "(" + std + ")";
        return "CC" + to_string(cc);
    }

    Action parseAction(const string& s) {
        Action a;
        string str = s;
        str.erase(remove(str.begin(), str.end(), ' '), str.end());
        if (str == "MOUSE_LEFT")   { a.type=ACT_MOUSE_L; a.name=str; return a; }
        if (str == "MOUSE_RIGHT")  { a.type=ACT_MOUSE_R; a.name=str; return a; }
        if (str == "MOUSE_MIDDLE") { a.type=ACT_MOUSE_M; a.name=str; return a; }
        if (str == "MOUSE_MOVE_X") { a.type=ACT_MOUSE_MX; a.name=str; return a; }
        if (str == "MOUSE_MOVE_Y") { a.type=ACT_MOUSE_MY; a.name=str; return a; }
        if (str == "MOUSE_SCROLL") { a.type=ACT_MOUSE_SCROLL; a.name=str; return a; }
        if (str.find('+') != string::npos) {
            a.type=ACT_KEY_COMBO; a.name=str;
            stringstream ss(str); string token;
            while (getline(ss, token, '+')) {
                auto it = VK_MAP.find(token);
                if (it != VK_MAP.end()) a.keys.push_back(it->second);
            }
            return a;
        }
        auto it = VK_MAP.find(str);
        if (it != VK_MAP.end()) {
            a.type=ACT_KEY; a.keys.push_back(it->second); a.name=str;
        }
        return a;
    }

    // Helper: execute press for an action
    void execPress(const Action& a) {
        switch (a.type) {
            case ACT_KEY: InputSim::pressKey(a.keys[0]); break;
            case ACT_KEY_COMBO: InputSim::pressCombo(a.keys); break;
            case ACT_MOUSE_L: InputSim::mouseDown(MOUSEEVENTF_LEFTDOWN); break;
            case ACT_MOUSE_R: InputSim::mouseDown(MOUSEEVENTF_RIGHTDOWN); break;
            case ACT_MOUSE_M: InputSim::mouseDown(MOUSEEVENTF_MIDDLEDOWN); break;
            default: break;
        }
    }

    // Helper: execute release for an action
    void execRelease(const Action& a) {
        switch (a.type) {
            case ACT_KEY: InputSim::releaseKey(a.keys[0]); break;
            case ACT_KEY_COMBO: InputSim::releaseCombo(a.keys); break;
            case ACT_MOUSE_L: InputSim::mouseUp(MOUSEEVENTF_LEFTUP); break;
            case ACT_MOUSE_R: InputSim::mouseUp(MOUSEEVENTF_RIGHTUP); break;
            case ACT_MOUSE_M: InputSim::mouseUp(MOUSEEVENTF_MIDDLEUP); break;
            default: break;
        }
    }

    bool loadFromFile(const string& path) {
        ifstream f(path);
        if (!f.is_open()) return false;
        noteMap.clear(); ccMap.clear(); ccNames.clear(); toggleState.clear();
        string line, section;
        while (getline(f, line)) {
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == string::npos) continue;
            line = line.substr(start);
            if (line[0] == '#') continue;
            if (line[0] == '[') {
                size_t end = line.find(']');
                if (end != string::npos) section = line.substr(1, end-1);
                continue;
            }
            size_t eq = line.find('=');
            if (eq == string::npos) continue;
            string key = line.substr(0, eq);
            string val = line.substr(eq+1);
            key.erase(key.find_last_not_of(" \t")+1);
            key.erase(0, key.find_first_not_of(" \t"));
            val.erase(val.find_last_not_of(" \t\r\n")+1);
            val.erase(0, val.find_first_not_of(" \t"));

            if (section == "NoteMap") {
                noteMap[atoi(key.c_str())] = parseAction(val);
            } else if (section == "CCMap") {
                ccMap[atoi(key.c_str())] = parseAction(val);
            } else if (section == "CCNames") {
                ccNames[atoi(key.c_str())] = val;
            } else if (section == "Settings") {
                if (key == "mouse_sensitivity") sensitivity = atoi(val.c_str());
                if (key == "cc_deadzone") ccDeadzone = atoi(val.c_str());
                if (key == "tap_duration") tapDuration = atoi(val.c_str());
                if (key == "note_mode") {
                    if (val == "hold") noteMode = MODE_HOLD;
                    else if (val == "toggle") noteMode = MODE_TOGGLE;
                    else if (val == "tap") noteMode = MODE_TAP;
                }
            }
        }
        return true;
    }

    void execNoteOn(int note, int vel) {
        onVelocityEvent(note, vel, getNoteName(note));

        auto it = noteMap.find(note);
        if (it == noteMap.end()) return;
        const Action& a = it->second;

        switch (noteMode) {
        case MODE_HOLD:
            execPress(a);
            break;
        case MODE_TOGGLE:
            if (toggleState[note]) {
                execRelease(a);
                toggleState[note] = false;
            } else {
                execPress(a);
                toggleState[note] = true;
            }
            break;
        case MODE_TAP: {
            execPress(a);
            Action acopy = a;
            int dur = tapDuration;
            thread([this, acopy, dur]() {
                Sleep(dur);
                execRelease(acopy);
            }).detach();
            break;
        }
        }
    }

    void execNoteOff(int note) {
        // In toggle and tap modes, Note Off is ignored
        if (noteMode == MODE_TOGGLE || noteMode == MODE_TAP) return;
        // MODE_HOLD: release on Note Off
        auto it = noteMap.find(note);
        if (it == noteMap.end()) return;
        execRelease(it->second);
    }

    void execCC(int cc, int val) {
        auto it = ccMap.find(cc);
        if (it == ccMap.end()) return;
        const Action& a = it->second;
        int lastVal = 64;
        if (lastCCVal.count(cc)) lastVal = lastCCVal[cc];
        int delta = val - lastVal;
        lastCCVal[cc] = val;
        if (abs(delta) < ccDeadzone) return;
        int scaled = delta * sensitivity / 20;
        switch (a.type) {
            case ACT_MOUSE_MX: InputSim::mouseMove(scaled, 0); break;
            case ACT_MOUSE_MY: InputSim::mouseMove(0, -scaled); break;
            case ACT_MOUSE_SCROLL: InputSim::mouseScroll(scaled * 10); break;
            case ACT_KEY:
                if (val >= 64) InputSim::pressKey(a.keys[0]);
                else InputSim::releaseKey(a.keys[0]);
                break;
            case ACT_KEY_COMBO:
                if (val >= 64) InputSim::pressCombo(a.keys);
                else InputSim::releaseCombo(a.keys);
                break;
            default: break;
        }
    }
};

// ==================== MIDI Device Manager ====================
struct DeviceInfo { int id; string name; bool isOpen; HMIDIIN handle; };

static MappingManager g_mapper;
static mutex g_mtx;
static atomic<bool> g_monitor{false};
static atomic<bool> g_mapping{true};
static vector<DeviceInfo> g_devices;

static string timestamp() {
    SYSTEMTIME st; GetLocalTime(&st);
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return string(buf);
}

void CALLBACK MidiCallback(HMIDIIN hMidi, UINT msg, DWORD_PTR inst,
                            DWORD_PTR p1, DWORD_PTR p2) {
    if (msg != MIM_DATA) return;
    int devIdx = (int)inst;
    int status = p1 & 0xFF;
    int data1 = (p1 >> 8) & 0xFF;
    int data2 = (p1 >> 16) & 0xFF;
    int mtype = status & 0xF0;
    int ch = (status & 0x0F) + 1;

    lock_guard<mutex> lk(g_mtx);

    if (g_monitor.load()) {
        string ts = timestamp();
        string devName = (devIdx < (int)g_devices.size()) ? g_devices[devIdx].name : "?";
        string actStr = "";
        if (mtype == 0x90 && data2 > 0) {
            auto it = g_mapper.noteMap.find(data1);
            if (it != g_mapper.noteMap.end()) actStr = " -> " + it->second.name;
            printf("[%s] [%-20s] NoteOn   ch=%-2d note=%-3d %-5s vel=%-3d%s\n",
                   ts.c_str(), devName.c_str(), ch, data1,
                   getNoteName(data1).c_str(), data2, actStr.c_str());
        } else if (mtype == 0x80 || (mtype == 0x90 && data2 == 0)) {
            auto it = g_mapper.noteMap.find(data1);
            if (it != g_mapper.noteMap.end()) actStr = " -> " + it->second.name + " (release)";
            printf("[%s] [%-20s] NoteOff  ch=%-2d note=%-3d %-5s%s\n",
                   ts.c_str(), devName.c_str(), ch, data1,
                   getNoteName(data1).c_str(), actStr.c_str());
        } else if (mtype == 0xB0) {
            auto it = g_mapper.ccMap.find(data1);
            if (it != g_mapper.ccMap.end()) actStr = " -> " + it->second.name;
            printf("[%s] [%-20s] CC       ch=%-2d %-20s val=%-3d%s\n",
                   ts.c_str(), devName.c_str(), ch,
                   g_mapper.getCCDisplayName(data1).c_str(), data2, actStr.c_str());
        } else if (mtype == 0xE0) {
            int bend = (data2 << 7) | data1;
            printf("[%s] [%-20s] PitchBd  ch=%-2d val=%d\n",
                   ts.c_str(), devName.c_str(), ch, bend);
        } else if (mtype == 0xA0) {
            printf("[%s] [%-20s] PolyAT   ch=%-2d note=%-3d %-5s pressure=%d\n",
                   ts.c_str(), devName.c_str(), ch, data1,
                   getNoteName(data1).c_str(), data2);
        } else if (mtype == 0xD0) {
            printf("[%s] [%-20s] ChanAT   ch=%-2d pressure=%d\n",
                   ts.c_str(), devName.c_str(), ch, data1);
        } else if (mtype == 0xC0) {
            printf("[%s] [%-20s] ProgChg  ch=%-2d program=%d\n",
                   ts.c_str(), devName.c_str(), ch, data1);
        } else {
            printf("[%s] [%-20s] Raw      status=0x%02X d1=%d d2=%d\n",
                   ts.c_str(), devName.c_str(), status, data1, data2);
        }
        fflush(stdout);
    }

    if (g_mapping.load()) {
        if (mtype == 0x90 && data2 > 0) g_mapper.execNoteOn(data1, data2);
        else if (mtype == 0x80 || (mtype == 0x90 && data2 == 0)) g_mapper.execNoteOff(data1);
        else if (mtype == 0xB0) g_mapper.execCC(data1, data2);
    }
}

void enumDevices() {
    for (auto& d : g_devices) {
        if (d.isOpen) { midiInStop(d.handle); midiInReset(d.handle); midiInClose(d.handle); }
    }
    g_devices.clear();
    UINT num = midiInGetNumDevs();
    for (UINT i = 0; i < num; i++) {
        MIDIINCAPS caps;
        if (midiInGetDevCaps(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            DeviceInfo di; di.id=i; di.name=caps.szPname; di.isOpen=false; di.handle=NULL;
            g_devices.push_back(di);
        }
    }
}

bool openDevice(int idx) {
    if (idx < 0 || idx >= (int)g_devices.size()) return false;
    if (g_devices[idx].isOpen) return true;
    MMRESULT r = midiInOpen(&g_devices[idx].handle, g_devices[idx].id,
                            (DWORD_PTR)MidiCallback, (DWORD_PTR)idx, CALLBACK_FUNCTION);
    if (r != MMSYSERR_NOERROR) return false;
    midiInStart(g_devices[idx].handle);
    g_devices[idx].isOpen = true;
    return true;
}

bool closeDevice(int idx) {
    if (idx < 0 || idx >= (int)g_devices.size()) return false;
    if (!g_devices[idx].isOpen) return true;
    midiInStop(g_devices[idx].handle);
    midiInReset(g_devices[idx].handle);
    midiInClose(g_devices[idx].handle);
    g_devices[idx].isOpen = false; g_devices[idx].handle = NULL;
    return true;
}

// ==================== Main ====================
void printHelp() {
    printf("\nCommands:\n");
    printf("  list              - List MIDI input devices\n");
    printf("  open <id>         - Open a MIDI device\n");
    printf("  close <id>        - Close a MIDI device\n");
    printf("  openall           - Open all devices\n");
    printf("  monitor [on|off]  - Toggle real-time MIDI monitor\n");
    printf("  mapping [on|off]  - Toggle key/mouse mapping\n");
    printf("  mode <hold|toggle|tap> - Set note trigger mode\n");
    printf("  reload            - Reload config file\n");
    printf("  status            - Show current status\n");
    printf("  help              - Show this help\n");
    printf("  quit              - Exit\n\n");
}

void printDevices() {
    if (g_devices.empty()) { printf("  (No MIDI input devices found)\n"); return; }
    for (size_t i = 0; i < g_devices.size(); i++) {
        printf("  [%d] %s %s\n", (int)i, g_devices[i].name.c_str(),
               g_devices[i].isOpen ? "(OPEN)" : "");
    }
}

void printStatus() {
    printf("\n=== Status ===\n");
    printf("  Monitor:     %s\n", g_monitor.load() ? "ON" : "OFF");
    printf("  Mapping:     %s\n", g_mapping.load() ? "ON" : "OFF");
    printf("  Note Mode:   %s\n", noteModeName(g_mapper.noteMode).c_str());
    if (g_mapper.noteMode == MODE_TAP)
        printf("  Tap Duration:%dms\n", g_mapper.tapDuration);
    printf("  Sensitivity: %d\n", g_mapper.sensitivity);
    printf("  Notes:       %d mappings\n", (int)g_mapper.noteMap.size());
    printf("  CCs:         %d mappings\n", (int)g_mapper.ccMap.size());
    printf("  CC Names:    %d custom\n", (int)g_mapper.ccNames.size());
    printf("  Devices:\n"); printDevices();
    printf("\n");
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    string configFile = "midi_map.ini";
    if (argc > 1) configFile = argv[1];

    printf("========================================\n");
    printf("   MIDI Mapper v1.1\n");
    printf("========================================\n\n");

    if (g_mapper.loadFromFile(configFile)) {
        printf("Config loaded: %s\n", configFile.c_str());
        printf("  %d note mappings, %d CC mappings, %d CC names\n",
               (int)g_mapper.noteMap.size(), (int)g_mapper.ccMap.size(),
               (int)g_mapper.ccNames.size());
        printf("  Note mode: %s", noteModeName(g_mapper.noteMode).c_str());
        if (g_mapper.noteMode == MODE_TAP) printf(" (%dms)", g_mapper.tapDuration);
        printf("\n");
    } else {
        printf("Warning: Cannot open '%s', no mappings loaded.\n", configFile.c_str());
    }

    enumDevices();
    printf("\nMIDI input devices:\n");
    printDevices();
    printHelp();

    char cmdBuf[256];
    while (true) {
        printf("> ");
        fflush(stdout);
        if (!fgets(cmdBuf, sizeof(cmdBuf), stdin)) break;
        string cmd(cmdBuf);
        cmd.erase(cmd.find_last_not_of(" \t\r\n")+1);
        cmd.erase(0, cmd.find_first_not_of(" \t"));
        if (cmd.empty()) continue;

        if (cmd=="quit" || cmd=="exit" || cmd=="q") break;
        else if (cmd=="help" || cmd=="?") printHelp();
        else if (cmd=="list") { enumDevices(); printDevices(); }
        else if (cmd=="openall") {
            for (int i=0; i<(int)g_devices.size(); i++) {
                if (openDevice(i)) printf("  Opened [%d] %s\n", i, g_devices[i].name.c_str());
                else printf("  Failed [%d]\n", i);
            }
        }
        else if (cmd.substr(0,5)=="open ") {
            int id = atoi(cmd.c_str()+5);
            if (openDevice(id)) printf("  Opened [%d] %s\n", id, g_devices[id].name.c_str());
            else printf("  Failed to open device %d\n", id);
        }
        else if (cmd.substr(0,6)=="close ") {
            int id = atoi(cmd.c_str()+6);
            if (closeDevice(id)) printf("  Closed [%d]\n", id);
            else printf("  Failed to close device %d\n", id);
        }
        else if (cmd.substr(0,7)=="monitor") {
            if (cmd.find("on")!=string::npos) g_monitor=true;
            else if (cmd.find("off")!=string::npos) g_monitor=false;
            else g_monitor = !g_monitor.load();
            printf("  Monitor: %s\n", g_monitor.load() ? "ON" : "OFF");
            if (g_monitor.load()) printf("  (MIDI messages will show in real-time)\n");
        }
        else if (cmd.substr(0,7)=="mapping") {
            if (cmd.find("on")!=string::npos) g_mapping=true;
            else if (cmd.find("off")!=string::npos) g_mapping=false;
            else g_mapping = !g_mapping.load();
            printf("  Mapping: %s\n", g_mapping.load() ? "ON" : "OFF");
        }
        else if (cmd.substr(0,5)=="mode ") {
            string m = cmd.substr(5);
            m.erase(0, m.find_first_not_of(" \t"));
            lock_guard<mutex> lk(g_mtx);
            if (m == "hold") { g_mapper.noteMode = MODE_HOLD; g_mapper.toggleState.clear(); }
            else if (m == "toggle") { g_mapper.noteMode = MODE_TOGGLE; g_mapper.toggleState.clear(); }
            else if (m == "tap") g_mapper.noteMode = MODE_TAP;
            else { printf("  Unknown mode. Use: hold, toggle, tap\n"); continue; }
            printf("  Note mode: %s\n", noteModeName(g_mapper.noteMode).c_str());
        }
        else if (cmd=="reload") {
            lock_guard<mutex> lk(g_mtx);
            if (g_mapper.loadFromFile(configFile))
                printf("  Reloaded: %d note, %d CC mappings, %d CC names, mode=%s\n",
                       (int)g_mapper.noteMap.size(), (int)g_mapper.ccMap.size(),
                       (int)g_mapper.ccNames.size(),
                       noteModeName(g_mapper.noteMode).c_str());
            else printf("  Failed to reload config\n");
        }
        else if (cmd=="status") printStatus();
        else printf("  Unknown command. Type 'help'.\n");
    }

    for (auto& d : g_devices) {
        if (d.isOpen) { midiInStop(d.handle); midiInReset(d.handle); midiInClose(d.handle); }
    }
    printf("Goodbye!\n");
    return 0;
}
