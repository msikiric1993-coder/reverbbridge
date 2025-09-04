// reverb_bridge.cpp — runtime OpenAL Soft preset applier (exports rm_apply_preset)
// Loads soft_oal.dll at runtime and sets an EAX‑style reverb effect/slot.
// Build: CMake + MSVC (Windows) OR MinGW‑w64 cross‑compile.
// Output: reverbbridge.dll → place in Anomaly\bin\

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <string>

typedef int   ALenum; typedef float ALfloat; typedef unsigned int ALuint;
#define AL_NO_ERROR 0
#define AL_EFFECTSLOT_EFFECT 0x0001
#define AL_EFFECTSLOT_GAIN   0x0002
#define AL_EFFECT_TYPE       0x8001
#define AL_EFFECT_EAXREVERB  0x8000
#define AL_EAXREVERB_DECAY_TIME            0x0001
#define AL_EAXREVERB_DECAY_HFRATIO         0x0002
#define AL_EAXREVERB_DECAY_LFRATIO         0x0003
#define AL_EAXREVERB_REFLECTIONS_GAIN      0x0004
#define AL_EAXREVERB_REFLECTIONS_DELAY     0x0005
#define AL_EAXREVERB_LATE_REVERB_GAIN      0x0006
#define AL_EAXREVERB_LATE_REVERB_DELAY     0x0007
#define AL_EAXREVERB_ECHO_TIME             0x0008
#define AL_EAXREVERB_ECHO_DEPTH            0x0009
#define AL_EAXREVERB_MODULATION_TIME       0x000A
#define AL_EAXREVERB_MODULATION_DEPTH      0x000B
#define AL_EAXREVERB_AIR_ABSORPTION_GAINHF 0x000C
#define AL_EAXREVERB_HFREFERENCE           0x000D
#define AL_EAXREVERB_LFREFERENCE           0x000E
#define AL_EAXREVERB_ROOM_ROLLOFF_FACTOR   0x000F
#define AL_EAXREVERB_DENSITY               0x0010
#define AL_EAXREVERB_DIFFUSION             0x0011

typedef int   (APIENTRY *LPALGETERROR)(void);
typedef void  (APIENTRY *LPALGENEFFECTS)(ALuint, ALuint*);
typedef void  (APIENTRY *LPALEFFECTI)(ALuint, ALenum, ALenum);
typedef void  (APIENTRY *LPALEFFECTF)(ALuint, ALenum, ALfloat);
typedef void  (APIENTRY *LPALGENAUXILIARYEFFECTSLOTS)(ALuint, ALuint*);
typedef void  (APIENTRY *LPALAUXILIARYEFFECTSLOTI)(ALuint, ALenum, ALuint);
typedef void  (APIENTRY *LPALAUXILIARYEFFECTSLOTF)(ALuint, ALenum, ALfloat);

static HMODULE gOpenAL = nullptr;
static LPALGETERROR alGetError = nullptr;
static LPALGENEFFECTS alGenEffects = nullptr;
static LPALEFFECTI alEffecti = nullptr;
static LPALEFFECTF alEffectf = nullptr;
static LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots = nullptr;
static LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti = nullptr;
static LPALAUXILIARYEFFECTSLOTF alAuxiliaryEffectSlotf = nullptr;
static ALuint gEffect=0, gSlot=0;

