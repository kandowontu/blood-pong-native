#include <windows.h>
#include <mmsystem.h>
#include <xinput.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "resource.h"

namespace {

constexpr int kCanvasWidth = 640;
constexpr int kCanvasHeight = 480;
constexpr int kArtX = 48;
constexpr int kArtY = 24;
// Fighter construction at original VA 0x0040D310 initializes this field to 0xBA.
constexpr int kMaximumHealth = 0xBA;
// Fighter construction initializes the adjacent turbo/energy field to 0x3A.
constexpr int kMaximumTurbo = 0x3A;
// The original caps the fighter's super field at 0x9A (for example 0x0040CC40).
constexpr int kMaximumSuper = 0x9A;
// The ball constructor at 0x0041294C uses a 544x432 playfield, a default
// five-pixel velocity on each axis, and 30 damage at either outer wall.
constexpr int kPlayfieldWidth = 0x220;
constexpr int kPlayfieldHeight = 0x1B0;
constexpr int kDefaultBallSpeed = 5;
constexpr int kDefaultBallDamage = 0x1E;
constexpr int kFighterCollisionWidth = 12;
constexpr int kFighterCollisionHeight = 54;
constexpr COLORREF kTransparent = RGB(0, 0, 0);

namespace VersusKode {
constexpr int ultimateUnlock = 111999;
constexpr int tinyBall = 110110;
constexpr int slowBall = 654321;
constexpr int fastBall = 123456;
constexpr int projectileHalfDamage = 421421;
constexpr int projectileDoubleDamage = 124124;
constexpr int ballHalfDamage = 222222;
constexpr int ballDoubleDamage = 888888;
constexpr int ballDisabled = 989121;
constexpr int invisibleBall = 100100;
constexpr int secondaryBall = 202202;
constexpr int decoyBall = 414141;
constexpr int crazyBall = 228882;
constexpr int mammothBall = 990990;
constexpr int giantBall = 880880;
constexpr int runDisabled = 604406;
constexpr int projectilesDisabled = 510510;
constexpr int hiddenBars = 123987;
constexpr int reverseControls = 35035;
constexpr int randomPaddles = 555555;
constexpr int invisiblePaddles = 711117;
constexpr int player1HalfEnergy = 33000;
constexpr int player2HalfEnergy = 33;
constexpr int bothPlayersHalfEnergy = 33033;
constexpr int player1QuarterEnergy = 707000;
constexpr int player2QuarterEnergy = 707;
constexpr int bothPlayersQuarterEnergy = 707707;
}  // namespace VersusKode

struct VersusKodeDefinition {
    int value{};
    std::string_view message;
};

// The complete message dispatcher at 0x00413EAC, in display order.
constexpr std::array<VersusKodeDefinition, 30> kVersusKodes{{
    {22067, "WHAT ELSE SHOULD I BE...ALL APOLOGIES"},
    {123926, "THERE IS NO KNOWLEDGE THAT IS NOT POWER"},
    {82397, "BE HERE NOW"},
    {VersusKode::tinyBall, "TINY BALL"},
    {VersusKode::slowBall, "SLOW BALL"},
    {VersusKode::fastBall, "FAST BALL"},
    {VersusKode::projectileHalfDamage, "PROJECTILE HALF DAMAGE"},
    {VersusKode::projectileDoubleDamage, "PROJECTILE DOUBLE DAMAGE"},
    {VersusKode::ballHalfDamage, "BALL HALF DAMAGE"},
    {VersusKode::ballDoubleDamage, "BALL DOUBLE DAMAGE"},
    {VersusKode::ballDisabled, "BALL DISABLED"},
    {VersusKode::invisibleBall, "INVISIBLE BALL"},
    {VersusKode::secondaryBall, "SECONDARY BALL"},
    {VersusKode::decoyBall, "DECOY BALL"},
    {VersusKode::crazyBall, "CRAZY BALL"},
    {VersusKode::mammothBall, "MAMMOTH BALL"},
    {VersusKode::giantBall, "GIANT BALL"},
    {VersusKode::runDisabled, "RUN DISABLED"},
    {VersusKode::projectilesDisabled, "PROJECTILES DISABLED"},
    {VersusKode::hiddenBars, "HIDDEN BARS"},
    {VersusKode::reverseControls, "REVERSE KONTROLS"},
    {VersusKode::randomPaddles, "RANDOM PADDLES"},
    {VersusKode::invisiblePaddles, "INVISIBLE PADDLES"},
    {VersusKode::player1HalfEnergy, "PLAYER 1 HALF ENERGY"},
    {VersusKode::player2HalfEnergy, "PLAYER 2 HALF ENERGY"},
    {VersusKode::bothPlayersHalfEnergy, "BOTH PLAYERS HALF ENERGY"},
    {VersusKode::player1QuarterEnergy, "PLAYER 1 QUARTER ENERGY"},
    {VersusKode::player2QuarterEnergy, "PLAYER 2 QUARTER ENERGY"},
    {VersusKode::bothPlayersQuarterEnergy, "BOTH PLAYERS QUARTER ENERGY"},
    {0, ""},
}};

enum class Screen {
    title, credits, characterSelect, versusKode, match, cheatMenu, configuration,
    continuePrompt, gameOver
};
enum class MatchPhase { playing, betweenRounds, finishPrompt, matchResult };
enum class CombatButton : std::uint8_t { attack1, attack2, attack3, turbo, super };

struct BitmapAsset {
    HBITMAP handle{};
    int width{};
    int height{};
};

struct ResourceView {
    const std::uint8_t* data{};
    std::size_t size{};
    explicit operator bool() const { return data != nullptr; }
};

struct SpriteAsset {
    int width{};
    int height{};
    std::vector<std::uint32_t> pixels;
    explicit operator bool() const { return width > 0 && height > 0 && !pixels.empty(); }
};

struct SoundAsset {
    std::vector<std::uint8_t> samples;
    std::uint32_t sampleRate{};
    explicit operator bool() const { return sampleRate != 0 && !samples.empty(); }
};

struct AudioVoice {
    const SoundAsset* sound{};
    double position{};
    float gain{1.0f};
};

struct Projectile {
    bool active{};
    bool secondaryPhase{};
    bool hasHit{};
    int owner{};
    float x{};
    float y{};
    float velocityX{};
    float velocityY{};
    int frame{};
    int age{};
    int lifetime{};
    int damage{};
    int originalType{};
    int variant{};
    int mode{};
};

struct BallEffectState {
    bool active{};
    bool visible{true};
    int type{};
    int owner{-1};
    int counter{};
    int returnBase{kDefaultBallSpeed};
    bool reboundPending{};
    bool echoActive{};
    float echoX{};
    float echoY{};
    float echoVelocityX{};
    float echoVelocityY{};
};

struct ProjectileVisual {
    int originalType{};
    int resourceType{};
    int firstResourceId{};
    int frameCount{};
};

struct ComponentDefinition {
    int originalType{};
    int variant{};
    int delay{};
    int damage{};
};

struct ComboRecipe {
    std::array<CombatButton, 5> buttons{};
    int length{};
    int component{};
    bool realmTransport{};
};

using XInputGetStateFunction = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
using XInputSetStateFunction = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);

struct App {
    HWND window{};
    HDC backDc{};
    HBITMAP backBitmap{};
    HBITMAP oldBackBitmap{};
    void* backPixels{};
    HDC presentationDc{};
    HBITMAP presentationBitmap{};
    HBITMAP oldPresentationBitmap{};
    int presentationWidth{};
    int presentationHeight{};
    BitmapAsset title{};
    BitmapAsset creditsBackdrop{};
    std::array<BitmapAsset, 4> selectedItems{};
    std::array<BitmapAsset, 4> normalItems{};
    BitmapAsset selectBackdrop{};
    BitmapAsset versusBackdrop{};
    BitmapAsset mysteryPortrait{};
    BitmapAsset continuePanel{};
    std::array<BitmapAsset, 10> continueDigits{};
    std::array<BitmapAsset, 3> matchBackdrops{};
    int matchStage{};
    BitmapAsset configBanner{};
    std::array<BitmapAsset, 17> portraits{};
    std::array<SpriteAsset, 16> standingPaddles{};
    std::array<std::array<SpriteAsset, 24>, 16> paddleFrames{};
    std::array<std::vector<SpriteAsset>, 25> projectileTypeFrames{};
    std::array<SpriteAsset, 16> fighterNameSprites{};
    // Type-2004 IDs 500..503 are the normal, giant, mammoth and tiny balls.
    std::array<SpriteAsset, 4> ballSprites{};
    SpriteAsset roundWinMarker{};
    SpriteAsset healthFrame{};
    SpriteAsset turboFrame{};
    std::array<SpriteAsset, 2> superFrames{};
    std::array<SpriteAsset, 18> roundFrames{};
    std::array<std::array<SpriteAsset, 18>, 3> roundNumberFrames{};
    std::array<SpriteAsset, 15> fightFrames{};
    std::array<SpriteAsset, 14> finishHimFrames{};
    std::array<SpriteAsset, 14> finishHerFrames{};
    SpriteAsset fatalitySprite{};
    SoundAsset titleMusic{};
    SoundAsset matchMusic{};
    SoundAsset ladderSound{};
    std::array<SoundAsset, 2> gameOverSounds{};
    std::array<SoundAsset, 16> fighterVoices{};
    SoundAsset menuMoveSound{};
    SoundAsset menuSelectSound{};
    SoundAsset roundSound{};
    std::array<SoundAsset, 3> roundNumberSounds{};
    SoundAsset fightSound{};
    SoundAsset finishHimSound{};
    SoundAsset finishHerSound{};
    SoundAsset fatalitySound{};
    SoundAsset realmTransportSound{};
    SoundAsset projectileAlternateSound{};
    SoundAsset projectileSecondarySound{};
    SoundAsset ballBounceSound{};
    SoundAsset ballHitSound{};
    std::array<SoundAsset, 25> projectileSounds{};
    HWAVEOUT audioDevice{};
    static constexpr std::size_t kAudioBufferCount = 4;
    static constexpr std::size_t kAudioBufferSamples = 512;
    static constexpr std::uint32_t kAudioOutputRate = 22050;
    std::array<std::array<std::int16_t, kAudioBufferSamples>, kAudioBufferCount>
        audioBuffers{};
    std::array<WAVEHDR, kAudioBufferCount> audioHeaders{};
    std::array<AudioVoice, 8> audioVoices{};
    const SoundAsset* currentMusic{};
    double musicPosition{};
    bool audioShuttingDown{};
    std::array<std::uint32_t, 256> palette{};
    Screen screen{Screen::title};
    Screen screenBelowCheats{Screen::match};
    int titleSelection{};
    int playerCount{1};
    int selectingPlayer{};
    std::array<int, 2> selectedCharacters{0, 1};
    std::array<int, 9> activeLadder{};
    int ladderIndex{};
    int continues{5};
    int continueCountdown{9};
    int continueCountdownTicks{};
    std::array<int, 6> kodeDigits{};
    int currentKode{};
    bool allContentUnlocked{};
    bool soundEnabled{true};
    bool vibrationEnabled{true};
    int configSelection{};
    int player1Pad{-1};
    int player2Pad{-1};
    int gamepadDeadZone{24};
    HMODULE xinputModule{};
    XInputGetStateFunction xinputGetState{};
    XInputSetStateFunction xinputSetState{};
    std::array<XINPUT_STATE, XUSER_MAX_COUNT> gamepads{};
    std::array<XINPUT_STATE, XUSER_MAX_COUNT> previousGamepads{};
    std::array<bool, XUSER_MAX_COUNT> gamepadConnected{};
    std::array<int, XUSER_MAX_COUNT> vibrationTicks{};
    int cheatSelection{};
    std::array<bool, 6> cheats{};
    float player1X{50.0f};
    float player1Y{189.0f};
    float player2X{482.0f};
    float player2Y{189.0f};
    float ballX{264.0f};
    float ballY{208.0f};
    float ballVelocityX{5.0f};
    float ballVelocityY{5.0f};
    int ballBaseSpeed{kDefaultBallSpeed};
    int ballDamage{kDefaultBallDamage};
    std::array<bool, 2> ballCollisionArmed{true, true};
    int ballCrazyTicks{};
    bool secondaryBallActive{};
    float secondaryBallX{264.0f};
    float secondaryBallY{208.0f};
    float secondaryBallVelocityX{-5.0f};
    float secondaryBallVelocityY{5.0f};
    std::array<bool, 2> secondaryBallCollisionArmed{true, true};
    std::array<BallEffectState, 2> ballEffects{};
    std::array<int, 2> paddleAppearance{0, 1};
    std::array<int, 2> randomPaddleTicks{};
    std::array<int, 2> score{};
    std::array<int, 2> turbo{kMaximumTurbo, kMaximumTurbo};
    int roundNumber{1};
    int roundIntroStage{};
    int roundIntroDelay{};
    bool roundIntroActive{};
    MatchPhase matchPhase{MatchPhase::playing};
    int matchPhaseTicks{};
    int roundWinner{-1};
    bool fatalityPerformed{};
    bool realmTransportPending{};
    bool realmMatchActive{};
    int realmReturnWinner{-1};
    std::array<int, 2> health{kMaximumHealth, kMaximumHealth};
    std::array<int, 2> super{};
    std::array<bool, 2> superActive{};
    std::array<int, 2> superDrainTicks{};
    std::array<int, 2> animationFrame{};
    std::array<int, 2> animationTicks{};
    std::array<int, 2> attackCooldown{};
    std::array<std::array<CombatButton, 5>, 2> comboHistory{};
    std::array<int, 2> comboHistorySize{};
    std::array<int, 2> comboTimeout{};
    std::array<int, 2> frozenTicks{};
    std::array<Projectile, 8> projectiles{};
    std::uint64_t frameCounter{};
    bool fullscreen{};
    DWORD savedStyle{};
    DWORD savedExStyle{};
    RECT savedWindowRect{};
};

App g_app;

constexpr std::array<std::string_view, 16> kCharacterNames{
    "FUNG SHWEI", "LO THAN", "JEWEL", "RAPTOR", "SO FRIO", "NAI PALM",
    "ONE EYE", "RAIDER", "SHOW LIN", "DAWG CAU", "OMOH", "CARMACK",
    "PAIN", "LO PAN", "MAI LAI", "BAKA"};

constexpr std::array<int, 16> kCharacterResourceTypes{
    2017, 2006, 2005, 2014, 2016, 2010, 2011, 2015,
    2018, 2000, 2012, 2002, 2013, 2008, 2020, 2009};

// Literal +0x77c assignments in the sixteen constructor handlers at
// 0x0040D83E..0x0040E71E. This selects the ball behavior applied while that
// fighter's full-Super state is armed.
constexpr std::array<int, 16> kBallEffectByCharacter{
    4, 1, 6, 3, 7, 8, 5, 10, 4, 9, 6, 7, 4, 9, 6, 8};

// The original loader at 0x0040AA64 binds these VOC resources to the same
// sixteen-entry fighter constructor order used by character selection.
constexpr std::array<int, 16> kFighterVoiceIds{
    2000, 2004, 2001, 2005, 2002, 2007, 2003, 2006,
    2010, 2014, 2011, 2015, 2012, 2017, 2013, 2016};

// Start cues used by the projectile dispatcher at 0x0041ACB5. Type four's
// mode-11 branch substitutes resource 3028 for the ordinary 3008 cue.
constexpr std::array<int, 25> kProjectileSoundIds{
    0, 3000, 3003, 3005, 3008, 3007, 3010, 3010, 3012, 3032,
    3013, 2050, 3014, 3019, 3020, 3013, 3012, 3021, 3031, 3030,
    3029, 3023, 3025, 3025, 3020};

// Four equally likely nine-match ladders written by 0x004027A8. The original
// stores one-based fighter numbers; this native table is zero-based.
constexpr std::array<std::array<int, 9>, 4> kTournamentLadders{{
    {{12, 1, 11, 2, 9, 7, 13, 3, 5}},
    {{4, 15, 14, 8, 9, 0, 11, 3, 5}},
    {{1, 0, 14, 13, 9, 2, 3, 7, 5}},
    {{0, 12, 15, 4, 9, 3, 13, 11, 5}},
}};

// The original finish-prompt initializer at 0x004143D9 selects FINISH HER for
// fighter numbers 3, 10, and 16, and FINISH HIM for every other fighter.
constexpr std::array<bool, 16> kUsesFinishHer{
    false, false, true, false, false, false, false, false,
    false, true, false, false, false, false, false, true};

// Exact visual banks selected by the original 25-way type switch at 0x0041ACB5.
constexpr std::array<ProjectileVisual, 25> kProjectileVisuals{{
    {0, 0, 0, 0},       {1, 2022, 300, 2},  {2, 2022, 700, 6},
    {3, 2022, 1300, 6}, {4, 2022, 160, 7},  {5, 2022, 180, 4},
    {6, 2022, 128, 6},  {7, 2022, 150, 2},  {8, 0, 0, 0},
    {9, 2004, 500, 1},  {10, 2022, 1500, 4},{11, 0, 0, 0},
    {12, 2022, 1200, 7},{13, 2022, 200, 6}, {14, 2022, 1000, 15},
    {15, 2022, 600, 3}, {16, 2022, 210, 6}, {17, 2022, 170, 1},
    {18, 2022, 400, 3}, {19, 2022, 1550, 5},{20, 2022, 1600, 1},
    {21, 2022, 350, 5}, {22, 2022, 260, 4}, {23, 2022, 250, 4},
    {24, 2022, 500, 3},
}};