static bool load_al(){
    if(gOpenAL) return true;
    gOpenAL = LoadLibraryA("soft_oal.dll");  // shipped in Anomaly\bin
    if(!gOpenAL) return false;
    auto gp = GetProcAddress;
    alGetError=(LPALGETERROR)gp(gOpenAL,"alGetError");
    alGenEffects=(LPALGENEFFECTS)gp(gOpenAL,"alGenEffects");
    alEffecti=(LPALEFFECTI)gp(gOpenAL,"alEffecti");
    alEffectf=(LPALEFFECTF)gp(gOpenAL,"alEffectf");
    alGenAuxiliaryEffectSlots=(LPALGENAUXILIARYEFFECTSLOTS)gp(gOpenAL,"alGenAuxiliaryEffectSlots");
    alAuxiliaryEffectSloti=(LPALAUXILIARYEFFECTSLOTI)gp(gOpenAL,"alAuxiliaryEffectSloti");
    alAuxiliaryEffectSlotf=(LPALAUXILIARYEFFECTSLOTF)gp(gOpenAL,"alAuxiliaryEffectSlotf");
    return alGetError&&alGenEffects&&alEffecti&&alEffectf&&alGenAuxiliaryEffectSlots&&alAuxiliaryEffectSloti&&alAuxiliaryEffectSlotf;
}
static bool ensure_effect(){
    if(!load_al()) return false;
    if(!gEffect){
        alGenEffects(1,&gEffect); if(alGetError()!=AL_NO_ERROR) return false;
        alEffecti(gEffect,AL_EFFECT_TYPE,AL_EFFECT_EAXREVERB); if(alGetError()!=AL_NO_ERROR) return false;
        alGenAuxiliaryEffectSlots(1,&gSlot); if(alGetError()!=AL_NO_ERROR) return false;
        alAuxiliaryEffectSloti(gSlot,AL_EFFECTSLOT_EFFECT,gEffect); if(alGetError()!=AL_NO_ERROR) return false;
    }
    return true;
}

struct P { float DecayTime, DecayHFRatio, DecayLFRatio,
                 RefGain, RefDelay, LateGain, LateDelay,
                 EchoTime, EchoDepth, ModTime, ModDepth,
                 AirAbsHF, HFRef, LFRef, RoomRolloff, Density, Diffusion, Gain; };

static std::map<std::string,P> make_presets(){
    auto lower = [](std::string s){ for(char& c:s) c=char(::tolower((unsigned char)c)); return s; };
    std::map<std::string,P> m;
    m[lower("Generic")]       = {1.49f,0.83f,1.0f,-1000,0.007f,-1100,0.011f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,1.0f,1.0f};
    m[lower("Room")]          = {0.40f,0.67f,1.0f, -100,0.002f, -600,0.006f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,0.79f,1.0f,1.0f};
    m[lower("Bathroom")]      = {1.49f,0.54f,1.0f,-1200,0.007f, -700,0.011f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,1.0f,1.2f};
    m[lower("Livingroom")]    = {0.50f,0.10f,1.0f,-1000,0.003f, -600,0.004f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,0.68f,1.0f,1.0f};
    m[lower("Stoneroom")]     = {2.31f,0.64f,1.0f,-1000,0.012f, -300,0.017f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,1.0f,0.9f};
    m[lower("Auditorium")]    = {4.32f,0.59f,1.0f,-1000,0.020f, -476,0.030f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,1.0f,0.9f};
    m[lower("ConcertHall")]   = {3.92f,0.70f,1.0f,-1000,0.020f, -500,0.029f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,1.0f,0.9f};
    m[lower("Cave")]          = {2.91f,1.30f,1.0f,-1000,0.015f, -600,0.022f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,0.7f,1.0f};
    m[lower("Hallway")]       = {1.49f,0.59f,1.0f,-1000,0.007f, -300,0.011f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,0.7f,1.0f};
    m[lower("StoneCorridor")] = {2.70f,0.79f,1.0f,-1000,0.013f, -300,0.020f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,0.8f,1.0f};
    m[lower("Forest")]        = {1.49f,0.54f,1.0f,-1000,0.162f,-1100,0.088f,0.125f,1.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,0.3f,1.0f};
    m[lower("City")]          = {1.49f,0.67f,1.0f,-1000,0.007f, -800,0.011f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,0.5f,1.0f};
    m[lower("Mountains")]     = {1.49f,0.21f,1.0f,-1000,0.300f,-1200,0.100f,0.25f,1.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,0.3f,1.0f};
    m[lower("Quarry")]        = {1.49f,0.83f,1.0f,-1000,0.061f,-1000,0.025f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,1.0f,1.0f};
    m[lower("Plain")]         = {1.49f,0.50f,1.0f,-1000,0.179f,-2000,0.100f,0.25f,1.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,0.5f,1.0f};
    m[lower("ParkingLot")]    = {1.65f,1.50f,1.0f,-1000,0.008f,-1300,0.012f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,1.0f,1.0f,1.0f};
    m[lower("SewerPipe")]     = {2.81f,0.14f,1.0f,-1000,0.014f, -800,0.021f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,0.81f,0.14f,1.0f};
    m[lower("Underwater")]    = {1.49f,1.00f,1.0f,-1000,0.007f, -400,0.011f,0.25f,0.0f,0.25f,0.0f,-5.0f,5000,250,0.0f,0.10f,1.0f,0.7f};
    m[lower("Drugged")]       = {8.39f,1.39f,1.0f,-1000,0.002f,-1000,0.030f,0.25f,1.0f,0.25f,1.0f,-5.0f,5000,250,0.0f,1.0f,1.0f,1.0f};
    m[lower("Dizzy")]         = {17.23f,0.56f,1.0f,-1000,0.002f,-1000,0.030f,0.25f,1.0f,0.25f,1.0f,-5.0f,5000,250,0.0f,1.0f,1.0f,1.0f};
    m[lower("Psychotic")]     = {7.56f,0.91f,1.0f,-1000,0.002f,-1000,0.030f,0.25f,1.0f,0.25f,1.0f,-5.0f,5000,250,0.0f,1.0f,1.0f,1.0f};
    return m;
}