// Literal constructor arguments passed to 0x0041AC54. Entries are addressed by
// the one-based component numbers emitted by each fighter's combo recognizer.
constexpr std::array<std::array<ComponentDefinition, 4>, 16> kComponents{{
    {{{1,1,0,10},{0,0,0,0},{0,0,0,0},{0,0,0,0}}},
    {{{2,1,0,30},{2,1,15,30},{2,1,16,30},{0,0,0,0}}},
    {{{3,5,0,30},{3,5,14,30},{3,5,12,30},{3,5,13,30}}},
    {{{5,1,0,30},{4,4,0,0},{4,4,11,0},{0,0,0,0}}},
    {{{6,2,0,0},{7,2,0,0},{8,2,0,0},{0,0,0,0}}},
    {{{9,10,6,30},{9,10,7,30},{9,10,8,30},{9,10,9,30}}},
    {{{10,1,0,20},{11,0,0,0},{0,0,0,0},{0,0,0,0}}},
    {{{12,4,0,30},{12,6,0,30},{0,0,0,0},{0,0,0,0}}},
    {{{13,1,0,30},{13,1,0,15},{13,1,0,10},{14,7,0,20}}},
    {{{24,12,25,15},{24,12,26,15},{14,7,0,20},{0,0,0,0}}},
    {{{17,8,0,30},{17,8,22,30},{16,1,24,30},{0,0,0,0}}},
    {{{18,5,0,30},{18,5,17,30},{18,5,18,30},{0,0,0,0}}},
    {{{19,4,0,20},{20,11,0,0},{0,0,0,0},{0,0,0,0}}},
    {{{21,1,23,30},{21,1,0,30},{0,0,0,0},{0,0,0,0}}},
    {{{15,9,0,30},{15,9,11,30},{0,0,0,0},{0,0,0,0}}},
    {{{22,1,0,20},{23,1,0,20},{0,0,0,0},{0,0,0,0}}},
}};

constexpr CombatButton A1 = CombatButton::attack1;
constexpr CombatButton A2 = CombatButton::attack2;
constexpr CombatButton A3 = CombatButton::attack3;
constexpr CombatButton TU = CombatButton::turbo;
constexpr CombatButton SU = CombatButton::super;
constexpr ComboRecipe move(CombatButton a, CombatButton b, CombatButton c,
                           CombatButton d, CombatButton e, int component) {
    return {{{a, b, c, d, e}}, 5, component, false};
}
constexpr ComboRecipe realmTransport() {
    return {{{SU, SU, TU, TU, TU}}, 4, 0, true};
}
constexpr ComboRecipe noCombo() {
    return {{{A1, A1, A1, A1, A1}}, 0, 0, false};
}

// Shortest accepting paths recovered by emulating each original recognizer with
// a synthetic fighter object. Every transition retains the original 60-update
// input window; component numbers are exactly those passed to 0x004081E8.
constexpr std::array<std::array<ComboRecipe, 4>, 16> kComboRecipes{{
    {{realmTransport(), move(A1,A1,A2,A2,TU,1), move(A1,A1,A3,A1,SU,3), move(A2,A3,A2,A1,TU,2)}},
    {{realmTransport(), move(A1,A1,A1,A3,TU,3), move(A2,A1,A2,A2,SU,1), move(A3,A1,A1,A3,TU,2)}},
    {{realmTransport(), move(A1,A2,A2,A3,SU,2), move(A2,A1,A2,A1,SU,1), move(A3,A2,A1,A1,SU,3)}},
    {{realmTransport(), move(A1,A1,A1,A2,SU,2), move(A1,A1,A3,A2,TU,3), move(A3,A3,A2,A2,TU,1)}},
    {{realmTransport(), move(A1,A2,A1,A1,SU,1), move(A2,A1,A3,A3,SU,2), move(A3,A3,A1,A1,SU,3)}},
    {{realmTransport(), move(A1,A1,A3,A3,TU,3), move(A2,A2,A2,A2,TU,2), move(A3,A1,A3,A1,SU,1)}},
    {{realmTransport(), move(A1,A1,A2,A3,TU,3), move(A3,A1,A2,A1,TU,1), move(A3,A3,A2,A2,TU,2)}},
    {{realmTransport(), move(A1,A1,A3,A1,TU,2), move(A2,A2,A2,A3,TU,1), move(A3,A3,A2,A1,SU,3)}},
    {{realmTransport(), move(A1,A1,A1,A3,SU,3), move(A1,A2,A2,A1,SU,1), move(A3,A3,A2,A3,SU,2)}},
    {{realmTransport(), move(A2,A2,A3,A1,SU,2), move(A2,A2,A3,A3,SU,3), move(A3,A2,A1,A1,TU,1)}},
    {{realmTransport(), move(A1,A2,A1,A2,SU,1), move(A3,A2,A3,A2,SU,2), noCombo()}},
    {{realmTransport(), move(A2,A2,A1,A3,TU,1), move(A3,A2,A3,A1,SU,2), noCombo()}},
    {{realmTransport(), move(A1,A2,A3,A2,TU,1), move(A2,A2,A1,A1,TU,2), noCombo()}},
    {{realmTransport(), move(A1,A2,A2,A3,SU,1), move(A3,A3,A2,A2,TU,2), noCombo()}},
    {{realmTransport(), move(A1,A1,A1,A1,SU,2), move(A2,A1,A1,A2,TU,3), move(A3,A2,A2,A3,SU,1)}},
    {{realmTransport(), move(A1,A2,A3,A3,SU,2), move(A3,A2,A2,A2,TU,1), noCombo()}},
}};

BitmapAsset loadBitmap(HINSTANCE instance, int id) {
    BitmapAsset result;
    result.handle = static_cast<HBITMAP>(LoadImageW(
        instance, MAKEINTRESOURCEW(id), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    if (result.handle) {
        BITMAP bitmap{};
        GetObjectW(result.handle, sizeof(bitmap), &bitmap);
        result.width = bitmap.bmWidth;
        result.height = bitmap.bmHeight;
    }
    return result;
}

ResourceView loadOriginalResource(HINSTANCE instance, int type, int id) {
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(type));
    if (!resource) return {};
    HGLOBAL loaded = LoadResource(instance, resource);
    if (!loaded) return {};
    return {static_cast<const std::uint8_t*>(LockResource(loaded)),
            static_cast<std::size_t>(SizeofResource(instance, resource))};
}

SoundAsset loadOriginalVoc(HINSTANCE instance, int id) {
    SoundAsset sound;
    const ResourceView resource = loadOriginalResource(instance, 2001, id);
    constexpr char signature[] = "Creative Voice File";
    if (!resource || resource.size < 26 ||
        std::memcmp(resource.data, signature, sizeof(signature) - 1) != 0) return sound;
    const std::size_t firstBlock = static_cast<std::size_t>(resource.data[20]) |
                                   (static_cast<std::size_t>(resource.data[21]) << 8);
    if (firstBlock >= resource.size) return sound;

    std::vector<std::uint8_t> pcm;
    std::uint32_t sampleRate{};
    std::size_t offset = firstBlock;
    while (offset < resource.size) {
        const std::uint8_t type = resource.data[offset++];
        if (type == 0) break;
        if (offset + 3 > resource.size) return {};
        const std::size_t length = static_cast<std::size_t>(resource.data[offset]) |
                                   (static_cast<std::size_t>(resource.data[offset + 1]) << 8) |
                                   (static_cast<std::size_t>(resource.data[offset + 2]) << 16);
        offset += 3;
        if (offset + length > resource.size) return {};
        if (type == 1 && length >= 2) {
            const std::uint8_t timeConstant = resource.data[offset];
            const std::uint8_t codec = resource.data[offset + 1];
            if (codec != 0 || timeConstant == 255) return {};
            const std::uint32_t blockRate = 1000000u / (256u - timeConstant);
            if (sampleRate && sampleRate != blockRate) return {};
            sampleRate = blockRate;
            pcm.insert(pcm.end(), resource.data + offset + 2, resource.data + offset + length);
        } else if (type == 2) {
            if (!sampleRate) return {};
            pcm.insert(pcm.end(), resource.data + offset, resource.data + offset + length);
        }
        offset += length;
    }
    if (!sampleRate || pcm.empty()) return sound;

    sound.samples = std::move(pcm);
    sound.sampleRate = sampleRate;
    return sound;
}

bool sampleSound(const SoundAsset& sound, double& position, bool loop, int& sample) {
    if (!sound) return false;
    if (position >= static_cast<double>(sound.samples.size())) {
        if (!loop) return false;
        position = std::fmod(position, static_cast<double>(sound.samples.size()));
    }
    const auto index = std::min(static_cast<std::size_t>(position), sound.samples.size() - 1);
    sample = (static_cast<int>(sound.samples[index]) - 128) << 8;
    position += static_cast<double>(sound.sampleRate) / App::kAudioOutputRate;
    return true;
}

void fillAudioBuffer(std::size_t bufferIndex) {
    auto& buffer = g_app.audioBuffers[bufferIndex];
    for (auto& output : buffer) {
        int mixed = 0;
        if (g_app.soundEnabled && g_app.currentMusic) {
            int sample{};
            if (sampleSound(*g_app.currentMusic, g_app.musicPosition, true, sample)) {
                mixed += sample * 5 / 10;
            }
        }
        if (g_app.soundEnabled) {
            for (auto& voice : g_app.audioVoices) {
                if (!voice.sound) continue;
                int sample{};
                if (!sampleSound(*voice.sound, voice.position, false, sample)) {
                    voice = {};
                    continue;
                }
                mixed += static_cast<int>(sample * voice.gain);
            }
        }
        output = static_cast<std::int16_t>(std::clamp(mixed, -32768, 32767));
    }
}

bool initializeAudio() {
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = App::kAudioOutputRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
    if (waveOutOpen(&g_app.audioDevice, WAVE_MAPPER, &format,
                    reinterpret_cast<DWORD_PTR>(g_app.window), 0, CALLBACK_WINDOW) !=
        MMSYSERR_NOERROR) {
        g_app.audioDevice = nullptr;
        return false;
    }
    for (std::size_t index = 0; index < App::kAudioBufferCount; ++index) {
        auto& header = g_app.audioHeaders[index];
        header.lpData = reinterpret_cast<LPSTR>(g_app.audioBuffers[index].data());
        header.dwBufferLength = static_cast<DWORD>(g_app.audioBuffers[index].size() *
                                                   sizeof(std::int16_t));
        header.dwUser = index;
        fillAudioBuffer(index);
        if (waveOutPrepareHeader(g_app.audioDevice, &header, sizeof(header)) !=
                MMSYSERR_NOERROR ||
            waveOutWrite(g_app.audioDevice, &header, sizeof(header)) != MMSYSERR_NOERROR) {
            g_app.audioShuttingDown = true;
            waveOutReset(g_app.audioDevice);
            for (std::size_t prepared = 0; prepared <= index; ++prepared) {
                if (g_app.audioHeaders[prepared].dwFlags & WHDR_PREPARED) {
                    waveOutUnprepareHeader(g_app.audioDevice, &g_app.audioHeaders[prepared],
                                           sizeof(WAVEHDR));
                }
            }
            waveOutClose(g_app.audioDevice);
            g_app.audioDevice = nullptr;
            return false;
        }
    }
    return true;
}

void refillAudioBuffer(WAVEHDR* completed) {
    if (!g_app.audioDevice || g_app.audioShuttingDown || !completed) return;
    const std::size_t index = static_cast<std::size_t>(completed->dwUser);
    if (index >= App::kAudioBufferCount || completed != &g_app.audioHeaders[index]) return;
    fillAudioBuffer(index);
    waveOutWrite(g_app.audioDevice, completed, sizeof(*completed));
}

void stopAllEffects() {
    for (auto& voice : g_app.audioVoices) voice = {};
}

void playEffect(const SoundAsset& sound, float gain = 0.85f) {
    if (!g_app.soundEnabled || !sound) return;
    AudioVoice* destination = nullptr;
    for (auto& voice : g_app.audioVoices) {
        if (!voice.sound) {
            destination = &voice;
            break;
        }
    }
    if (!destination) destination = &g_app.audioVoices.front();
    *destination = {&sound, 0.0, gain};
}

void playMusic(const SoundAsset& sound) {
    g_app.currentMusic = g_app.soundEnabled && sound ? &sound : nullptr;
    g_app.musicPosition = 0.0;
    if (!g_app.soundEnabled) stopAllEffects();
}

void playTitleMusic() { playMusic(g_app.titleMusic); }

void playMatchMusic() { playMusic(g_app.matchMusic); }

void shutdownAudio() {
    if (!g_app.audioDevice) return;
    g_app.audioShuttingDown = true;
    g_app.currentMusic = nullptr;
    stopAllEffects();
    waveOutReset(g_app.audioDevice);
    for (auto& header : g_app.audioHeaders) {
        if (header.dwFlags & WHDR_PREPARED) {
            waveOutUnprepareHeader(g_app.audioDevice, &header, sizeof(header));
        }
    }
    waveOutClose(g_app.audioDevice);
    g_app.audioDevice = nullptr;
}

BitmapAsset loadOriginalBitmap(HINSTANCE instance, int type, int id, bool capturePalette = false) {
    BitmapAsset result;
    const ResourceView resource = loadOriginalResource(instance, type, id);
    if (!resource || resource.size < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) return result;
    const auto* file = reinterpret_cast<const BITMAPFILEHEADER*>(resource.data);
    if (file->bfType != 0x4D42 || file->bfOffBits >= resource.size) return result;
    const auto* info = reinterpret_cast<const BITMAPINFO*>(resource.data + sizeof(BITMAPFILEHEADER));
    if (info->bmiHeader.biBitCount != 8 || info->bmiHeader.biCompression != BI_RGB) return result;
    result.handle = CreateDIBitmap(g_app.backDc, &info->bmiHeader, CBM_INIT,
                                    resource.data + file->bfOffBits, info, DIB_RGB_COLORS);
    result.width = info->bmiHeader.biWidth;
    result.height = std::abs(info->bmiHeader.biHeight);
    if (capturePalette) {
        const auto* colors = reinterpret_cast<const RGBQUAD*>(
            resource.data + sizeof(BITMAPFILEHEADER) + info->bmiHeader.biSize);
        const std::size_t colorCount = std::min<std::size_t>(
            info->bmiHeader.biClrUsed ? info->bmiHeader.biClrUsed : 256, 256);
        for (std::size_t i = 0; i < colorCount; ++i) {
            g_app.palette[i] = (static_cast<std::uint32_t>(colors[i].rgbRed) << 16) |
                               (static_cast<std::uint32_t>(colors[i].rgbGreen) << 8) |
                               colors[i].rgbBlue;
        }
    }
    return result;
}

SpriteAsset loadOriginalSprite(HINSTANCE instance, int type, int id) {
    const ResourceView resource = loadOriginalResource(instance, type, id);
    if (!resource) return {};
    std::vector<std::vector<int>> rows;
    std::size_t offset = 0;
    bool ended = false;
    while (offset + 4 <= resource.size) {
        std::uint32_t command{};
        std::memcpy(&command, resource.data + offset, sizeof(command));
        offset += 4;
        const std::uint32_t opcode = command >> 24;
        const std::size_t count = command & 0x00FFFFFF;
        if (opcode == 0) {
            ended = true;
            break;
        }
        if (opcode == 1) {
            rows.emplace_back();
        } else if (opcode == 2) {
            if (rows.empty() || offset + count > resource.size) return {};
            auto& row = rows.back();
            for (std::size_t i = 0; i < count; ++i) row.push_back(resource.data[offset + i]);
            offset += (count + 3) & ~std::size_t{3};
        } else if (opcode == 3) {
            if (rows.empty()) return {};
            rows.back().insert(rows.back().end(), count, -1);
        } else {
            return {};
        }
    }
    if (!ended || offset != resource.size || rows.empty()) return {};
    SpriteAsset sprite;
    sprite.height = static_cast<int>(rows.size());
    for (const auto& row : rows) sprite.width = std::max(sprite.width, static_cast<int>(row.size()));
    sprite.pixels.assign(static_cast<std::size_t>(sprite.width * sprite.height), 0);
    for (int y = 0; y < sprite.height; ++y) {
        for (int x = 0; x < static_cast<int>(rows[y].size()); ++x) {
            const int paletteIndex = rows[y][x];
            if (paletteIndex >= 0) {
                sprite.pixels[static_cast<std::size_t>(y * sprite.width + x)] =
                    0xFF000000 | g_app.palette[static_cast<std::size_t>(paletteIndex)];
            }
        }
    }
    return sprite;
}

void drawSprite(const SpriteAsset& sprite, int x, int y, bool mirror = false) {
    if (!sprite || !g_app.backPixels) return;
    auto* target = static_cast<std::uint32_t*>(g_app.backPixels);
    for (int sy = 0; sy < sprite.height; ++sy) {
        const int dy = y + sy;
        if (dy < 0 || dy >= kCanvasHeight) continue;
        for (int sx = 0; sx < sprite.width; ++sx) {
            const int dx = x + sx;
            if (dx < 0 || dx >= kCanvasWidth) continue;
            const int sourceX = mirror ? sprite.width - 1 - sx : sx;
            const std::uint32_t pixel = sprite.pixels[
                static_cast<std::size_t>(sy * sprite.width + sourceX)];
            if (pixel & 0xFF000000) {
                target[static_cast<std::size_t>(dy * kCanvasWidth + dx)] = pixel & 0x00FFFFFF;
            }
        }
    }
}

void drawBitmap(const BitmapAsset& asset, int x, int y, bool transparent = false) {
    if (!asset.handle) return;
    HDC source = CreateCompatibleDC(g_app.backDc);
    HGDIOBJ old = SelectObject(source, asset.handle);
    if (transparent) {
        TransparentBlt(g_app.backDc, x, y, asset.width, asset.height, source, 0, 0,
                       asset.width, asset.height, kTransparent);
    } else {
        BitBlt(g_app.backDc, x, y, asset.width, asset.height, source, 0, 0, SRCCOPY);
    }
    SelectObject(source, old);
    DeleteDC(source);
}

HFONT makeFont(int height, int weight, bool italic, const wchar_t* face = L"Arial") {
    return CreateFontW(-height, 0, 0, 0, weight, italic, FALSE, FALSE, ANSI_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, face);
}

void drawTextLine(std::string_view text, int y, int height, COLORREF color,
                  int weight = FW_BOLD, bool italic = false) {
    HFONT font = makeFont(height, weight, italic);
    HGDIOBJ oldFont = SelectObject(g_app.backDc, font);
    SetBkMode(g_app.backDc, TRANSPARENT);
    RECT shadow{0, y + 2, kCanvasWidth + 2, y + height + 8};
    SetTextColor(g_app.backDc, RGB(0, 0, 0));
    DrawTextA(g_app.backDc, text.data(), static_cast<int>(text.size()), &shadow,
              DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    RECT target{0, y, kCanvasWidth, y + height + 8};
    SetTextColor(g_app.backDc, color);
    DrawTextA(g_app.backDc, text.data(), static_cast<int>(text.size()), &target,
              DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(g_app.backDc, oldFont);
    DeleteObject(font);
}

void clearCanvas() {
    RECT canvas{0, 0, kCanvasWidth, kCanvasHeight};
    FillRect(g_app.backDc, &canvas, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
}

void drawCreditsMenuItem(bool selected) {
    constexpr int x = kArtX + 360;
    constexpr int y = kArtY + 210;
    HFONT font = makeFont(19, FW_BOLD, true, L"Arial");
    HGDIOBJ oldFont = SelectObject(g_app.backDc, font);
    SetBkMode(g_app.backDc, TRANSPARENT);
    SetTextColor(g_app.backDc, selected ? RGB(235, 18, 25) : RGB(245, 245, 245));
    TextOutA(g_app.backDc, x + 26, y - 2, "KREDITS", 7);
    HBRUSH orb = CreateSolidBrush(selected ? RGB(190, 0, 9) : RGB(45, 165, 210));
    HGDIOBJ oldBrush = SelectObject(g_app.backDc, orb);
    HPEN pen = CreatePen(PS_SOLID, 1, selected ? RGB(255, 50, 50) : RGB(125, 225, 255));
    HGDIOBJ oldPen = SelectObject(g_app.backDc, pen);
    Ellipse(g_app.backDc, x, y + 1, x + 16, y + 17);
    SelectObject(g_app.backDc, oldPen);
    SelectObject(g_app.backDc, oldBrush);
    DeleteObject(pen);
    DeleteObject(orb);
    SelectObject(g_app.backDc, oldFont);
    DeleteObject(font);
}

void renderTitle() {
    drawBitmap(g_app.title, kArtX, kArtY);
    constexpr std::array<int, 5> ys{120, 150, 180, 210, 240};
    constexpr std::array<int, 5> assetIndices{0, 1, 2, -1, 3};
    for (int index = 0; index < 5; ++index) {
        const bool selected = g_app.titleSelection == index;
        if (assetIndices[index] < 0) {
            drawCreditsMenuItem(selected);
            continue;
        }
        const auto& item = selected ? g_app.selectedItems[assetIndices[index]]
                                    : g_app.normalItems[assetIndices[index]];
        drawBitmap(item, kArtX + 360, kArtY + ys[index], true);
    }
}

void renderCredits() {
    drawBitmap(g_app.creditsBackdrop, kArtX, kArtY);
    RECT veil{kArtX + 30, kArtY + 12, kArtX + 514, kArtY + 420};
    HBRUSH darkRed = CreateSolidBrush(RGB(24, 0, 3));
    FillRect(g_app.backDc, &veil, darkRed);
    DeleteObject(darkRed);

    drawTextLine("KREDITS", 43, 34, RGB(225, 10, 22), FW_HEAVY, true);
    drawTextLine("A KURODA PRODUCTION", 88, 18, RGB(255, 190, 35));
    drawTextLine("PROGRAMMED BY BRANDON KURODA", 119, 16, RGB(245, 245, 245));
    drawTextLine("GRAPHICS BY BRANDON KURODA", 143, 16, RGB(245, 245, 245));
    drawTextLine("MUSIC BY THE GROUP SOUR!", 167, 16, RGB(245, 245, 245));
    drawTextLine("PUBLISHED BY MONKEY BYTE DEVELOPMENT", 191, 16, RGB(245, 245, 245));
    drawTextLine("SPECIAL THANKS TO", 229, 17, RGB(80, 205, 245));
    drawTextLine("JEREMY ANDREASEN  -  DEREK CHENG  -  AUSTIN KEYS", 254, 13, RGB(235, 235, 235));
    drawTextLine("GARY KURODA  -  MIKE HIRAKAMI  -  MIKE HOLM", 275, 13, RGB(235, 235, 235));
    drawTextLine("JOHN SPEEDIE  -  ERIC WILLIAMS  -  TED \"TOASTY\" BEST", 296, 13, RGB(235, 235, 235));
    drawTextLine("NATIVE WINDOWS PRESERVATION PORT", 337, 16, RGB(255, 190, 35));
    drawTextLine("INITIATED BY THE REPOSITORY OWNER", 361, 13, RGB(235, 235, 235));
    drawTextLine("REVERSE ENGINEERING AND PORT ASSISTANCE: OPENAI CODEX", 382, 12, RGB(235, 235, 235));
    drawTextLine("ESC / ENTER TO RETURN", 433, 12, RGB(140, 140, 150), FW_NORMAL);
}

void drawOutline(int left, int top, int right, int bottom, COLORREF color, int thickness = 3) {
    HPEN pen = CreatePen(PS_SOLID, thickness, color);
    HGDIOBJ oldPen = SelectObject(g_app.backDc, pen);
    HGDIOBJ oldBrush = SelectObject(g_app.backDc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(g_app.backDc, left, top, right, bottom);
    SelectObject(g_app.backDc, oldBrush);
    SelectObject(g_app.backDc, oldPen);
    DeleteObject(pen);
}

void drawSmallText(std::string_view text, int x, int y, COLORREF color, int height = 14,
                   int weight = FW_BOLD) {
    HFONT font = makeFont(height, weight, false);
    HGDIOBJ oldFont = SelectObject(g_app.backDc, font);
    SetBkMode(g_app.backDc, TRANSPARENT);
    SetTextColor(g_app.backDc, RGB(0, 0, 0));
    TextOutA(g_app.backDc, x + 1, y + 1, text.data(), static_cast<int>(text.size()));
    SetTextColor(g_app.backDc, color);
    TextOutA(g_app.backDc, x, y, text.data(), static_cast<int>(text.size()));
    SelectObject(g_app.backDc, oldFont);
    DeleteObject(font);
}

void renderCharacterSelect() {
    drawBitmap(g_app.selectBackdrop, kArtX, kArtY);
    drawTextLine("SELECT YOUR PADDLE", 40, 25, RGB(235, 20, 28), FW_HEAVY, true);

    constexpr std::array<int, 2> portraitX{kArtX + 100, kArtX + 360};
    for (int player = 0; player < 2; ++player) {
        const int character = g_app.selectedCharacters[player];
        drawBitmap(g_app.portraits[character], portraitX[player], kArtY + 127);
        drawOutline(portraitX[player] - 3, kArtY + 124, portraitX[player] + 83,
                    kArtY + 230, player == 0 ? RGB(40, 205, 255) : RGB(235, 20, 28),
                    g_app.selectingPlayer == player ? 4 : 2);
        const std::string_view name = kCharacterNames[character];
        drawSmallText(name, portraitX[player] + 40 - static_cast<int>(name.size()) * 4,
                      kArtY + 238, player == 0 ? RGB(80, 215, 255) : RGB(255, 55, 60), 13);
    }
    if (g_app.playerCount == 1) {
        drawSmallText("CPU", kArtX + 388, kArtY + 105, RGB(255, 55, 60), 13);
    } else {
        drawSmallText("PLAYER 2", kArtX + 371, kArtY + 105, RGB(255, 55, 60), 13);
    }
    drawSmallText("PLAYER 1", kArtX + 111, kArtY + 105, RGB(80, 215, 255), 13);

    const int available = g_app.allContentUnlocked ? 16 : 4;
    for (int index = 0; index < 16; ++index) {
        const int column = index / 8;
        const int row = index % 8;
        const bool locked = index >= available;
        const COLORREF color = locked ? RGB(85, 85, 90) : RGB(230, 230, 230);
        drawSmallText(locked ? "LOCKED" : kCharacterNames[index], kArtX + 196 + column * 88,
                      kArtY + 82 + row * 22, color, 10, FW_NORMAL);
    }
    drawTextLine("LEFT / RIGHT TO CHOOSE   -   ENTER TO KONTINUE", 443, 12,
                 RGB(210, 210, 215), FW_NORMAL);
}

void drawLadderPortrait(int ladderPosition, int x, int y, bool current) {
    if (ladderPosition < 0 || ladderPosition >= static_cast<int>(g_app.activeLadder.size())) return;
    const BitmapAsset& portrait = ladderPosition == 4
        ? g_app.mysteryPortrait
        : g_app.portraits[static_cast<std::size_t>(g_app.activeLadder[ladderPosition])];
    drawBitmap(portrait, x, y);
    drawOutline(x - 2, y - 2, x + portrait.width + 2, y + portrait.height + 2,
                current ? RGB(230, 20, 25) : RGB(135, 135, 145), current ? 3 : 1);
}

void renderCpuLadder() {
    drawBitmap(g_app.selectBackdrop, kArtX, kArtY);
    const int player = g_app.selectedCharacters[0];
    drawBitmap(g_app.portraits[static_cast<std::size_t>(player)], kArtX + 172, kArtY + 166);
    drawOutline(kArtX + 170, kArtY + 164, kArtX + 254, kArtY + 268,
                RGB(60, 205, 255), 3);
    drawLadderPortrait(g_app.ladderIndex - 1, kArtX + 292, kArtY + 226, false);
    drawLadderPortrait(g_app.ladderIndex, kArtX + 292, kArtY + 166, true);
    drawLadderPortrait(g_app.ladderIndex + 1, kArtX + 292, kArtY + 106, false);

    drawTextLine("BLOOD PONG TOURNAMENT", kArtY + 38, 21,
                 RGB(225, 15, 25), FW_HEAVY, true);
    std::string battle = "BATTLE " + std::to_string(g_app.ladderIndex + 1) + " OF 9";
    drawTextLine(battle, kArtY + 355, 15, RGB(230, 230, 235));
    drawSmallText(kCharacterNames[static_cast<std::size_t>(player)],
                  kArtX + 172, kArtY + 278, RGB(80, 215, 255), 12);
    const int opponent = g_app.activeLadder[static_cast<std::size_t>(g_app.ladderIndex)];
    drawSmallText(g_app.ladderIndex == 4 ? "???" : kCharacterNames[opponent],
                  kArtX + 292, kArtY + 278, RGB(255, 55, 60), 12);
    drawTextLine("ENTER TO FIGHT", kArtY + 390, 12, RGB(205, 205, 210), FW_NORMAL);
}

int kodeFromDigits() {
    int value{};
    for (int digit : g_app.kodeDigits) value = value * 10 + digit;
    return value;
}

std::string_view versusKodeMessage(int value) {
    for (const auto& kode : kVersusKodes) {
        if (kode.value == value) return kode.message;
    }
    return {};
}

void renderVersusKode() {
    if (g_app.playerCount == 1) {
        renderCpuLadder();
        return;
    }
    drawBitmap(g_app.versusBackdrop, kArtX, kArtY);
    drawBitmap(g_app.portraits[g_app.selectedCharacters[0]], kArtX + 100, kArtY + 127);
    drawBitmap(g_app.portraits[g_app.selectedCharacters[1]], kArtX + 360, kArtY + 127);
    drawOutline(kArtX + 97, kArtY + 124, kArtX + 183, kArtY + 230, RGB(80, 215, 255), 2);
    drawOutline(kArtX + 357, kArtY + 124, kArtX + 443, kArtY + 230, RGB(255, 55, 60), 2);
    HFONT digitFont = makeFont(29, FW_HEAVY, false, L"Times New Roman");
    HGDIOBJ oldFont = SelectObject(g_app.backDc, digitFont);
    SetBkMode(g_app.backDc, TRANSPARENT);
    for (int index = 0; index < 6; ++index) {
        char digit = static_cast<char>('0' + g_app.kodeDigits[index]);
        SetTextColor(g_app.backDc, RGB(205, 10, 20));
        TextOutA(g_app.backDc, kArtX + 174 + index * 34, kArtY + 318, &digit, 1);
    }
    SelectObject(g_app.backDc, oldFont);
    DeleteObject(digitFont);
    const int enteredKode = kodeFromDigits();
    const auto message = versusKodeMessage(enteredKode);
    if (enteredKode == VersusKode::ultimateUnlock && g_app.allContentUnlocked) {
        drawTextLine("ULTIMATE KODE ACCEPTED", 400, 15, RGB(55, 235, 95));
    } else if (!message.empty()) {
        drawTextLine(message, 400, 12, RGB(225, 225, 230));
    } else {
        drawTextLine("VERSUS KODE", 400, 15, RGB(210, 210, 215));
    }
}

void drawGaugeFill(int x, int y, int width, int height, int value, int maximum,
                   COLORREF color, bool rightToLeft = false) {
    const int filled = width * std::clamp(value, 0, maximum) / maximum;
    RECT fill{rightToLeft ? x + width - filled : x, y,
              rightToLeft ? x + width : x + filled, y + height};
    HBRUSH brush = CreateSolidBrush(color);
    if (filled > 0) FillRect(g_app.backDc, &fill, brush);
    DeleteObject(brush);
}

const SpriteAsset& activePaddleSprite(int player) {
    const int character = g_app.paddleAppearance[static_cast<std::size_t>(player)];
    if (g_app.animationTicks[player] > 0) {
        const auto& frame = g_app.paddleFrames[character][g_app.animationFrame[player] % 24];
        if (frame) return frame;
    }
    return g_app.standingPaddles[character];
}

const SpriteAsset& activeBallSprite() {
    if (g_app.currentKode == VersusKode::tinyBall) return g_app.ballSprites[3];
    if (g_app.currentKode == VersusKode::mammothBall) return g_app.ballSprites[2];
    if (g_app.currentKode == VersusKode::giantBall) return g_app.ballSprites[1];
    return g_app.ballSprites[0];
}

const SpriteAsset& activeProjectileSprite(const Projectile& projectile) {
    static const SpriteAsset empty{};
    if (projectile.originalType == 8 || projectile.originalType == 11) {
        return g_app.standingPaddles[g_app.paddleAppearance[static_cast<std::size_t>(
            projectile.owner)]];
    }
    if (projectile.originalType < 0 || projectile.originalType >=
        static_cast<int>(g_app.projectileTypeFrames.size())) return empty;
    const auto& frames = g_app.projectileTypeFrames[static_cast<std::size_t>(
        projectile.originalType)];
    if (frames.empty()) return empty;
    const int cadence = projectile.originalType == 14 ? 5
        : (projectile.originalType == 24 ? 6 : 3);
    std::size_t index = static_cast<std::size_t>(projectile.frame / cadence);
    if (projectile.originalType == 14) {
        index = std::min(index, frames.size() - 1);
    } else {
        index %= frames.size();
    }
    return frames[index];
}

void renderMatch() {
    drawBitmap(g_app.matchBackdrops[static_cast<std::size_t>(g_app.matchStage)], kArtX, kArtY);
    const auto& leftPaddle = activePaddleSprite(0);
    const auto& rightPaddle = activePaddleSprite(1);
    if (g_app.currentKode != VersusKode::invisiblePaddles) {
        drawSprite(leftPaddle, kArtX + static_cast<int>(g_app.player1X),
                   kArtY + static_cast<int>(g_app.player1Y));
        drawSprite(rightPaddle, kArtX + static_cast<int>(g_app.player2X),
                   kArtY + static_cast<int>(g_app.player2Y), true);
    }
    for (const auto& projectile : g_app.projectiles) {
        if (!projectile.active) continue;
        const auto& sprite = activeProjectileSprite(projectile);
        drawSprite(sprite, kArtX + static_cast<int>(projectile.x),
                   kArtY + static_cast<int>(projectile.y), projectile.owner == 1);
    }
    const bool showBall = !g_app.roundIntroActive &&
        g_app.currentKode != VersusKode::ballDisabled &&
        g_app.currentKode != VersusKode::invisibleBall;
    if (showBall) {
        const auto& ball = activeBallSprite();
        if (g_app.ballEffects[0].visible) {
            drawSprite(ball, kArtX + static_cast<int>(g_app.ballX),
                       kArtY + static_cast<int>(g_app.ballY));
        }
        if (g_app.ballEffects[0].echoActive) {
            drawSprite(ball, kArtX + static_cast<int>(g_app.ballEffects[0].echoX),
                       kArtY + static_cast<int>(g_app.ballEffects[0].echoY));
        }
        if (g_app.secondaryBallActive) {
            if (g_app.ballEffects[1].visible) {
                drawSprite(ball, kArtX + static_cast<int>(g_app.secondaryBallX),
                           kArtY + static_cast<int>(g_app.secondaryBallY));
            }
            if (g_app.ballEffects[1].echoActive) {
                drawSprite(ball, kArtX + static_cast<int>(g_app.ballEffects[1].echoX),
                           kArtY + static_cast<int>(g_app.ballEffects[1].echoY));
            }
        }
    }

    // The original HUD resources are 400/401/402/403 in type 2004.  Their
    // frame geometry and fixed 544x432 coordinates are preserved here; only
    // the mutable interiors are painted before the labels are overlaid.
    if (g_app.currentKode != VersusKode::hiddenBars) {
    constexpr int leftHealthX = kArtX + 50;
    constexpr int rightHealthX = kArtX + 302;
    constexpr int healthY = kArtY + 50;
    drawSprite(g_app.healthFrame, leftHealthX, healthY);
    drawSprite(g_app.healthFrame, rightHealthX, healthY, true);
    drawGaugeFill(leftHealthX + 3, healthY + 3, 186, 16, g_app.health[0],
                  kMaximumHealth, RGB(0, 0, 145));
    drawGaugeFill(rightHealthX + 3, healthY + 3, 186, 16, g_app.health[1],
                  kMaximumHealth, RGB(0, 0, 145), true);

    constexpr int turboY = kArtY + 73;
    drawSprite(g_app.turboFrame, leftHealthX, turboY);
    drawSprite(g_app.turboFrame, kArtX + 430, turboY, true);
    drawGaugeFill(leftHealthX + 3, turboY + 3, 58, 8, g_app.turbo[0],
                  kMaximumTurbo, RGB(0, 238, 0));
    drawGaugeFill(kArtX + 433, turboY + 3, 58, 8, g_app.turbo[1],
                  kMaximumTurbo, RGB(0, 238, 0), true);

    const auto& leftName = g_app.fighterNameSprites[g_app.selectedCharacters[0]];
    const auto& rightName = g_app.fighterNameSprites[g_app.selectedCharacters[1]];
    drawSprite(leftName, leftHealthX + 7, healthY + 4);
    drawSprite(rightName, rightHealthX + 185 - rightName.width, healthY + 4);

    const int visibleSuper0 = g_app.cheats[2] ? kMaximumSuper : g_app.super[0];
    const int visibleSuper1 = g_app.cheats[2] ? kMaximumSuper : g_app.super[1];
    constexpr int superY = kArtY + 378;
    drawSprite(g_app.superFrames[visibleSuper0 >= kMaximumSuper], kArtX + 50, superY);
    drawSprite(g_app.superFrames[visibleSuper1 >= kMaximumSuper], kArtX + 334, superY);
    drawGaugeFill(kArtX + 53, superY + 19, 154, 10, visibleSuper0, kMaximumSuper,
                  RGB(115, 0, 0));
    drawGaugeFill(kArtX + 337, superY + 19, 154, 10, visibleSuper1, kMaximumSuper,
                  RGB(115, 0, 0), true);

    drawSmallText("WINS:", kArtX + 50, kArtY + 22, RGB(245, 0, 0), 15);
    drawSmallText("WINS:", kArtX + 426, kArtY + 22, RGB(245, 0, 0), 15);
    char score1[3]{static_cast<char>('0' + (g_app.score[0] / 10) % 10),
                   static_cast<char>('0' + g_app.score[0] % 10), 0};
    char score2[3]{static_cast<char>('0' + (g_app.score[1] / 10) % 10),
                   static_cast<char>('0' + g_app.score[1] % 10), 0};
    drawSmallText(score1, kArtX + 99, kArtY + 22, RGB(245, 0, 0), 15);
    drawSmallText(score2, kArtX + 475, kArtY + 22, RGB(245, 0, 0), 15);
    for (int win = 0; win < std::min(2, g_app.score[0]); ++win) {
        drawSprite(g_app.roundWinMarker, kArtX + 244 + win * 40, kArtY + 20);
    }
    for (int win = 0; win < std::min(2, g_app.score[1]); ++win) {
        drawSprite(g_app.roundWinMarker, kArtX + 284 - win * 40, kArtY + 20);
    }
    }

    if (g_app.roundIntroActive && g_app.roundIntroStage > 0) {
        const SpriteAsset* intro{};
        int x{};
        int y = kArtY + 185;
        if (g_app.roundIntroStage <= 18) {
            intro = &g_app.roundFrames[static_cast<std::size_t>(g_app.roundIntroStage - 1)];
            x = kArtX + 176;
        } else if (g_app.roundIntroStage <= 36) {
            const int round = std::clamp(g_app.roundNumber, 1, 3) - 1;
            intro = &g_app.roundNumberFrames[static_cast<std::size_t>(round)]
                                                   [static_cast<std::size_t>(g_app.roundIntroStage - 19)];
            constexpr std::array<int, 3> widths{28, 42, 44};
            x = kArtX + 272 - widths[static_cast<std::size_t>(round)] / 2;
        } else {
            int frame{};
            if (g_app.roundIntroStage <= 42) frame = g_app.roundIntroStage - 37;
            else if (g_app.roundIntroStage <= 49) frame = 5;
            else frame = g_app.roundIntroStage - 44;
            intro = &g_app.fightFrames[static_cast<std::size_t>(std::clamp(frame, 0, 14))];
            x = kArtX + 164;
            y = kArtY + 116;
        }
        if (intro) drawSprite(*intro, x, y);
    }

    if (g_app.matchPhase == MatchPhase::betweenRounds && g_app.roundWinner >= 0) {
        std::string message(kCharacterNames[static_cast<std::size_t>(
            g_app.selectedCharacters[static_cast<std::size_t>(g_app.roundWinner)])]);
        message += " WINS";
        drawTextLine(message, kArtY + 188, 28, RGB(220, 0, 10), FW_HEAVY, true);
    } else if (g_app.matchPhase == MatchPhase::finishPrompt && g_app.roundWinner >= 0) {
        const int loser = 1 - g_app.roundWinner;
        const int loserCharacter = g_app.selectedCharacters[static_cast<std::size_t>(loser)];
        const int stage = std::clamp(g_app.matchPhaseTicks / 5, 0, 27);
        int frame = stage <= 5 ? stage : (stage < 20 ? 5 : stage - 14);
        frame = std::clamp(frame, 0, 13);
        const auto& frames = kUsesFinishHer[static_cast<std::size_t>(loserCharacter)]
            ? g_app.finishHerFrames : g_app.finishHimFrames;
        const auto& prompt = frames[static_cast<std::size_t>(frame)];
        drawSprite(prompt, kArtX + 272 - prompt.width / 2,
                   kArtY + 216 - prompt.height / 2);
    } else if (g_app.matchPhase == MatchPhase::matchResult && g_app.roundWinner >= 0) {
        if (g_app.fatalityPerformed) {
            drawSprite(g_app.fatalitySprite,
                       kArtX + 272 - g_app.fatalitySprite.width / 2,
                       kArtY + 230 - g_app.fatalitySprite.height / 2);
        }
        std::string message(kCharacterNames[static_cast<std::size_t>(
            g_app.selectedCharacters[static_cast<std::size_t>(g_app.roundWinner)])]);
        message += " WINS";
        drawTextLine(message, kArtY + 145, 28, RGB(220, 0, 10), FW_HEAVY, true);
        drawTextLine("ENTER TO CONTINUE", kArtY + 350, 13,
                     RGB(220, 220, 220), FW_NORMAL);
    }
}

void renderContinuePrompt() {
    renderMatch();
    drawBitmap(g_app.continuePanel, kArtX + 172, kArtY + 138);
    const int credits = std::clamp(g_app.continues, 0, 9);
    const int countdown = std::clamp(g_app.continueCountdown, 0, 9);
    drawBitmap(g_app.continueDigits[static_cast<std::size_t>(credits)],
               kArtX + 264, kArtY + 206);
    drawBitmap(g_app.continueDigits[static_cast<std::size_t>(countdown)],
               kArtX + 322, kArtY + 249);
}

void renderGameOver() {
    drawBitmap(g_app.matchBackdrops[0], kArtX, kArtY);
    drawTextLine("GAME OVER", kArtY + 190, 54, RGB(205, 0, 10), FW_HEAVY, true);
    drawTextLine("ENTER TO RETURN", kArtY + 330, 13,
                 RGB(220, 220, 225), FW_NORMAL);
}

void renderCheatMenu() {
    renderMatch();
    RECT panel{kArtX + 116, kArtY + 57, kArtX + 428, kArtY + 375};
    HBRUSH panelBrush = CreateSolidBrush(RGB(18, 0, 3));
    FillRect(g_app.backDc, &panel, panelBrush);
    DeleteObject(panelBrush);
    drawOutline(panel.left, panel.top, panel.right, panel.bottom, RGB(185, 10, 20), 3);
    drawTextLine("KHEAT MENU", 94, 27, RGB(235, 18, 25), FW_HEAVY, true);
    constexpr std::array<std::string_view, 6> labels{
        "PLAYER 1 INVULNERABLE", "PLAYER 2 INVULNERABLE", "INFINITE SUPER",
        "FREEZE BALL", "SLOW MOTION", "UNLOCK ALL CONTENT"};
    for (int index = 0; index < static_cast<int>(labels.size()); ++index) {
        const COLORREF color = index == g_app.cheatSelection ? RGB(255, 190, 35) : RGB(235, 235, 235);
        drawSmallText(labels[index], kArtX + 151, kArtY + 112 + index * 35, color, 14);
        drawSmallText(g_app.cheats[index] ? "ON" : "OFF", kArtX + 365,
                      kArtY + 112 + index * 35,
                      g_app.cheats[index] ? RGB(55, 235, 95) : RGB(130, 130, 135), 14);
    }
}

std::string controllerLabel(int pad) {
    if (pad < 0) return "KEYBOARD";
    std::string label = "GAMEPAD ";
    label.push_back(static_cast<char>('1' + pad));
    label += g_app.gamepadConnected[static_cast<std::size_t>(pad)] ? "  CONNECTED" : "  NOT FOUND";
    return label;
}

void drawConfigRow(int row, std::string_view label, std::string_view value) {
    const int y = kArtY + 111 + row * 43;
    const bool selected = g_app.configSelection == row;
    drawSmallText(label, kArtX + 90, y,
                  selected ? RGB(255, 190, 35) : RGB(235, 235, 235), 13);
    drawSmallText(value, kArtX + 287, y,
                  selected ? RGB(255, 225, 110) : RGB(130, 205, 235), 12);
}

void renderConfiguration() {
    drawBitmap(g_app.creditsBackdrop, kArtX, kArtY);
    drawBitmap(g_app.configBanner, kArtX + 15, kArtY + 15, true);
    RECT panel{kArtX + 74, kArtY + 84, kArtX + 470, kArtY + 370};
    HBRUSH brush = CreateSolidBrush(RGB(18, 0, 3));
    FillRect(g_app.backDc, &panel, brush);
    DeleteObject(brush);
    drawOutline(panel.left, panel.top, panel.right, panel.bottom, RGB(170, 10, 20), 2);
    const std::string player1 = controllerLabel(g_app.player1Pad);
    const std::string player2 = controllerLabel(g_app.player2Pad);
    const std::string deadZone = std::to_string(g_app.gamepadDeadZone) + "%";
    drawConfigRow(0, "PLAYER 1 INPUT", player1);
    drawConfigRow(1, "PLAYER 2 INPUT", player2);
    drawConfigRow(2, "STICK DEAD ZONE", deadZone);
    drawConfigRow(3, "VIBRATION", g_app.vibrationEnabled ? "ON" : "OFF");
    drawConfigRow(4, "MUSIC AND SOUND", g_app.soundEnabled ? "ON" : "OFF");
    drawConfigRow(5, "RETURN", "");
    drawSmallText("P1 W/S  ATTACK 1/2/3  SUPER 4  TURBO 5",
                  kArtX + 82, kArtY + 378, RGB(145, 145, 150), 10, FW_NORMAL);
    drawSmallText("P2 ARROWS  ATTACK 6/7/8  SUPER 9  TURBO 0",
                  kArtX + 82, kArtY + 394, RGB(145, 145, 150), 10, FW_NORMAL);
}

void render() {
    clearCanvas();
    switch (g_app.screen) {
        case Screen::title: renderTitle(); break;
        case Screen::credits: renderCredits(); break;
        case Screen::characterSelect: renderCharacterSelect(); break;
        case Screen::versusKode: renderVersusKode(); break;
        case Screen::match: renderMatch(); break;
        case Screen::cheatMenu: renderCheatMenu(); break;
        case Screen::configuration: renderConfiguration(); break;
        case Screen::continuePrompt: renderContinuePrompt(); break;
        case Screen::gameOver: renderGameOver(); break;
    }
}

void invalidate() {
    render();
    InvalidateRect(g_app.window, nullptr, FALSE);
}

void toggleFullscreen() {
    if (!g_app.fullscreen) {
        g_app.savedStyle = static_cast<DWORD>(GetWindowLongPtrW(g_app.window, GWL_STYLE));
        g_app.savedExStyle = static_cast<DWORD>(GetWindowLongPtrW(g_app.window, GWL_EXSTYLE));
        GetWindowRect(g_app.window, &g_app.savedWindowRect);
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromWindow(g_app.window, MONITOR_DEFAULTTONEAREST), &monitor);
        SetWindowLongPtrW(g_app.window, GWL_STYLE, g_app.savedStyle & ~WS_OVERLAPPEDWINDOW);
        SetWindowLongPtrW(g_app.window, GWL_EXSTYLE, g_app.savedExStyle & ~WS_EX_WINDOWEDGE);
        SetWindowPos(g_app.window, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                     monitor.rcMonitor.right - monitor.rcMonitor.left,
                     monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        g_app.fullscreen = true;
    } else {
        SetWindowLongPtrW(g_app.window, GWL_STYLE, g_app.savedStyle);
        SetWindowLongPtrW(g_app.window, GWL_EXSTYLE, g_app.savedExStyle);
        SetWindowPos(g_app.window, nullptr, g_app.savedWindowRect.left, g_app.savedWindowRect.top,
                     g_app.savedWindowRect.right - g_app.savedWindowRect.left,
                     g_app.savedWindowRect.bottom - g_app.savedWindowRect.top,
                     SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER);
        g_app.fullscreen = false;
    }
    InvalidateRect(g_app.window, nullptr, FALSE);
}

void initializeTournament() {
    const std::size_t variant = static_cast<std::size_t>(GetTickCount64() & 3);
    g_app.activeLadder = kTournamentLadders[variant];
    g_app.ladderIndex = 0;
    g_app.continues = 5;
    g_app.realmTransportPending = false;
    g_app.realmMatchActive = false;
    g_app.realmReturnWinner = -1;
    g_app.selectedCharacters[1] = g_app.activeLadder[0];
}

void showCpuLadder() {
    g_app.selectedCharacters[1] =
        g_app.activeLadder[static_cast<std::size_t>(g_app.ladderIndex)];
    g_app.screen = Screen::versusKode;
    g_app.currentMusic = nullptr;
    g_app.musicPosition = 0.0;
    stopAllEffects();
    playEffect(g_app.ladderSound, 1.0f);
    invalidate();
}

void showGameOver() {
    g_app.realmTransportPending = false;
    g_app.realmMatchActive = false;
    g_app.realmReturnWinner = -1;
    g_app.screen = Screen::gameOver;
    g_app.currentMusic = nullptr;
    g_app.musicPosition = 0.0;
    stopAllEffects();
    const std::size_t cue = static_cast<std::size_t>((GetTickCount64() >> 4) & 1);
    playEffect(g_app.gameOverSounds[cue], 1.0f);
    invalidate();
}

void activateTitleSelection() {
    playEffect(g_app.menuSelectSound);
    switch (g_app.titleSelection) {
        case 0:
            g_app.playerCount = 1;
            g_app.selectingPlayer = 0;
            initializeTournament();
            g_app.screen = Screen::characterSelect;
            break;
        case 1:
            g_app.playerCount = 2;
            g_app.selectingPlayer = 0;
            g_app.screen = Screen::characterSelect;
            break;
        case 2: g_app.screen = Screen::configuration; break;
        case 3: g_app.screen = Screen::credits; break;
        case 4: PostMessageW(g_app.window, WM_CLOSE, 0, 0); return;
    }
    invalidate();
}

void resetBall() {
    const auto& sprite = activeBallSprite();
    g_app.ballX = 272.0f - static_cast<float>(sprite.width / 2);
    g_app.ballY = 216.0f - static_cast<float>(sprite.height / 2);
    g_app.ballBaseSpeed = g_app.currentKode == VersusKode::fastBall ? 8
        : (g_app.currentKode == VersusKode::slowBall ? 4 : kDefaultBallSpeed);
    g_app.ballDamage = g_app.currentKode == VersusKode::ballDoubleDamage ? 60
        : (g_app.currentKode == VersusKode::ballHalfDamage ? 15 : kDefaultBallDamage);
    g_app.ballCollisionArmed = {true, true};
    g_app.ballCrazyTicks = 0;
    // 0x00412B44 and 0x00412B5A call GetTickCount independently and use bit
    // zero to choose the sign of each five-pixel component.
    g_app.ballVelocityX = (GetTickCount() & 1) != 0
        ? -static_cast<float>(g_app.ballBaseSpeed)
        : static_cast<float>(g_app.ballBaseSpeed);
    g_app.ballVelocityY = (GetTickCount() & 1) != 0
        ? -static_cast<float>(g_app.ballBaseSpeed)
        : static_cast<float>(g_app.ballBaseSpeed);
    g_app.secondaryBallActive = g_app.currentKode == VersusKode::secondaryBall ||
        g_app.currentKode == VersusKode::decoyBall;
    g_app.secondaryBallX = g_app.ballX;
    g_app.secondaryBallY = g_app.ballY;
    g_app.secondaryBallVelocityX = -g_app.ballVelocityX;
    g_app.secondaryBallVelocityY = (GetTickCount() & 1) != 0
        ? -static_cast<float>(g_app.ballBaseSpeed)
        : static_cast<float>(g_app.ballBaseSpeed);
    g_app.secondaryBallCollisionArmed = {true, true};
    g_app.ballEffects = {};
    for (auto& effect : g_app.ballEffects) effect.returnBase = g_app.ballBaseSpeed;
}

void beginRound() {
    // 0x0040D3A2/0x0040D4FC initialize the original 12x54 collision boxes at
    // x=50 and x=482. The two horizontal movement regions are 0..200 and
    // 344..544; the shared vertical region is 0..432.
    g_app.player1X = 50.0f;
    g_app.player1Y = 189.0f;
    g_app.player2X = 482.0f;
    g_app.player2Y = 189.0f;
    g_app.health = {kMaximumHealth, kMaximumHealth};
    if (g_app.currentKode == VersusKode::player1HalfEnergy ||
        g_app.currentKode == VersusKode::bothPlayersHalfEnergy) {
        g_app.health[0] >>= 1;
    } else if (g_app.currentKode == VersusKode::player1QuarterEnergy ||
               g_app.currentKode == VersusKode::bothPlayersQuarterEnergy) {
        g_app.health[0] >>= 2;
    }
    if (g_app.currentKode == VersusKode::player2HalfEnergy ||
        g_app.currentKode == VersusKode::bothPlayersHalfEnergy) {
        g_app.health[1] >>= 1;
    } else if (g_app.currentKode == VersusKode::player2QuarterEnergy ||
               g_app.currentKode == VersusKode::bothPlayersQuarterEnergy) {
        g_app.health[1] >>= 2;
    }
    g_app.turbo = {kMaximumTurbo, kMaximumTurbo};
    g_app.super = {};
    g_app.superActive = {};
    g_app.superDrainTicks = {};
    g_app.animationFrame = {};
    g_app.animationTicks = {};
    g_app.attackCooldown = {};
    g_app.comboHistorySize = {};
    g_app.comboTimeout = {};
    g_app.frozenTicks = {};
    g_app.projectiles = {};
    g_app.paddleAppearance = g_app.selectedCharacters;
    g_app.randomPaddleTicks = {};
    g_app.roundIntroStage = 0;
    g_app.roundIntroDelay = 0;
    g_app.roundIntroActive = true;
    g_app.matchPhase = MatchPhase::playing;
    g_app.matchPhaseTicks = 0;
    g_app.roundWinner = -1;
    resetBall();
}

void beginMatch() {
    if (g_app.playerCount == 2) {
        const int enteredKode = kodeFromDigits();
        if (enteredKode == VersusKode::ultimateUnlock) {
            g_app.allContentUnlocked = true;
            g_app.currentKode = 0;
        } else if (enteredKode == VersusKode::randomPaddles &&
                   !g_app.allContentUnlocked) {
            // 0x0040145D clears 555555 in the unregistered/shareware state.
            g_app.currentKode = 0;
        } else {
            g_app.currentKode = enteredKode;
        }
    } else {
        g_app.currentKode = 0;
    }
    g_app.score = {};
    g_app.roundNumber = 1;
    g_app.fatalityPerformed = false;
    g_app.matchStage = static_cast<int>((GetTickCount64() / 17 +
        g_app.selectedCharacters[0] * 3 + g_app.selectedCharacters[1]) % 3);
    beginRound();
    g_app.screen = Screen::match;
    playMatchMusic();
    invalidate();
}

void showContinuePrompt() {
    g_app.continueCountdown = 9;
    g_app.continueCountdownTicks = 0;
    g_app.screen = Screen::continuePrompt;
    invalidate();
}

void acceptContinue() {
    if (g_app.continues <= 0) {
        showGameOver();
        return;
    }
    --g_app.continues;
    beginMatch();
}

void advanceAfterMatch() {
    if (g_app.playerCount != 1) {
        g_app.screen = Screen::title;
        playTitleMusic();
        invalidate();
        return;
    }
    if (g_app.realmMatchActive) {
        const bool realmVictory = g_app.roundWinner == 0;
        const int returnWinner = g_app.realmReturnWinner;
        g_app.realmMatchActive = false;
        g_app.realmTransportPending = false;
        g_app.realmReturnWinner = -1;
        if (!realmVictory) {
            showGameOver();
            return;
        }
        // Winning the forced fighter-number-7 encounter returns to the normal
        // ladder result without consuming an additional battle position.
        if (returnWinner == 0) {
            if (++g_app.ladderIndex >= static_cast<int>(g_app.activeLadder.size())) {
                showGameOver();
            } else {
                showCpuLadder();
            }
        } else {
            g_app.selectedCharacters[1] =
                g_app.activeLadder[static_cast<std::size_t>(g_app.ladderIndex)];
            showContinuePrompt();
        }
        return;
    }
    if (g_app.realmTransportPending) {
        // 0x0041507C forces original fighter number 7 (zero-based index 6)
        // into a one-player match before normal ladder bookkeeping resumes.
        g_app.realmReturnWinner = g_app.roundWinner;
        g_app.realmMatchActive = true;
        g_app.selectedCharacters[1] = 6;
        beginMatch();
        return;
    }
    if (g_app.roundWinner == 0) {
        if (++g_app.ladderIndex >= static_cast<int>(g_app.activeLadder.size())) {
            showGameOver();
        } else {
            showCpuLadder();
        }
    } else {
        showContinuePrompt();
    }
}

bool handleSecretShortcut() {
    if (g_app.screen == Screen::versusKode) {
        // The original special path compares the six-digit value to 111999.
        // Populate that exact visible kode before enabling the reconstructed
        // full-version flag; no on-screen text advertises this shortcut.
        g_app.kodeDigits = {1, 1, 1, 9, 9, 9};
        g_app.allContentUnlocked = true;
        g_app.cheats[5] = true;
        invalidate();
        return true;
    }
    if (g_app.screen == Screen::match) {
        g_app.screenBelowCheats = g_app.screen;
        g_app.screen = Screen::cheatMenu;
        invalidate();
        return true;
    }
    if (g_app.screen == Screen::cheatMenu) {
        g_app.screen = g_app.screenBelowCheats;
        invalidate();
        return true;
    }
    return false;
}

int cycleController(int current, int direction) {
    constexpr int count = XUSER_MAX_COUNT + 1;
    int slot = current + 1;
    slot = (slot + direction + count) % count;
    return slot - 1;
}

void adjustConfiguration(int direction) {
    switch (g_app.configSelection) {
        case 0: g_app.player1Pad = cycleController(g_app.player1Pad, direction); break;
        case 1: g_app.player2Pad = cycleController(g_app.player2Pad, direction); break;
        case 2:
            g_app.gamepadDeadZone += direction * 4;
            if (g_app.gamepadDeadZone > 40) g_app.gamepadDeadZone = 12;
            if (g_app.gamepadDeadZone < 12) g_app.gamepadDeadZone = 40;
            break;
        case 3:
            g_app.vibrationEnabled = !g_app.vibrationEnabled;
            if (!g_app.vibrationEnabled && g_app.xinputSetState) {
                XINPUT_VIBRATION vibration{};
                for (DWORD pad = 0; pad < XUSER_MAX_COUNT; ++pad) {
                    g_app.xinputSetState(pad, &vibration);
                    g_app.vibrationTicks[pad] = 0;
                }
            }
            break;
        case 4:
            g_app.soundEnabled = !g_app.soundEnabled;
            playTitleMusic();
            break;
        default: break;
    }
    invalidate();
}

bool gamepadButtonPressed(std::size_t pad, WORD button) {
    return g_app.gamepadConnected[pad] &&
           (g_app.gamepads[pad].Gamepad.wButtons & button) &&
           !(g_app.previousGamepads[pad].Gamepad.wButtons & button);
}

void pollGamepads() {
    if (!g_app.xinputGetState) return;
    for (DWORD pad = 0; pad < XUSER_MAX_COUNT; ++pad) {
        g_app.previousGamepads[pad] = g_app.gamepads[pad];
        XINPUT_STATE state{};
        g_app.gamepadConnected[pad] = g_app.xinputGetState(pad, &state) == ERROR_SUCCESS;
        g_app.gamepads[pad] = state;
        if (g_app.vibrationTicks[pad] > 0 && --g_app.vibrationTicks[pad] == 0 &&
            g_app.xinputSetState) {
            XINPUT_VIBRATION vibration{};
            g_app.xinputSetState(pad, &vibration);
        }
    }
}

void postGamepadMenuInput() {
    if (g_app.screen == Screen::match && g_app.matchPhase != MatchPhase::matchResult) return;
    for (std::size_t pad = 0; pad < XUSER_MAX_COUNT; ++pad) {
        if (!g_app.gamepadConnected[pad]) continue;
        if (gamepadButtonPressed(pad, XINPUT_GAMEPAD_DPAD_UP)) PostMessageW(g_app.window, WM_KEYDOWN, VK_UP, 0);
        if (gamepadButtonPressed(pad, XINPUT_GAMEPAD_DPAD_DOWN)) PostMessageW(g_app.window, WM_KEYDOWN, VK_DOWN, 0);
        if (gamepadButtonPressed(pad, XINPUT_GAMEPAD_DPAD_LEFT)) PostMessageW(g_app.window, WM_KEYDOWN, VK_LEFT, 0);
        if (gamepadButtonPressed(pad, XINPUT_GAMEPAD_DPAD_RIGHT)) PostMessageW(g_app.window, WM_KEYDOWN, VK_RIGHT, 0);
        if (gamepadButtonPressed(pad, XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_START)) PostMessageW(g_app.window, WM_KEYDOWN, VK_RETURN, 0);
        if (gamepadButtonPressed(pad, XINPUT_GAMEPAD_B | XINPUT_GAMEPAD_BACK)) PostMessageW(g_app.window, WM_KEYDOWN, VK_ESCAPE, 0);
        break;
    }
}

float gamepadVertical(int pad) {
    if (pad < 0 || pad >= XUSER_MAX_COUNT || !g_app.gamepadConnected[static_cast<std::size_t>(pad)]) return 0.0f;
    const auto& gamepad = g_app.gamepads[static_cast<std::size_t>(pad)].Gamepad;
    if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) return -1.0f;
    if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) return 1.0f;
    const float normalized = static_cast<float>(gamepad.sThumbLY) / 32767.0f;
    const float deadZone = static_cast<float>(g_app.gamepadDeadZone) / 100.0f;
    if (std::abs(normalized) <= deadZone) return 0.0f;
    return normalized > 0.0f ? -1.0f : 1.0f;
}

float gamepadHorizontal(int pad) {
    if (pad < 0 || pad >= XUSER_MAX_COUNT ||
        !g_app.gamepadConnected[static_cast<std::size_t>(pad)]) return 0.0f;
    const auto& gamepad = g_app.gamepads[static_cast<std::size_t>(pad)].Gamepad;
    if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) return -1.0f;
    if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) return 1.0f;
    const float normalized = static_cast<float>(gamepad.sThumbLX) / 32767.0f;
    const float deadZone = static_cast<float>(g_app.gamepadDeadZone) / 100.0f;
    if (std::abs(normalized) <= deadZone) return 0.0f;
    return normalized > 0.0f ? 1.0f : -1.0f;
}

void damagePlayer(int player, int amount) {
    if (g_app.matchPhase != MatchPhase::playing || g_app.roundIntroActive) return;
    if (g_app.cheats[static_cast<std::size_t>(player)]) return;
    const int pad = player == 0 ? g_app.player1Pad : g_app.player2Pad;
    if (g_app.vibrationEnabled && g_app.xinputSetState && pad >= 0 && pad < XUSER_MAX_COUNT) {
        XINPUT_VIBRATION vibration{32000, 18000};
        g_app.xinputSetState(static_cast<DWORD>(pad), &vibration);
        g_app.vibrationTicks[static_cast<std::size_t>(pad)] = 9;
    }
    g_app.health[player] = std::max(0, g_app.health[player] - amount);
    g_app.animationTicks[player] = 48;
    g_app.animationFrame[player] = 0;
    if (g_app.health[player] == 0) {
        g_app.roundWinner = 1 - player;
        ++g_app.score[static_cast<std::size_t>(g_app.roundWinner)];
        g_app.projectiles = {};
        g_app.matchPhaseTicks = 0;
        g_app.matchPhase = g_app.score[static_cast<std::size_t>(g_app.roundWinner)] >= 2
            ? MatchPhase::finishPrompt : MatchPhase::betweenRounds;
        if (g_app.matchPhase == MatchPhase::finishPrompt) {
            const int defeatedCharacter = g_app.selectedCharacters[static_cast<std::size_t>(player)];
            playEffect(kUsesFinishHer[static_cast<std::size_t>(defeatedCharacter)]
                           ? g_app.finishHerSound : g_app.finishHimSound,
                       1.0f);
        }
    }
}

void launchComponent(int player, int componentIndex) {
    if (g_app.attackCooldown[player] > 0) return;
    const int character = g_app.selectedCharacters[static_cast<std::size_t>(player)];
    if (componentIndex < 1 || componentIndex > 4) return;
    const auto& component = kComponents[static_cast<std::size_t>(character)]
                                       [static_cast<std::size_t>(componentIndex - 1)];
    if (g_app.currentKode == VersusKode::projectilesDisabled) {
        g_app.animationTicks[player] = 48;
        g_app.animationFrame[player] = 12;
        g_app.attackCooldown[player] = 28;
        return;
    }
    if (component.originalType == 0) {
        // Type zero has no independent sprite object in the original. It still
        // enters the fighter's special animation, so retain that visible state.
        g_app.animationTicks[player] = 48;
        g_app.animationFrame[player] = 12;
        g_app.attackCooldown[player] = 28;
        return;
    }
    for (auto& projectile : g_app.projectiles) {
        if (projectile.active) continue;
        projectile.active = true;
        projectile.secondaryPhase = false;
        projectile.hasHit = false;
        projectile.owner = player;
        projectile.damage = component.damage;
        if (g_app.currentKode == VersusKode::projectileDoubleDamage) {
            projectile.damage <<= 1;
        } else if (g_app.currentKode == VersusKode::projectileHalfDamage) {
            projectile.damage >>= 1;
        }
        projectile.originalType = component.originalType;
        projectile.variant = component.variant;
        projectile.mode = component.delay;
        projectile.age = 0;
        projectile.lifetime = 0;
        const auto& sprite = activeProjectileSprite(projectile);
        const auto& paddle = g_app.standingPaddles[static_cast<std::size_t>(character)];
        const float paddleLeft = player == 0 ? g_app.player1X : g_app.player2X;
        const float paddleRight = paddleLeft + paddle.width;
        projectile.x = player == 0 ? paddleRight : paddleLeft - sprite.width;
        projectile.y = (player == 0 ? g_app.player1Y : g_app.player2Y) +
                       (paddle.height - sprite.height) * 0.5f;
        const float speed = component.delay == 11 ? 5.0f : 9.0f;
        projectile.velocityX = player == 0 ? speed : -speed;
        projectile.velocityY = component.delay == 17 ? -3.0f
            : (component.delay == 18 ? 3.0f : 0.0f);
        if (component.originalType == 1) {
            // 0x0041C808 anchors the 516-pixel beam behind the owner and sweeps
            // it toward the opponent at 14 pixels per update.
            projectile.x = player == 0 ? paddleRight - sprite.width : paddleLeft;
            projectile.velocityX = player == 0 ? 14.0f : -14.0f;
            g_app.super[player] = std::min(kMaximumSuper, g_app.super[player] + 30);
        } else if (component.originalType == 7) {
            // 0x0041C5F0 launches this vertically from the owner's upper edge.
            // After it clears the top, 0x0041C658 relocates variant 1/2/3 over
            // the far side of the arena and drops it back at nine pixels/tick.
            projectile.x = paddleLeft - 6.0f;
            projectile.y = (player == 0 ? g_app.player1Y : g_app.player2Y) +
                           paddle.height - sprite.height;
            projectile.velocityX = 0.0f;
            projectile.velocityY = -9.0f;
        } else if (component.originalType == 8) {
            // The ice double is mirrored across the original 544-pixel playfield
            // and persists for exactly 100 updates (0x0041C4CC/0x0041C57C).
            projectile.x = 544.0f - paddleRight;
            projectile.y = player == 0 ? g_app.player1Y : g_app.player2Y;
            projectile.velocityX = 0.0f;
            projectile.velocityY = 0.0f;
            projectile.lifetime = 100;
        } else if (component.originalType == 9) {
            // Nai Palm's four modes are the original extra-ball diagonals from
            // 0x0041D328. Modes 6/8 use 6x6 velocity; 7/9 use 7x5.
            const bool steep = component.delay == 6 || component.delay == 8;
            projectile.velocityX = player == 0 ? (steep ? 6.0f : 7.0f)
                                                : (steep ? -6.0f : -7.0f);
            projectile.velocityY = (component.delay == 6 || component.delay == 7)
                ? -static_cast<float>(steep ? 6 : 5)
                : static_cast<float>(steep ? 6 : 5);
        } else if (component.originalType == 11) {
            // One Eye's moving double copies the owner's paddle and travels at
            // the exact nine-pixel horizontal velocity from 0x0041CFCC.
            projectile.x = paddleLeft;
            projectile.y = player == 0 ? g_app.player1Y : g_app.player2Y;
            projectile.velocityX = player == 0 ? 9.0f : -9.0f;
            projectile.velocityY = 0.0f;
        } else if (component.originalType == 14) {
            // The 60-stage giant effect is bottom-anchored against the far edge
            // (0x0041CA58) and animates in place rather than flying laterally.
            projectile.x = player == 0 ? 544.0f - sprite.width : 0.0f;
            projectile.y = 432.0f - sprite.height;
            projectile.velocityX = 0.0f;
            projectile.velocityY = 0.0f;
            projectile.lifetime = 300;
        } else if (component.originalType == 17) {
            // Omoh's lob starts at -8 vertical velocity. Mode 22 is seven
            // pixels/tick horizontally; its other mode is five (0x0041D1DC).
            const float speed = component.delay == 22 ? 7.0f : 5.0f;
            projectile.velocityX = player == 0 ? speed : -speed;
            projectile.velocityY = -8.0f;
        } else if (component.originalType == 20) {
            // Pain's full-height apparition is reflected to the opposite side
            // and bottom-aligned with its owner by 0x0041D694.
            projectile.x = 544.0f - paddleRight;
            projectile.y = (player == 0 ? g_app.player1Y : g_app.player2Y) +
                           paddle.height - sprite.height;
            projectile.velocityX = 0.0f;
            projectile.velocityY = 0.0f;
            projectile.lifetime = 30;
        } else if (component.originalType == 24) {
            // Dawg Cau's falling column starts near the lower boundary, rises
            // at four pixels/tick and drifts left or right by two.
            const int seed = static_cast<int>((g_app.frameCounter + player * 29) & 63);
            projectile.x = player == 0 ? paddleLeft + seed
                                       : paddleRight - sprite.width - seed;
            projectile.y = 452.0f - sprite.height;
            projectile.velocityX = ((g_app.frameCounter >> 2) & 1) ? 2.0f : -2.0f;
            projectile.velocityY = -4.0f;
        }
        projectile.frame = 0;
        g_app.attackCooldown[player] = 28;
        g_app.animationTicks[player] = 48;
        g_app.animationFrame[player] = 12;
        if (component.originalType == 4 && component.delay == 11) {
            playEffect(g_app.projectileAlternateSound);
        } else if (component.originalType > 0 && component.originalType <
                   static_cast<int>(g_app.projectileSounds.size())) {
            playEffect(g_app.projectileSounds[static_cast<std::size_t>(
                component.originalType)]);
        }
        return;
    }
}

bool recipeMatches(int player, const ComboRecipe& recipe) {
    if (recipe.length <= 0 || g_app.comboHistorySize[player] < recipe.length) return false;
    const int first = g_app.comboHistorySize[player] - recipe.length;
    for (int index = 0; index < recipe.length; ++index) {
        if (g_app.comboHistory[player][static_cast<std::size_t>(first + index)] !=
            recipe.buttons[static_cast<std::size_t>(index)]) return false;
    }
    return true;
}

void processCombatButton(int player, CombatButton button) {
    auto& size = g_app.comboHistorySize[player];
    auto& history = g_app.comboHistory[player];
    if (size == static_cast<int>(history.size())) {
        std::move(history.begin() + 1, history.end(), history.begin());
        --size;
    }
    history[static_cast<std::size_t>(size++)] = button;
    g_app.comboTimeout[player] = 60;

    // 0x0040D064 arms the fighter's +0x754 Super flag without consuming the
    // input from the component recognizer. The separate status object drains
    // the +0x104 gauge while this flag remains active.
    if (button == CombatButton::super && g_app.matchPhase == MatchPhase::playing &&
        !g_app.roundIntroActive && !g_app.superActive[static_cast<std::size_t>(player)] &&
        (g_app.super[static_cast<std::size_t>(player)] >= kMaximumSuper ||
         g_app.cheats[2])) {
        g_app.super[static_cast<std::size_t>(player)] = kMaximumSuper;
        g_app.superActive[static_cast<std::size_t>(player)] = true;
        g_app.superDrainTicks[static_cast<std::size_t>(player)] = 0;
    }

    const int character = g_app.selectedCharacters[static_cast<std::size_t>(player)];
    for (const auto& recipe : kComboRecipes[static_cast<std::size_t>(character)]) {
        if (!recipeMatches(player, recipe)) continue;
        size = 0;
        g_app.comboTimeout[player] = 0;
        if (recipe.realmTransport) {
            // 0x00408248 is not the fatality activator. It is a guarded
            // one-player secret: full version, arena number 2, then
            // SUPER, SUPER, TURBO, TURBO. The original records a pending
            // transport and later forces fighter number 7 as the opponent.
            if (player == 0 && g_app.playerCount == 1 &&
                g_app.matchPhase == MatchPhase::playing && !g_app.roundIntroActive &&
                g_app.allContentUnlocked && g_app.matchStage == 1 &&
                !g_app.realmTransportPending && !g_app.realmMatchActive) {
                g_app.realmTransportPending = true;
                g_app.animationTicks[0] = 48;
                g_app.animationFrame[0] = 12;
                playEffect(g_app.realmTransportSound, 1.0f);
            }
        } else if (recipe.component > 0) {
            if (g_app.matchPhase == MatchPhase::finishPrompt &&
                g_app.roundWinner == player) {
                launchComponent(player, recipe.component);
                g_app.fatalityPerformed = true;
                g_app.matchPhase = MatchPhase::matchResult;
                g_app.matchPhaseTicks = 0;
                playEffect(g_app.fatalitySound, 1.0f);
            } else if (g_app.matchPhase == MatchPhase::playing &&
                       !g_app.roundIntroActive) {
                launchComponent(player, recipe.component);
            }
        }
        return;
    }
}

bool gamepadButtonPressedForPlayer(int pad, WORD button) {
    return pad >= 0 && pad < XUSER_MAX_COUNT &&
           gamepadButtonPressed(static_cast<std::size_t>(pad), button);
}

void triggerPaddleAttack(int player, int attack);

bool combatButtonPressed(int player, CombatButton button) {
    const int pad = player == 0 ? g_app.player1Pad : g_app.player2Pad;
    if (pad < 0) return false;
    constexpr std::array<WORD, 5> buttons{
        XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B,
        XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_Y};
    return gamepadButtonPressedForPlayer(pad, buttons[static_cast<std::size_t>(button)]);
}

bool handleKeyboardCombatKey(WPARAM key) {
    constexpr std::array<std::array<WPARAM, 5>, 2> keys{{
        {{'1', '2', '3', '5', '4'}},
        {{'6', '7', '8', '0', '9'}},
    }};
    for (int player = 0; player < 2; ++player) {
        if (player == 1 && g_app.playerCount != 2) continue;
        for (int input = 0; input < 5; ++input) {
            if (key != keys[static_cast<std::size_t>(player)][static_cast<std::size_t>(input)]) {
                continue;
            }
            const auto button = static_cast<CombatButton>(input);
            if (g_app.matchPhase == MatchPhase::playing && !g_app.roundIntroActive) {
                if (input < 3) triggerPaddleAttack(player, input);
                processCombatButton(player, button);
            } else if (g_app.matchPhase == MatchPhase::finishPrompt &&
                       g_app.roundWinner == player) {
                processCombatButton(player, button);
            }
            return true;
        }
    }
    return false;
}

void triggerPaddleAttack(int player, int attack) {
    g_app.animationTicks[player] = std::max(g_app.animationTicks[player], 22);
    g_app.animationFrame[player] = attack * 6;
}

void updateProjectiles() {
    const auto& left = activePaddleSprite(0);
    const auto& right = activePaddleSprite(1);
    for (auto& projectile : g_app.projectiles) {
        if (!projectile.active) continue;
        const auto& sprite = activeProjectileSprite(projectile);

        ++projectile.age;
        if (projectile.originalType == 7 && !projectile.secondaryPhase &&
            projectile.y + sprite.height < -100.0f) {
            projectile.secondaryPhase = true;
            projectile.velocityY = 9.0f;
            constexpr std::array<float, 3> leftDropX{510.0f, 432.0f, 354.0f};
            constexpr std::array<float, 3> rightDropX{10.0f, 88.0f, 166.0f};
            const int variant = std::clamp(projectile.variant, 1, 3) - 1;
            projectile.x = projectile.owner == 0
                ? leftDropX[static_cast<std::size_t>(variant)]
                : rightDropX[static_cast<std::size_t>(variant)];
            playEffect(g_app.projectileSecondarySound);
        }
        if (projectile.originalType == 17 && (projectile.age % 4) == 0) {
            // 0x0041D27C decreases vertical velocity for the first four
            // gravity steps, then increases it by two on every later step.
            const int gravityStep = projectile.age / 4;
            if (gravityStep < 5) projectile.velocityY -= 1.0f;
            else projectile.velocityY += 2.0f;
        }
        if ((projectile.originalType == 2 || projectile.originalType == 3 ||
             projectile.originalType == 4) && !projectile.secondaryPhase) {
            const int width = sprite ? sprite.width : 12;
            const bool enteredOpponentHalf = projectile.owner == 0
                ? projectile.x + width >= 344.0f : projectile.x <= 200.0f;
            if (enteredOpponentHalf) {
                projectile.secondaryPhase = true;
                // Mode dispatch table at 0x0041BB16. The horizontal offsets
                // are mirrored in its player-two table at 0x0041BB9F.
                if (projectile.mode == 12) projectile.velocityY = -4.0f;
                if (projectile.mode == 13) projectile.velocityY = 4.0f;
                if (projectile.mode == 15) projectile.x += projectile.owner == 0 ? -5.0f : 5.0f;
                if (projectile.mode == 16) projectile.x += projectile.owner == 0 ? 6.0f : -6.0f;
            }
        }
        projectile.x += projectile.velocityX;
        projectile.y += projectile.velocityY;
        ++projectile.frame;
        const int width = sprite ? sprite.width : 12;
        const int height = sprite ? sprite.height : 12;

        if (projectile.originalType == 9 &&
            (projectile.y < 0.0f || projectile.y + height > 432.0f)) {
            projectile.y = std::clamp(projectile.y, 0.0f,
                                      std::max(0.0f, 432.0f - height));
            projectile.velocityY = -projectile.velocityY;
            playEffect(g_app.ballBounceSound, 0.7f);
        }
        if (projectile.originalType == 24) {
            const float minimumX = projectile.owner == 0 ? 10.0f : 282.0f;
            const float maximumX = projectile.owner == 0 ? 262.0f : 534.0f - width;
            if (projectile.x < minimumX || projectile.x > maximumX) {
                projectile.x = std::clamp(projectile.x, minimumX, maximumX);
                projectile.velocityX = -projectile.velocityX;
            }
        }

        const int target = 1 - projectile.owner;
        const auto& targetSprite = target == 0 ? left : right;
        const float targetX = target == 0 ? g_app.player1X : g_app.player2X;
        const float targetY = target == 0 ? g_app.player1Y : g_app.player2Y;
        const bool armed = projectile.originalType != 8 || projectile.age >= 20;
        const bool collides = armed && projectile.x + width >= targetX &&
                              projectile.x <= targetX + targetSprite.width &&
                              projectile.y + height >= targetY &&
                              projectile.y <= targetY + targetSprite.height;
        if (collides && !projectile.hasHit) {
            projectile.hasHit = true;
            playEffect(g_app.ballHitSound, 0.8f);
            if (projectile.originalType == 6 || projectile.originalType == 7 ||
                projectile.originalType == 8) {
                // So Frio's zero-damage effects replace the target's behavior
                // callbacks on contact. Preserve that as the corresponding
                // temporary frozen-control state rather than treating zero as
                // a conventional damage projectile.
                g_app.frozenTicks[static_cast<std::size_t>(target)] = 100;
            } else if (projectile.originalType == 11) {
                // The moving double reverses after contact in 0x0041D044 and
                // retreats through the side from which it was launched.
                projectile.velocityX = -projectile.velocityX;
                projectile.secondaryPhase = true;
            } else if (projectile.originalType == 20) {
                g_app.frozenTicks[static_cast<std::size_t>(target)] =
                    std::max(g_app.frozenTicks[static_cast<std::size_t>(target)], 30);
            } else {
                damagePlayer(target, projectile.damage);
                g_app.super[static_cast<std::size_t>(projectile.owner)] =
                    std::min(kMaximumSuper,
                             g_app.super[static_cast<std::size_t>(projectile.owner)] + 8);
            }
            if (projectile.originalType != 11 && projectile.originalType != 20) {
                projectile.active = false;
            }
        } else if (projectile.lifetime > 0 && projectile.age >= projectile.lifetime) {
            projectile.active = false;
        } else if (projectile.originalType == 7 && projectile.secondaryPhase &&
                   projectile.y > 432.0f) {
            projectile.active = false;
        } else if (projectile.originalType == 17 && projectile.y > 432.0f) {
            projectile.active = false;
        } else if (projectile.originalType == 24 && projectile.y + height < 0.0f) {
            projectile.active = false;
        } else if (projectile.x < -width || projectile.x > 544.0f) {
            projectile.active = false;
        }
    }
}

void clearBallEffect(BallEffectState& effect, bool clearEcho = false) {
    effect.active = false;
    effect.visible = true;
    effect.type = 0;
    effect.owner = -1;
    effect.counter = 0;
    effect.returnBase = g_app.ballBaseSpeed;
    effect.reboundPending = false;
    if (clearEcho) effect.echoActive = false;
}

float randomSignedMagnitude(int minimum, int rangeMask) {
    const float magnitude = static_cast<float>(
        minimum + static_cast<int>(GetTickCount() & rangeMask));
    return (GetTickCount() & 1) != 0 ? magnitude : -magnitude;
}

void updateBallEcho(BallEffectState& effect, int ballWidth, int ballHeight,
                    float timeScale) {
    if (!effect.echoActive) return;
    effect.echoX += effect.echoVelocityX * timeScale;
    effect.echoY += effect.echoVelocityY * timeScale;
    if (effect.echoY < 0.0f ||
        effect.echoY + ballHeight > static_cast<float>(kPlayfieldHeight)) {
        effect.echoY = std::clamp(
            effect.echoY, 0.0f,
            static_cast<float>(kPlayfieldHeight - ballHeight));
        effect.echoVelocityY = -effect.echoVelocityY;
    }
    if (effect.echoX + ballWidth < 0.0f ||
        effect.echoX > static_cast<float>(kPlayfieldWidth)) {
        effect.echoActive = false;
    }
}

void updateBallEffectBeforeBounds(BallEffectState& effect, float x, float y,
                                  float& velocityX, float& velocityY,
                                  int ballWidth, int ballHeight) {
    if (!effect.active) return;
    ++effect.counter;
    const bool movingRight = velocityX > 0.0f;
    const float horizontalGap = movingRight
        ? g_app.player2X - (x + ballWidth)
        : x - (g_app.player1X + kFighterCollisionWidth);
    const float targetY = movingRight ? g_app.player2Y : g_app.player1Y;

    switch (effect.type) {
        case 4:
            if (effect.counter >= 6) {
                effect.counter = 0;
                const float horizontal = static_cast<float>(
                    2 + static_cast<int>(GetTickCount() & 7));
                velocityX = velocityX >= 0.0f ? horizontal : -horizontal;
                velocityY = randomSignedMagnitude(2, 7);
            }
            break;
        case 6:
            if (horizontalGap < 100.0f) {
                velocityY = randomSignedMagnitude(6, 0);
                clearBallEffect(effect);
            }
            break;
        case 7:
            if (horizontalGap < 100.0f &&
                std::abs((y + ballHeight * 0.5f) - targetY + 27.0f) < 100.0f) {
                velocityY = -velocityY;
                clearBallEffect(effect);
            }
            break;
        case 9:
            if (effect.reboundPending && std::abs(horizontalGap) > 100.0f) {
                velocityX = -velocityX;
                if ((GetTickCount() & 1) != 0) velocityY = -velocityY;
                clearBallEffect(effect);
            }
            break;
        case 10:
            if (horizontalGap < 200.0f && effect.counter >= 4) {
                effect.counter = 0;
                velocityX += velocityX >= 0.0f ? 1.0f : -1.0f;
                velocityY += velocityY >= 0.0f ? 1.0f : -1.0f;
            }
            break;
        default:
            break;
    }
}

void finishBallEffectAtHorizontalWall(BallEffectState& effect,
                                      float& velocityX, float& velocityY) {
    if (!effect.active) return;
    const int type = effect.type;
    if (type == 1) {
        velocityX = std::copysign(std::max(1.0f, std::abs(velocityX) - 3.0f), velocityX);
        velocityY = std::copysign(std::max(1.0f, std::abs(velocityY) - 3.0f), velocityY);
    } else if (type == 2) {
        velocityX = std::copysign(std::abs(velocityX) + 3.0f, velocityX);
        velocityY = std::copysign(std::abs(velocityY) + 3.0f, velocityY);
    } else if (type == 4 || type == 5 || type == 10) {
        const float base = static_cast<float>(g_app.ballBaseSpeed);
        velocityX = std::copysign(base, velocityX);
        velocityY = std::copysign(base, velocityY);
    }
    clearBallEffect(effect);
}

bool applyPlayerBallEffect(int player, BallEffectState& effect,
                           float x, float y, float& velocityX, float& velocityY) {
    if (!g_app.superActive[static_cast<std::size_t>(player)]) return false;
    const int character = std::clamp(
        g_app.selectedCharacters[static_cast<std::size_t>(player)], 0, 15);
    effect.active = true;
    effect.visible = true;
    effect.type = kBallEffectByCharacter[static_cast<std::size_t>(character)];
    effect.owner = player;
    effect.counter = 0;
    effect.returnBase = g_app.ballBaseSpeed;
    effect.reboundPending = false;

    if (effect.type == 1) effect.returnBase = 8;
    if (effect.type == 2) effect.returnBase = 4;
    if (effect.type == 3) effect.visible = false;
    if (effect.type == 6) {
        velocityX = player == 0 ? 6.0f : -6.0f;
        velocityY = 0.0f;
        return true;
    }
    if (effect.type == 8) {
        velocityX = player == 0 ? 6.0f : -6.0f;
        velocityY = randomSignedMagnitude(4, 0);
        effect.echoActive = true;
        effect.echoX = x;
        effect.echoY = y;
        effect.echoVelocityX = velocityX;
        effect.echoVelocityY = -velocityY;
        return true;
    }
    return false;
}

void updateBallObject(float& x, float& y, float& velocityX, float& velocityY,
                      std::array<bool, 2>& collisionArmed,
                      BallEffectState& effect, bool wallOnly) {
    const auto& sprite = activeBallSprite();
    const int ballWidth = sprite ? sprite.width : 16;
    const int ballHeight = sprite ? sprite.height : 16;
    const float timeScale = g_app.cheats[4] ? 0.45f : 1.0f;
    x += velocityX * timeScale;
    y += velocityY * timeScale;
    updateBallEcho(effect, ballWidth, ballHeight, timeScale);

    if (wallOnly) {
        // The decoy callback at 0x00413868 reflects at every edge, makes no
        // paddle tests, and plays the normal bounce cue only on vertical walls.
        if (y < 0.0f || y + ballHeight > static_cast<float>(kPlayfieldHeight)) {
            y = std::clamp(y, 0.0f, static_cast<float>(kPlayfieldHeight - ballHeight));
            velocityY = -velocityY;
            playEffect(g_app.ballBounceSound, 0.65f);
        }
        if (x < 0.0f || x + ballWidth > static_cast<float>(kPlayfieldWidth)) {
            x = std::clamp(x, 0.0f, static_cast<float>(kPlayfieldWidth - ballWidth));
            velocityX = -velocityX;
        }
        return;
    }

    updateBallEffectBeforeBounds(
        effect, x, y, velocityX, velocityY, ballWidth, ballHeight);

    // 0x004130AF and 0x004131CB treat the left and right playfield edges as
    // damaging walls. The ball rebounds in place and stays in play.
    if (x + ballWidth > static_cast<float>(kPlayfieldWidth)) {
        x = static_cast<float>(kPlayfieldWidth - ballWidth);
        velocityX = -velocityX;
        collisionArmed = {true, true};
        playEffect(g_app.ballHitSound, 0.75f);
        damagePlayer(1, g_app.ballDamage);
        finishBallEffectAtHorizontalWall(effect, velocityX, velocityY);
        if (g_app.matchPhase != MatchPhase::playing) return;
    } else if (x < 0.0f) {
        x = 0.0f;
        velocityX = -velocityX;
        collisionArmed = {true, true};
        playEffect(g_app.ballHitSound, 0.75f);
        damagePlayer(0, g_app.ballDamage);
        finishBallEffectAtHorizontalWall(effect, velocityX, velocityY);
        if (g_app.matchPhase != MatchPhase::playing) return;
    }

    if (y < 0.0f || y + ballHeight > static_cast<float>(kPlayfieldHeight)) {
        const bool hitTop = y < 0.0f;
        y = std::clamp(y, 0.0f, static_cast<float>(kPlayfieldHeight - ballHeight));
        if (effect.active && effect.type == 5) {
            const float horizontal = static_cast<float>(
                3 + static_cast<int>(GetTickCount() & 7));
            velocityX = velocityX >= 0.0f ? horizontal : -horizontal;
            const float vertical = static_cast<float>(
                3 + static_cast<int>(GetTickCount() & 7));
            velocityY = hitTop ? vertical : -vertical;
        } else {
            velocityY = -velocityY;
        }
        collisionArmed = {true, true};
        playEffect(g_app.ballBounceSound, 0.65f);
    }

    const float leftEdge = g_app.player1X + kFighterCollisionWidth;
    const float rightEdge = g_app.player2X;
    const bool touchesPlayer1 = x < leftEdge && x + ballWidth >= g_app.player1X &&
        y + ballHeight >= g_app.player1Y &&
        y < g_app.player1Y + kFighterCollisionHeight;
    if (touchesPlayer1 && collisionArmed[0]) {
        collisionArmed[0] = false;
        collisionArmed[1] = true;
        playEffect(g_app.ballHitSound, 0.75f);
        const bool delayedRebound = effect.active && effect.type == 9 &&
            effect.owner != 0;
        if (delayedRebound) {
            effect.reboundPending = true;
        } else {
            clearBallEffect(effect);
            if (applyPlayerBallEffect(0, effect, x, y, velocityX, velocityY)) return;
        }
        const float base = static_cast<float>(effect.returnBase);
        if (velocityX < 0.0f) {
            if (g_app.player1Y + 20.0f > y + ballHeight) {
                velocityX = base - 1.0f;
                velocityY = -(base + 1.0f);
            } else if (g_app.player1Y + kFighterCollisionHeight - 20.0f < y) {
                velocityX = base - 1.0f;
                velocityY = base + 1.0f;
            } else {
                velocityX = base;
                velocityY = velocityY > 0.0f ? base : -base;
            }
        } else {
            velocityX = base + 1.0f;
            const float reduced = base - 1.0f;
            velocityY = velocityY > 0.0f ? -reduced : reduced;
        }
    }

    const bool touchesPlayer2 = x + ballWidth > rightEdge &&
        x <= g_app.player2X + kFighterCollisionWidth &&
        y + ballHeight >= g_app.player2Y &&
        y < g_app.player2Y + kFighterCollisionHeight;
    if (touchesPlayer2 && collisionArmed[1]) {
        collisionArmed[1] = false;
        collisionArmed[0] = true;
        playEffect(g_app.ballHitSound, 0.75f);
        const bool delayedRebound = effect.active && effect.type == 9 &&
            effect.owner != 1;
        if (delayedRebound) {
            effect.reboundPending = true;
        } else {
            clearBallEffect(effect);
            if (applyPlayerBallEffect(1, effect, x, y, velocityX, velocityY)) return;
        }
        const float base = static_cast<float>(effect.returnBase);
        if (velocityX > 0.0f) {
            if (g_app.player2Y + 20.0f > y + ballHeight) {
                velocityX = -(base - 1.0f);
                velocityY = -(base + 1.0f);
            } else if (g_app.player2Y + kFighterCollisionHeight - 20.0f < y) {
                velocityX = -(base - 1.0f);
                velocityY = base + 1.0f;
            } else {
                velocityX = -base;
                velocityY = velocityY > 0.0f ? base : -base;
            }
        } else {
            velocityX = -(base + 1.0f);
            const float reduced = base - 1.0f;
            velocityY = velocityY > 0.0f ? -reduced : reduced;
        }
    }
}

void updateMatch() {
    if (g_app.screen != Screen::match) return;
    ++g_app.frameCounter;
    for (int player = 0; player < 2; ++player) {
        if (g_app.attackCooldown[player] > 0) --g_app.attackCooldown[player];
        if (g_app.frozenTicks[player] > 0) --g_app.frozenTicks[player];
        if (g_app.comboTimeout[player] > 0 && --g_app.comboTimeout[player] == 0) {
            g_app.comboHistorySize[player] = 0;
        }
        if (g_app.animationTicks[player] > 0) {
            --g_app.animationTicks[player];
            g_app.animationFrame[player] = (48 - g_app.animationTicks[player]) / 2;
        }
    }

    if (g_app.matchPhase != MatchPhase::playing) {
        ++g_app.matchPhaseTicks;
        if (g_app.matchPhase == MatchPhase::betweenRounds) {
            // The original waits just over 200 engine updates after the KO
            // before rebuilding both fighters for the next round.
            if (g_app.matchPhaseTicks > 200) {
                g_app.roundNumber = std::min(3, g_app.score[0] + g_app.score[1] + 1);
                beginRound();
            }
        } else if (g_app.matchPhase == MatchPhase::finishPrompt) {
            // The stock game keeps the winner's component recognizer live
            // during the prompt. A completed ordinary component recipe is a
            // manual finisher; the shared realm-transport recipe remains
            // subject to its separate one-player/arena/full-version guards.
            if (g_app.roundWinner >= 0) {
                for (int input = 0; input < 5; ++input) {
                    const auto button = static_cast<CombatButton>(input);
                    if (combatButtonPressed(g_app.roundWinner, button)) {
                        processCombatButton(g_app.roundWinner, button);
                    }
                }
            }
            if (g_app.matchPhase == MatchPhase::finishPrompt &&
                g_app.matchPhaseTicks >= 140) {
                // 0x004144BF uses a 50% fallback and, when selected, activates
                // component 1 or 2 for the winner before result processing.
                if ((GetTickCount64() & 1) != 0 && g_app.roundWinner >= 0) {
                    const int component = 1 + static_cast<int>((GetTickCount64() >> 1) & 1);
                    launchComponent(g_app.roundWinner, component);
                    g_app.fatalityPerformed = true;
                    playEffect(g_app.fatalitySound, 1.0f);
                }
                g_app.matchPhase = MatchPhase::matchResult;
                g_app.matchPhaseTicks = 0;
            }
        }
        invalidate();
        return;
    }

    // The original round announcer advances once every five engine updates:
    // ROUND (18 stages), the round digit (18), then FIGHT (15 frames with a
    // seven-stage hold in its middle), and releases play at stage 59.
    if (g_app.roundIntroActive) {
        if (g_app.roundIntroDelay > 0) {
            --g_app.roundIntroDelay;
        } else {
            g_app.roundIntroDelay = 4;
            ++g_app.roundIntroStage;
            if (g_app.roundIntroStage == 1) {
                playEffect(g_app.roundSound, 1.0f);
            } else if (g_app.roundIntroStage == 19) {
                const int round = std::clamp(g_app.roundNumber, 1, 3) - 1;
                playEffect(g_app.roundNumberSounds[static_cast<std::size_t>(round)], 1.0f);
            } else if (g_app.roundIntroStage == 37) {
                playEffect(g_app.fightSound, 1.0f);
            }
            if (g_app.roundIntroStage >= 59) g_app.roundIntroActive = false;
        }
        invalidate();
        return;
    }

    for (int player = 0; player < 2; ++player) {
        if (g_app.cheats[2]) {
            g_app.super[static_cast<std::size_t>(player)] = kMaximumSuper;
        }
        if (player == 1 && g_app.playerCount == 1 &&
            g_app.super[1] >= kMaximumSuper && !g_app.superActive[1]) {
            // The stock CPU callbacks arm +0x754 as soon as their Super gauge
            // reaches the same 0x9A threshold used by human input.
            g_app.superActive[1] = true;
            g_app.superDrainTicks[1] = 0;
        }
        if (!g_app.superActive[static_cast<std::size_t>(player)]) {
            g_app.superDrainTicks[static_cast<std::size_t>(player)] = 0;
            continue;
        }
        if (g_app.cheats[2]) continue;
        if (++g_app.superDrainTicks[static_cast<std::size_t>(player)] >= 5) {
            g_app.superDrainTicks[static_cast<std::size_t>(player)] = 0;
            auto& gauge = g_app.super[static_cast<std::size_t>(player)];
            gauge = std::max(0, gauge - 1);
            if (gauge == 0) {
                g_app.superActive[static_cast<std::size_t>(player)] = false;
            }
        }
    }

    // The shared movement routine at 0x0040B431 uses eight pixels per update
    // in all four directions. Holding Turbo while moving vertically consumes
    // two gauge units and adds a five-pixel boost; an unheld Turbo key restores
    // one unit per update. Horizontal movement is deliberately not boosted.
    constexpr float paddleSpeed = 8.0f;
    constexpr float turboVerticalBoost = 5.0f;
    float player1MoveX = 0.0f;
    float player1MoveY = 0.0f;
    if (g_app.player1Pad >= 0) {
        player1MoveX = gamepadHorizontal(g_app.player1Pad);
        player1MoveY = gamepadVertical(g_app.player1Pad);
    } else {
        if (GetAsyncKeyState('A') & 0x8000) player1MoveX -= 1.0f;
        if (GetAsyncKeyState('D') & 0x8000) player1MoveX += 1.0f;
        if (GetAsyncKeyState('W') & 0x8000) player1MoveY -= 1.0f;
        if (GetAsyncKeyState('S') & 0x8000) player1MoveY += 1.0f;
    }
    if (g_app.currentKode == VersusKode::reverseControls) {
        player1MoveX = -player1MoveX;
        player1MoveY = -player1MoveY;
    }
    const bool player1Turbo = g_app.player1Pad >= 0
        ? (g_app.gamepads[static_cast<std::size_t>(g_app.player1Pad)].Gamepad.wButtons &
           XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0
        : (GetAsyncKeyState('5') & 0x8000) != 0;
    float player1VerticalSpeed = paddleSpeed;
    if (player1Turbo && player1MoveY != 0.0f && g_app.turbo[0] >= 2 &&
        g_app.currentKode != VersusKode::runDisabled) {
        g_app.turbo[0] -= 2;
        player1VerticalSpeed += turboVerticalBoost;
    } else if (!player1Turbo) {
        g_app.turbo[0] = std::min(kMaximumTurbo, g_app.turbo[0] + 1);
    }
    if (g_app.frozenTicks[0] == 0) {
        g_app.player1X += player1MoveX * paddleSpeed;
        g_app.player1Y += player1MoveY * player1VerticalSpeed;
    }
    if (g_app.playerCount == 2) {
        float player2MoveX = 0.0f;
        float player2MoveY = 0.0f;
        if (g_app.player2Pad >= 0) {
            player2MoveX = gamepadHorizontal(g_app.player2Pad);
            player2MoveY = gamepadVertical(g_app.player2Pad);
        } else {
            if (GetAsyncKeyState(VK_LEFT) & 0x8000) player2MoveX -= 1.0f;
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) player2MoveX += 1.0f;
            if (GetAsyncKeyState(VK_UP) & 0x8000) player2MoveY -= 1.0f;
            if (GetAsyncKeyState(VK_DOWN) & 0x8000) player2MoveY += 1.0f;
        }
        if (g_app.currentKode == VersusKode::reverseControls) {
            player2MoveX = -player2MoveX;
            player2MoveY = -player2MoveY;
        }
        const bool player2Turbo = g_app.player2Pad >= 0
            ? (g_app.gamepads[static_cast<std::size_t>(g_app.player2Pad)].Gamepad.wButtons &
               XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0
            : (GetAsyncKeyState('0') & 0x8000) != 0;
        float player2VerticalSpeed = paddleSpeed;
        if (player2Turbo && player2MoveY != 0.0f && g_app.turbo[1] >= 2 &&
            g_app.currentKode != VersusKode::runDisabled) {
            g_app.turbo[1] -= 2;
            player2VerticalSpeed += turboVerticalBoost;
        } else if (!player2Turbo) {
            g_app.turbo[1] = std::min(kMaximumTurbo, g_app.turbo[1] + 1);
        }
        if (g_app.frozenTicks[1] == 0) {
            g_app.player2X += player2MoveX * paddleSpeed;
            g_app.player2Y += player2MoveY * player2VerticalSpeed;
        }
    } else {
        // CPU paddles use the same eight-pixel step and half-court bounds. The
        // original character callbacks choose attacks independently; position
        // prediction here follows the live ball in both axes.
        const float targetX = std::clamp(g_app.ballX - 6.0f, 344.0f, 532.0f);
        const float targetY = g_app.ballY - 22.0f;
        if (g_app.frozenTicks[1] == 0) {
            g_app.player2X += std::clamp(targetX - g_app.player2X,
                                         -paddleSpeed, paddleSpeed);
            g_app.player2Y += std::clamp(targetY - g_app.player2Y,
                                         -paddleSpeed, paddleSpeed);
        }
        g_app.turbo[1] = std::min(kMaximumTurbo, g_app.turbo[1] + 1);
    }
    g_app.player1X = std::clamp(g_app.player1X, 0.0f, 188.0f);
    g_app.player2X = std::clamp(g_app.player2X, 344.0f, 532.0f);
    g_app.player1Y = std::clamp(g_app.player1Y, 0.0f, 378.0f);
    g_app.player2Y = std::clamp(g_app.player2Y, 0.0f, 378.0f);
    if (g_app.currentKode == VersusKode::randomPaddles) {
        const bool componentsIdle = std::none_of(
            g_app.projectiles.begin(), g_app.projectiles.end(),
            [](const Projectile& projectile) { return projectile.active; });
        for (int player = 0; player < 2; ++player) {
            if (++g_app.randomPaddleTicks[static_cast<std::size_t>(player)] > 400 &&
                componentsIdle) {
                g_app.randomPaddleTicks[static_cast<std::size_t>(player)] = 0;
                g_app.paddleAppearance[static_cast<std::size_t>(player)] =
                    static_cast<int>(GetTickCount() & 0x0f);
            }
        }
    }
    for (int attack = 0; attack < 3; ++attack) {
        const auto button = static_cast<CombatButton>(attack);
        if (combatButtonPressed(0, button)) {
            triggerPaddleAttack(0, attack);
            processCombatButton(0, button);
        }
        if (g_app.playerCount == 2) {
            if (combatButtonPressed(1, button)) {
                triggerPaddleAttack(1, attack);
                processCombatButton(1, button);
            }
        }
    }
    for (CombatButton button : {CombatButton::turbo, CombatButton::super}) {
        if (combatButtonPressed(0, button)) processCombatButton(0, button);
        if (g_app.playerCount == 2 && combatButtonPressed(1, button)) {
            processCombatButton(1, button);
        }
    }
    if (g_app.playerCount == 1 && g_app.attackCooldown[1] == 0 &&
        (g_app.frameCounter % 180) == 0) {
        const int character = g_app.selectedCharacters[1];
        const auto& recipes = kComboRecipes[static_cast<std::size_t>(character)];
        for (const auto& recipe : recipes) {
            if (recipe.component > 0) {
                launchComponent(1, recipe.component);
                break;
            }
        }
    }
    updateProjectiles();
    if (g_app.matchPhase != MatchPhase::playing) {
        invalidate();
        return;
    }
    if (g_app.cheats[3]) {
        invalidate();
        return;
    }
    if (g_app.currentKode != VersusKode::ballDisabled) {
        if (g_app.currentKode == VersusKode::crazyBall && ++g_app.ballCrazyTicks >= 6) {
            g_app.ballCrazyTicks = 0;
            const int horizontalMagnitude = 2 + static_cast<int>(GetTickCount() & 7);
            g_app.ballVelocityX = g_app.ballVelocityX > 0.0f
                ? static_cast<float>(horizontalMagnitude)
                : -static_cast<float>(horizontalMagnitude);
            const bool verticalPositive = (GetTickCount() & 1) != 0;
            const int verticalMagnitude = 2 + static_cast<int>(GetTickCount() & 7);
            g_app.ballVelocityY = verticalPositive
                ? static_cast<float>(verticalMagnitude)
                : -static_cast<float>(verticalMagnitude);
        }
        updateBallObject(g_app.ballX, g_app.ballY,
                         g_app.ballVelocityX, g_app.ballVelocityY,
                         g_app.ballCollisionArmed, g_app.ballEffects[0], false);
        if (g_app.matchPhase != MatchPhase::playing) {
            invalidate();
            return;
        }
        if (g_app.secondaryBallActive) {
            updateBallObject(g_app.secondaryBallX, g_app.secondaryBallY,
                             g_app.secondaryBallVelocityX,
                             g_app.secondaryBallVelocityY,
                             g_app.secondaryBallCollisionArmed,
                             g_app.ballEffects[1],
                             g_app.currentKode == VersusKode::decoyBall);
        }
    }
    invalidate();
}

bool ensurePresentationBuffer(HDC referenceDc, int width, int height) {
    if (g_app.presentationDc && g_app.presentationBitmap &&
        g_app.presentationWidth == width && g_app.presentationHeight == height) return true;

    if (g_app.presentationDc && g_app.oldPresentationBitmap) {
        SelectObject(g_app.presentationDc, g_app.oldPresentationBitmap);
    }
    if (g_app.presentationBitmap) DeleteObject(g_app.presentationBitmap);
    if (g_app.presentationDc) DeleteDC(g_app.presentationDc);
    g_app.presentationDc = nullptr;
    g_app.presentationBitmap = nullptr;
    g_app.oldPresentationBitmap = nullptr;
    g_app.presentationWidth = 0;
    g_app.presentationHeight = 0;

    if (width <= 0 || height <= 0) return false;
    g_app.presentationDc = CreateCompatibleDC(referenceDc);
    g_app.presentationBitmap = CreateCompatibleBitmap(referenceDc, width, height);
    if (!g_app.presentationDc || !g_app.presentationBitmap) {
        if (g_app.presentationBitmap) DeleteObject(g_app.presentationBitmap);
        if (g_app.presentationDc) DeleteDC(g_app.presentationDc);
        g_app.presentationDc = nullptr;
        g_app.presentationBitmap = nullptr;
        return false;
    }
    g_app.oldPresentationBitmap = static_cast<HBITMAP>(
        SelectObject(g_app.presentationDc, g_app.presentationBitmap));
    g_app.presentationWidth = width;
    g_app.presentationHeight = height;
    return true;
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_SYSKEYDOWN:
            if (wParam == VK_RETURN && (lParam & (1LL << 29))) {
                toggleFullscreen();
                return 0;
            }
            if (wParam == VK_F1 && (GetKeyState(VK_CONTROL) & 0x8000) &&
                (GetKeyState(VK_MENU) & 0x8000) && handleSecretShortcut()) return 0;
            break;
        case WM_KEYDOWN:
            if (wParam == VK_F1 && (GetKeyState(VK_CONTROL) & 0x8000) &&
                (GetKeyState(VK_MENU) & 0x8000)) {
                if (handleSecretShortcut()) return 0;
            }
            if (g_app.screen == Screen::title) {
                if (wParam == VK_UP) {
                    g_app.titleSelection = (g_app.titleSelection + 4) % 5;
                    playEffect(g_app.menuMoveSound, 0.7f);
                    invalidate();
                    return 0;
                }
                if (wParam == VK_DOWN) {
                    g_app.titleSelection = (g_app.titleSelection + 1) % 5;
                    playEffect(g_app.menuMoveSound, 0.7f);
                    invalidate();
                    return 0;
                }
                if (wParam == VK_RETURN || wParam == VK_SPACE) {
                    activateTitleSelection();
                    return 0;
                }
            } else if (g_app.screen == Screen::characterSelect) {
                const int available = g_app.allContentUnlocked ? 16 : 4;
                int& character = g_app.selectedCharacters[g_app.selectingPlayer];
                if (wParam == VK_LEFT) {
                    character = (character + available - 1) % available;
                    playEffect(g_app.menuMoveSound, 0.7f);
                    invalidate();
                    return 0;
                }
                if (wParam == VK_RIGHT) {
                    character = (character + 1) % available;
                    playEffect(g_app.menuMoveSound, 0.7f);
                    invalidate();
                    return 0;
                }
                if (wParam == VK_RETURN || wParam == VK_SPACE) {
                    playEffect(g_app.fighterVoices[static_cast<std::size_t>(character)], 1.0f);
                    if (g_app.playerCount == 2 && g_app.selectingPlayer == 0) {
                        g_app.selectingPlayer = 1;
                        g_app.selectedCharacters[1] %= available;
                        invalidate();
                    } else if (g_app.playerCount == 2) {
                        g_app.kodeDigits = {};
                        g_app.screen = Screen::versusKode;
                        invalidate();
                    } else {
                        showCpuLadder();
                    }
                    return 0;
                }
                if (wParam == VK_ESCAPE) {
                    g_app.screen = Screen::title;
                    invalidate();
                    return 0;
                }
            } else if (g_app.screen == Screen::versusKode) {
                if (wParam >= '0' && wParam <= '9') {
                    for (int index = 0; index < 5; ++index) g_app.kodeDigits[index] = g_app.kodeDigits[index + 1];
                    g_app.kodeDigits[5] = static_cast<int>(wParam - '0');
                    playEffect(g_app.menuMoveSound, 0.65f);
                    invalidate();
                    return 0;
                }
                if (wParam == VK_RETURN || wParam == VK_SPACE) {
                    playEffect(g_app.menuSelectSound);
                    beginMatch();
                    return 0;
                }
                if (wParam == VK_ESCAPE) {
                    g_app.screen = Screen::characterSelect;
                    g_app.selectingPlayer = g_app.playerCount == 2 ? 1 : 0;
                    playTitleMusic();
                    invalidate();
                    return 0;
                }
            } else if (g_app.screen == Screen::match) {
                if (!(lParam & (1LL << 30)) && handleKeyboardCombatKey(wParam)) return 0;
                if ((wParam == VK_RETURN || wParam == VK_SPACE) &&
                    g_app.matchPhase == MatchPhase::matchResult) {
                    advanceAfterMatch();
                    return 0;
                }
                if (wParam == VK_ESCAPE) {
                    g_app.screen = Screen::title;
                    playTitleMusic();
                    invalidate();
                    return 0;
                }
            } else if (g_app.screen == Screen::cheatMenu) {
                if (wParam == VK_UP) {
                    g_app.cheatSelection = (g_app.cheatSelection + 5) % 6;
                    playEffect(g_app.menuMoveSound, 0.7f);
                    invalidate();
                    return 0;
                }
                if (wParam == VK_DOWN) {
                    g_app.cheatSelection = (g_app.cheatSelection + 1) % 6;
                    playEffect(g_app.menuMoveSound, 0.7f);
                    invalidate();
                    return 0;
                }
                if (wParam == VK_RETURN || wParam == VK_SPACE) {
                    g_app.cheats[g_app.cheatSelection] = !g_app.cheats[g_app.cheatSelection];
                    if (g_app.cheatSelection == 5 && g_app.cheats[5]) g_app.allContentUnlocked = true;
                    playEffect(g_app.menuSelectSound);
                    invalidate();
                    return 0;
                }
                if (wParam == VK_ESCAPE) {
                    g_app.screen = g_app.screenBelowCheats;
                    invalidate();
                    return 0;
                }
            } else if (g_app.screen == Screen::configuration) {
                if (wParam == VK_UP) {
                    g_app.configSelection = (g_app.configSelection + 5) % 6;
                    playEffect(g_app.menuMoveSound, 0.7f);
                    invalidate();
                    return 0;
                }
                if (wParam == VK_DOWN) {
                    g_app.configSelection = (g_app.configSelection + 1) % 6;
                    playEffect(g_app.menuMoveSound, 0.7f);
                    invalidate();
                    return 0;
                }
                if (wParam == VK_LEFT) {
                    playEffect(g_app.menuSelectSound, 0.7f);
                    adjustConfiguration(-1);
                    return 0;
                }
                if (wParam == VK_RIGHT) {
                    playEffect(g_app.menuSelectSound, 0.7f);
                    adjustConfiguration(1);
                    return 0;
                }
                if (wParam == VK_RETURN || wParam == VK_SPACE) {
                    playEffect(g_app.menuSelectSound);
                    if (g_app.configSelection == 5) {
                        g_app.screen = Screen::title;
                        invalidate();
                    } else {
                        adjustConfiguration(1);
                    }
                    return 0;
                }
                if (wParam == VK_ESCAPE) {
                    g_app.screen = Screen::title;
                    invalidate();
                    return 0;
                }
            } else if (g_app.screen == Screen::credits &&
                       (wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == VK_SPACE)) {
                g_app.screen = Screen::title;
                invalidate();
                return 0;
            } else if (g_app.screen == Screen::continuePrompt) {
                if (wParam == VK_RETURN || wParam == VK_SPACE ||
                    wParam == '1' || wParam == '2' || wParam == '3' ||
                    wParam == '4' || wParam == '5') {
                    acceptContinue();
                    return 0;
                }
                if (wParam == VK_ESCAPE) {
                    showGameOver();
                    return 0;
                }
            } else if (g_app.screen == Screen::gameOver &&
                       (wParam == VK_RETURN || wParam == VK_SPACE || wParam == VK_ESCAPE)) {
                g_app.screen = Screen::title;
                playTitleMusic();
                invalidate();
                return 0;
            }
            if (wParam == VK_ESCAPE && g_app.screen == Screen::title) {
                PostMessageW(window, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        case WM_TIMER:
            pollGamepads();
            postGamepadMenuInput();
            if (g_app.screen == Screen::continuePrompt) {
                if (++g_app.continueCountdownTicks >= 40) {
                    g_app.continueCountdownTicks = 0;
                    if (--g_app.continueCountdown < 0) showGameOver();
                    else invalidate();
                }
            } else {
                updateMatch();
            }
            return 0;
        case MM_WOM_DONE:
            refillAudioBuffer(reinterpret_cast<WAVEHDR*>(lParam));
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);
            int width = client.right - client.left;
            int height = client.bottom - client.top;
            int scaledWidth = width;
            int scaledHeight = scaledWidth * kCanvasHeight / kCanvasWidth;
            if (scaledHeight > height) {
                scaledHeight = height;
                scaledWidth = scaledHeight * kCanvasWidth / kCanvasHeight;
            }
            const int x = (width - scaledWidth) / 2;
            const int y = (height - scaledHeight) / 2;
            if (ensurePresentationBuffer(dc, width, height)) {
                PatBlt(g_app.presentationDc, 0, 0, width, height, BLACKNESS);
                SetStretchBltMode(g_app.presentationDc, COLORONCOLOR);
                StretchBlt(g_app.presentationDc, x, y, scaledWidth, scaledHeight,
                           g_app.backDc, 0, 0, kCanvasWidth, kCanvasHeight, SRCCOPY);
                // This is the only copy to the live window. The black letterbox
                // clear and scaled game image are never presented separately.
                BitBlt(dc, 0, 0, width, height, g_app.presentationDc, 0, 0, SRCCOPY);
            } else {
                SetStretchBltMode(dc, COLORONCOLOR);
                StretchBlt(dc, x, y, scaledWidth, scaledHeight, g_app.backDc, 0, 0,
                           kCanvasWidth, kCanvasHeight, SRCCOPY);
            }
            EndPaint(window, &paint);
            return 0;
        }
        case WM_SIZE:
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

bool initializeBackbuffer(HWND window) {
    HDC windowDc = GetDC(window);
    g_app.backDc = CreateCompatibleDC(windowDc);
    ReleaseDC(window, windowDc);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = kCanvasWidth;
    info.bmiHeader.biHeight = -kCanvasHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    g_app.backBitmap = CreateDIBSection(g_app.backDc, &info, DIB_RGB_COLORS,
                                        &g_app.backPixels, nullptr, 0);
    if (!g_app.backBitmap) return false;
    g_app.oldBackBitmap = static_cast<HBITMAP>(SelectObject(g_app.backDc, g_app.backBitmap));
    return true;
}

void destroyResources() {
    shutdownAudio();
    if (g_app.xinputSetState) {
        XINPUT_VIBRATION vibration{};
        for (DWORD pad = 0; pad < XUSER_MAX_COUNT; ++pad) {
            g_app.xinputSetState(pad, &vibration);
        }
    }
    for (auto& item : g_app.selectedItems) if (item.handle) DeleteObject(item.handle);
    for (auto& item : g_app.normalItems) if (item.handle) DeleteObject(item.handle);
    if (g_app.title.handle) DeleteObject(g_app.title.handle);
    if (g_app.creditsBackdrop.handle) DeleteObject(g_app.creditsBackdrop.handle);
    if (g_app.selectBackdrop.handle) DeleteObject(g_app.selectBackdrop.handle);
    if (g_app.versusBackdrop.handle) DeleteObject(g_app.versusBackdrop.handle);
    if (g_app.mysteryPortrait.handle) DeleteObject(g_app.mysteryPortrait.handle);
    if (g_app.continuePanel.handle) DeleteObject(g_app.continuePanel.handle);
    for (auto& digit : g_app.continueDigits) if (digit.handle) DeleteObject(digit.handle);
    for (auto& backdrop : g_app.matchBackdrops) if (backdrop.handle) DeleteObject(backdrop.handle);
    if (g_app.configBanner.handle) DeleteObject(g_app.configBanner.handle);
    for (auto& portrait : g_app.portraits) if (portrait.handle) DeleteObject(portrait.handle);
    if (g_app.presentationDc && g_app.oldPresentationBitmap) {
        SelectObject(g_app.presentationDc, g_app.oldPresentationBitmap);
    }
    if (g_app.presentationBitmap) DeleteObject(g_app.presentationBitmap);
    if (g_app.presentationDc) DeleteDC(g_app.presentationDc);
    if (g_app.backDc && g_app.oldBackBitmap) SelectObject(g_app.backDc, g_app.oldBackBitmap);
    if (g_app.backBitmap) DeleteObject(g_app.backBitmap);
    if (g_app.backDc) DeleteDC(g_app.backDc);
    if (g_app.xinputModule) FreeLibrary(g_app.xinputModule);
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand) {
    SetProcessDPIAware();
    constexpr wchar_t kClassName[] = L"BloodPongNativeWindow";
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kClassName;
    if (!RegisterClassExW(&windowClass)) return 1;

    RECT desired{0, 0, kCanvasWidth, kCanvasHeight};
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);
    g_app.window = CreateWindowExW(
        0, kClassName, L"Blood Pong — Native Preservation Port", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, desired.right - desired.left, desired.bottom - desired.top,
        nullptr, nullptr, instance, nullptr);
    if (!g_app.window || !initializeBackbuffer(g_app.window)) return 2;

    g_app.title = loadBitmap(instance, IDB_TITLE);
    g_app.creditsBackdrop = loadBitmap(instance, IDB_CREDITS_BACKDROP);
    g_app.selectedItems = {loadBitmap(instance, IDB_MENU_ONE_SELECTED),
                           loadBitmap(instance, IDB_MENU_TWO_SELECTED),
                           loadBitmap(instance, IDB_MENU_CONFIG_SELECTED),
                           loadBitmap(instance, IDB_MENU_QUIT_SELECTED)};
    g_app.normalItems = {loadBitmap(instance, IDB_MENU_ONE), loadBitmap(instance, IDB_MENU_TWO),
                         loadBitmap(instance, IDB_MENU_CONFIG), loadBitmap(instance, IDB_MENU_QUIT)};
    g_app.matchBackdrops[0] = loadOriginalBitmap(instance, 1001, 128, true);
    g_app.matchBackdrops[1] = loadOriginalBitmap(instance, 1001, 129);
    g_app.matchBackdrops[2] = loadOriginalBitmap(instance, 1001, 130);
    g_app.selectBackdrop = loadOriginalBitmap(instance, 2007, 302);
    g_app.versusBackdrop = loadOriginalBitmap(instance, 2007, 304);
    g_app.mysteryPortrait = loadOriginalBitmap(instance, 2007, 2000);
    g_app.continuePanel = loadOriginalBitmap(instance, 2007, 300);
    for (std::size_t digit = 0; digit < g_app.continueDigits.size(); ++digit) {
        g_app.continueDigits[digit] =
            loadOriginalBitmap(instance, 2007, 400 + static_cast<int>(digit));
    }
    g_app.configBanner = loadOriginalBitmap(instance, 2007, 303);
    for (int index = 0; index < 16; ++index) {
        g_app.portraits[index] = loadOriginalBitmap(instance, 2007, 1000 + index);
        g_app.standingPaddles[index] =
            loadOriginalSprite(instance, kCharacterResourceTypes[index], 1000);
        g_app.fighterNameSprites[index] = loadOriginalSprite(instance, 2023, 600 + index);
        for (int frame = 0; frame < 24; ++frame) {
            g_app.paddleFrames[index][frame] =
                loadOriginalSprite(instance, kCharacterResourceTypes[index], 3000 + frame);
        }
    }
    for (const auto& visual : kProjectileVisuals) {
        if (visual.originalType <= 0 || visual.frameCount <= 0) continue;
        auto& frames = g_app.projectileTypeFrames[static_cast<std::size_t>(
            visual.originalType)];
        for (int frame = 0; frame < visual.frameCount; ++frame) {
            frames.push_back(loadOriginalSprite(instance, visual.resourceType,
                                                visual.firstResourceId + frame));
        }
    }
    g_app.portraits[16] = loadOriginalBitmap(instance, 2007, 2000);
    for (int index = 0; index < static_cast<int>(g_app.ballSprites.size()); ++index) {
        g_app.ballSprites[static_cast<std::size_t>(index)] =
            loadOriginalSprite(instance, 2004, 500 + index);
    }
    g_app.roundWinMarker = loadOriginalSprite(instance, 2004, 128);
    g_app.healthFrame = loadOriginalSprite(instance, 2004, 400);
    g_app.turboFrame = loadOriginalSprite(instance, 2004, 401);
    g_app.superFrames = {loadOriginalSprite(instance, 2004, 402),
                         loadOriginalSprite(instance, 2004, 403)};
    for (int frame = 0; frame < 18; ++frame) {
        g_app.roundFrames[static_cast<std::size_t>(frame)] =
            loadOriginalSprite(instance, 2023, 1000 + frame);
        for (int round = 0; round < 3; ++round) {
            g_app.roundNumberFrames[static_cast<std::size_t>(round)]
                                         [static_cast<std::size_t>(frame)] =
                loadOriginalSprite(instance, 2023, 2000 + round * 1000 + frame);
        }
    }
    for (int frame = 0; frame < 15; ++frame) {
        g_app.fightFrames[static_cast<std::size_t>(frame)] =
            loadOriginalSprite(instance, 2023, 300 + frame);
    }
    for (int frame = 0; frame < 14; ++frame) {
        g_app.finishHimFrames[static_cast<std::size_t>(frame)] =
            loadOriginalSprite(instance, 2023, 400 + frame);
        g_app.finishHerFrames[static_cast<std::size_t>(frame)] =
            loadOriginalSprite(instance, 2023, 450 + frame);
    }
    g_app.fatalitySprite = loadOriginalSprite(instance, 2023, 250);
    g_app.titleMusic = loadOriginalVoc(instance, 8000);
    g_app.matchMusic = loadOriginalVoc(instance, 8005);
    g_app.ladderSound = loadOriginalVoc(instance, 8004);
    g_app.gameOverSounds = {loadOriginalVoc(instance, 5002),
                            loadOriginalVoc(instance, 5003)};
    for (std::size_t index = 0; index < g_app.fighterVoices.size(); ++index) {
        g_app.fighterVoices[index] = loadOriginalVoc(instance, kFighterVoiceIds[index]);
    }
    g_app.menuMoveSound = loadOriginalVoc(instance, 3001);
    g_app.menuSelectSound = loadOriginalVoc(instance, 3002);
    g_app.roundSound = loadOriginalVoc(instance, 1000);
    for (std::size_t round = 0; round < g_app.roundNumberSounds.size(); ++round) {
        g_app.roundNumberSounds[round] = loadOriginalVoc(instance, 1001 + static_cast<int>(round));
    }
    g_app.fightSound = loadOriginalVoc(instance, 1004);
    g_app.finishHimSound = loadOriginalVoc(instance, 1005);
    g_app.finishHerSound = loadOriginalVoc(instance, 1006);
    g_app.fatalitySound = loadOriginalVoc(instance, 250);
    g_app.realmTransportSound = loadOriginalVoc(instance, 5001);
    g_app.projectileAlternateSound = loadOriginalVoc(instance, 3028);
    g_app.projectileSecondarySound = loadOriginalVoc(instance, 3011);
    g_app.ballBounceSound = loadOriginalVoc(instance, 500);
    g_app.ballHitSound = loadOriginalVoc(instance, 501);
    for (std::size_t type = 1; type < g_app.projectileSounds.size(); ++type) {
        g_app.projectileSounds[type] = loadOriginalVoc(instance, kProjectileSoundIds[type]);
    }
    constexpr std::array<const wchar_t*, 3> xinputLibraries{
        L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};
    for (const wchar_t* library : xinputLibraries) {
        g_app.xinputModule = LoadLibraryW(library);
        if (g_app.xinputModule) {
            g_app.xinputGetState = reinterpret_cast<XInputGetStateFunction>(
                GetProcAddress(g_app.xinputModule, "XInputGetState"));
            g_app.xinputSetState = reinterpret_cast<XInputSetStateFunction>(
                GetProcAddress(g_app.xinputModule, "XInputSetState"));
            if (g_app.xinputGetState) break;
            FreeLibrary(g_app.xinputModule);
            g_app.xinputModule = nullptr;
        }
    }
    pollGamepads();
    initializeAudio();
    render();
    ShowWindow(g_app.window, showCommand);
    UpdateWindow(g_app.window);
    // A 15 ms USER timer stays on the ~15.6 ms system tick; 16 ms can round up
    // to two ticks on affected Windows configurations and halve gameplay speed.
    timeBeginPeriod(1);
    SetTimer(g_app.window, 1, 15, nullptr);
    playTitleMusic();

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    KillTimer(g_app.window, 1);
    destroyResources();
    timeEndPeriod(1);
    return static_cast<int>(message.wParam);
}