static bool apply_params(const P& p){
    if(!ensure_effect()) return false;
    auto F=[&](ALenum a, float v){ alEffectf(gEffect,a,v); };
    F(AL_EAXREVERB_DECAY_TIME, p.DecayTime);
    F(AL_EAXREVERB_DECAY_HFRATIO, p.DecayHFRatio);
    F(AL_EAXREVERB_DECAY_LFRATIO, p.DecayLFRatio);
    F(AL_EAXREVERB_REFLECTIONS_GAIN, p.RefGain/1000.0f);
    F(AL_EAXREVERB_REFLECTIONS_DELAY, p.RefDelay);
    F(AL_EAXREVERB_LATE_REVERB_GAIN, p.LateGain/1000.0f);
    F(AL_EAXREVERB_LATE_REVERB_DELAY, p.LateDelay);
    F(AL_EAXREVERB_ECHO_TIME, p.EchoTime);
    F(AL_EAXREVERB_ECHO_DEPTH, p.EchoDepth);
    F(AL_EAXREVERB_MODULATION_TIME, p.ModTime);
    F(AL_EAXREVERB_MODULATION_DEPTH, p.ModDepth);
    F(AL_EAXREVERB_AIR_ABSORPTION_GAINHF, powf(10.0f, p.AirAbsHF/20.0f));
    F(AL_EAXREVERB_HFREFERENCE, p.HFRef);
    F(AL_EAXREVERB_LFREFERENCE, p.LFRef);
    F(AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, p.RoomRolloff);
    F(AL_EAXREVERB_DENSITY, p.Density);
    F(AL_EAXREVERB_DIFFUSION, p.Diffusion);
    alAuxiliaryEffectSlotf(gSlot, AL_EFFECTSLOT_GAIN, p.Gain);
    alAuxiliaryEffectSloti(gSlot, AL_EFFECTSLOT_EFFECT, gEffect);
    return (alGetError()==AL_NO_ERROR);
}

extern "C" __declspec(dllexport) bool rm_apply_preset(const char* name){
    static std::map<std::string,P> presets = make_presets();
    std::string key = name? name: "Generic";
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c){return char(::tolower(c));});
    auto it = presets.find(key);
    if(it==presets.end()) it = presets.find("generic");
    return apply_params(it->second);
}
