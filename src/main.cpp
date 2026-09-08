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
constexpr COLORREF kTransparent = RGB(0, 0, 0);

enum class Screen { title, credits, characterSelect, versusKode, match, cheatMenu, configuration };

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
    std::vector<std::uint8_t> wave;
    explicit operator bool() const { return !wave.empty(); }
};

struct Projectile {
    bool active{};
    int owner{};
    float x{};
    float y{};
    float velocityX{};
    float velocityY{};
    int frame{};
    int damage{};
    int originalType{};
};

struct AttackDefinition {
    int originalType{};
    int resourceType{};
    int firstResourceId{};
    int frameCount{};
    int damage{};
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
    std::array<BitmapAsset, 3> matchBackdrops{};
    int matchStage{};
    BitmapAsset configBanner{};
    std::array<BitmapAsset, 17> portraits{};
    std::array<SpriteAsset, 16> standingPaddles{};
    std::array<std::array<SpriteAsset, 24>, 16> paddleFrames{};
    std::array<std::vector<SpriteAsset>, 16> projectileFrames{};
    std::array<SpriteAsset, 16> fighterNameSprites{};
    SpriteAsset ballSprite{};
    SpriteAsset roundWinMarker{};
    SpriteAsset healthFrame{};
    SpriteAsset turboFrame{};
    std::array<SpriteAsset, 2> superFrames{};
    std::array<SpriteAsset, 18> roundFrames{};
    std::array<std::array<SpriteAsset, 18>, 3> roundNumberFrames{};
    std::array<SpriteAsset, 15> fightFrames{};
    SoundAsset titleMusic{};
    SoundAsset matchMusic{};
    std::array<std::uint32_t, 256> palette{};
    Screen screen{Screen::title};
    Screen screenBelowCheats{Screen::match};
    int titleSelection{};
    int playerCount{1};
    int selectingPlayer{};
    std::array<int, 2> selectedCharacters{0, 1};
    std::array<int, 6> kodeDigits{};
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
    float player1Y{189.0f};
    float player2Y{189.0f};
    float ballX{264.0f};
    float ballY{208.0f};
    float ballVelocityX{3.25f};
    float ballVelocityY{2.15f};
    std::array<int, 2> score{};
    std::array<int, 2> turbo{kMaximumTurbo, kMaximumTurbo};
    int roundNumber{1};
    int roundIntroStage{};
    int roundIntroDelay{};
    bool roundIntroActive{};
    std::array<int, 2> health{kMaximumHealth, kMaximumHealth};
    std::array<int, 2> super{};
    std::array<int, 2> animationFrame{};
    std::array<int, 2> animationTicks{};
    std::array<int, 2> attackCooldown{};
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

// The first literal projectile initialized by each fighter constructor. The type
// switch at 0x0041ACB5 maps those types to these exact embedded art banks; type 9
// deliberately reuses the original ball sprite rather than a type-2022 effect.
constexpr std::array<AttackDefinition, 16> kCharacterPrimaryAttacks{{
    {1, 2022, 300, 2, 10},       // Fung Shwei
    {2, 2022, 700, 6, 30},       // Lo Than
    {3, 2022, 1300, 6, 30},      // Jewel
    {5, 2022, 180, 4, 30},       // Raptor
    {6, 2022, 128, 6, 0},        // So Frio (status effect; no direct damage)
    {9, 2004, 500, 1, 30},       // Nai Palm (extra ball)
    {10, 2022, 1500, 4, 20},     // One Eye
    {12, 2022, 1200, 7, 30},     // Raider
    {13, 2022, 200, 6, 30},      // Show Lin
    {24, 2022, 500, 3, 15},      // Dawg Cau
    {17, 2022, 170, 1, 30},      // Omoh
    {18, 2022, 400, 3, 30},      // Carmack
    {19, 2022, 1550, 5, 20},     // Pain
    {21, 2022, 350, 5, 30},      // Lo Pan
    {15, 2022, 600, 3, 30},      // Mai Lai
    {22, 2022, 260, 4, 20},      // Baka
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

void appendU16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
}

void appendU32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        output.push_back(static_cast<std::uint8_t>(value >> shift));
    }
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

    sound.wave.reserve(44 + pcm.size());
    sound.wave.insert(sound.wave.end(), {'R', 'I', 'F', 'F'});
    appendU32(sound.wave, static_cast<std::uint32_t>(36 + pcm.size()));
    sound.wave.insert(sound.wave.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    appendU32(sound.wave, 16);
    appendU16(sound.wave, WAVE_FORMAT_PCM);
    appendU16(sound.wave, 1);
    appendU32(sound.wave, sampleRate);
    appendU32(sound.wave, sampleRate);
    appendU16(sound.wave, 1);
    appendU16(sound.wave, 8);
    sound.wave.insert(sound.wave.end(), {'d', 'a', 't', 'a'});
    appendU32(sound.wave, static_cast<std::uint32_t>(pcm.size()));
    sound.wave.insert(sound.wave.end(), pcm.begin(), pcm.end());
    return sound;
}

void playMusic(const SoundAsset& sound) {
    if (!g_app.soundEnabled || !sound) {
        PlaySoundW(nullptr, nullptr, 0);
        return;
    }
    PlaySoundA(reinterpret_cast<LPCSTR>(sound.wave.data()), nullptr,
               SND_ASYNC | SND_MEMORY | SND_LOOP | SND_NODEFAULT);
}

void playTitleMusic() { playMusic(g_app.titleMusic); }

void playMatchMusic() { playMusic(g_app.matchMusic); }

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

void renderVersusKode() {
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
    if (g_app.allContentUnlocked) {
        drawTextLine("ULTIMATE KODE ACCEPTED", 400, 15, RGB(55, 235, 95));
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
    const int character = g_app.selectedCharacters[player];
    if (g_app.animationTicks[player] > 0) {
        const auto& frame = g_app.paddleFrames[character][g_app.animationFrame[player] % 24];
        if (frame) return frame;
    }
    return g_app.standingPaddles[character];
}

const SpriteAsset& activeProjectileSprite(const Projectile& projectile) {
    static const SpriteAsset empty{};
    const int character = g_app.selectedCharacters[static_cast<std::size_t>(projectile.owner)];
    const auto& frames = g_app.projectileFrames[character];
    if (frames.empty()) return empty;
    const std::size_t index = std::min<std::size_t>(
        static_cast<std::size_t>(projectile.frame / 3), frames.size() - 1);
    return frames[index];
}

void renderMatch() {
    drawBitmap(g_app.matchBackdrops[static_cast<std::size_t>(g_app.matchStage)], kArtX, kArtY);
    const auto& leftPaddle = activePaddleSprite(0);
    const auto& rightPaddle = activePaddleSprite(1);
    drawSprite(leftPaddle, kArtX + 24, kArtY + static_cast<int>(g_app.player1Y));
    drawSprite(rightPaddle, kArtX + 520 - rightPaddle.width,
               kArtY + static_cast<int>(g_app.player2Y), true);
    for (const auto& projectile : g_app.projectiles) {
        if (!projectile.active) continue;
        const auto& sprite = activeProjectileSprite(projectile);
        drawSprite(sprite, kArtX + static_cast<int>(projectile.x),
                   kArtY + static_cast<int>(projectile.y), projectile.owner == 1);
    }
    drawSprite(g_app.ballSprite, kArtX + static_cast<int>(g_app.ballX),
               kArtY + static_cast<int>(g_app.ballY));

    // The original HUD resources are 400/401/402/403 in type 2004.  Their
    // frame geometry and fixed 544x432 coordinates are preserved here; only
    // the mutable interiors are painted before the labels are overlaid.
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

void activateTitleSelection() {
    switch (g_app.titleSelection) {
        case 0:
            g_app.playerCount = 1;
            g_app.selectingPlayer = 0;
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

void resetBall(int direction) {
    g_app.ballX = 264.0f;
    g_app.ballY = 208.0f;
    g_app.ballVelocityX = 3.25f * static_cast<float>(direction);
    g_app.ballVelocityY = 2.15f;
}

void beginMatch() {
    g_app.player1Y = 189.0f;
    g_app.player2Y = 189.0f;
    g_app.score = {};
    g_app.health = {kMaximumHealth, kMaximumHealth};
    g_app.turbo = {kMaximumTurbo, kMaximumTurbo};
    g_app.super = {};
    g_app.roundNumber = 1;
    g_app.roundIntroStage = 0;
    g_app.roundIntroDelay = 0;
    g_app.roundIntroActive = true;
    g_app.animationFrame = {};
    g_app.animationTicks = {};
    g_app.attackCooldown = {};
    g_app.projectiles = {};
    g_app.matchStage = static_cast<int>((GetTickCount64() / 17 +
        g_app.selectedCharacters[0] * 3 + g_app.selectedCharacters[1]) % 3);
    resetBall(1);
    g_app.screen = Screen::match;
    playMatchMusic();
    invalidate();
}

bool handleSecretShortcut() {
    if (g_app.screen == Screen::versusKode) {
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
    if (g_app.screen == Screen::match) return;
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
    return -std::clamp(normalized, -1.0f, 1.0f);
}

void damagePlayer(int player, int amount) {
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
        ++g_app.score[1 - player];
        g_app.health = {kMaximumHealth, kMaximumHealth};
        g_app.turbo = {kMaximumTurbo, kMaximumTurbo};
        g_app.super = {};
        g_app.projectiles = {};
        g_app.roundNumber = std::min(3, g_app.score[0] + g_app.score[1] + 1);
        g_app.roundIntroStage = 0;
        g_app.roundIntroDelay = 0;
        g_app.roundIntroActive = true;
        resetBall(player == 0 ? 1 : -1);
    }
}

void launchProjectile(int player) {
    if (g_app.attackCooldown[player] > 0) return;
    constexpr int cost = 25;
    if (!g_app.cheats[2] && g_app.super[player] < cost) return;
    const int character = g_app.selectedCharacters[static_cast<std::size_t>(player)];
    const auto& attack = kCharacterPrimaryAttacks[static_cast<std::size_t>(character)];
    for (auto& projectile : g_app.projectiles) {
        if (projectile.active) continue;
        projectile.active = true;
        projectile.owner = player;
        projectile.x = player == 0 ? 55.0f : 475.0f;
        projectile.y = (player == 0 ? g_app.player1Y : g_app.player2Y) + 17.0f;
        projectile.velocityX = player == 0 ? 6.5f : -6.5f;
        projectile.velocityY = 0.0f;
        projectile.frame = 0;
        projectile.damage = attack.damage;
        projectile.originalType = attack.originalType;
        if (!g_app.cheats[2]) g_app.super[player] -= cost;
        g_app.attackCooldown[player] = 28;
        return;
    }
}

bool gamepadButtonPressedForPlayer(int pad, WORD button) {
    return pad >= 0 && pad < XUSER_MAX_COUNT &&
           gamepadButtonPressed(static_cast<std::size_t>(pad), button);
}

void triggerPaddleAttack(int player, int attack) {
    g_app.animationTicks[player] = std::max(g_app.animationTicks[player], 22);
    g_app.animationFrame[player] = attack * 6;

    const auto& paddle = activePaddleSprite(player);
    const int ballWidth = g_app.ballSprite ? g_app.ballSprite.width : 16;
    const int ballHeight = g_app.ballSprite ? g_app.ballSprite.height : 16;
    const float paddleX = player == 0 ? 24.0f : 520.0f - static_cast<float>(paddle.width);
    const bool horizontallyClose = player == 0
        ? g_app.ballX <= paddleX + paddle.width + 34.0f && g_app.ballX + ballWidth >= paddleX
        : g_app.ballX + ballWidth >= paddleX - 34.0f && g_app.ballX <= paddleX + paddle.width;
    const bool verticallyClose = g_app.ballY + ballHeight >=
                                     (player == 0 ? g_app.player1Y : g_app.player2Y) - 12.0f &&
                                 g_app.ballY <=
                                     (player == 0 ? g_app.player1Y : g_app.player2Y) +
                                         paddle.height + 12.0f;
    if (!horizontallyClose || !verticallyClose) return;

    const float direction = player == 0 ? 1.0f : -1.0f;
    g_app.ballVelocityX = direction * std::min(6.4f, std::abs(g_app.ballVelocityX) + 0.65f);
    if (attack == 0) g_app.ballVelocityY = -std::max(2.4f, std::abs(g_app.ballVelocityY));
    if (attack == 1) g_app.ballVelocityY *= 0.55f;
    if (attack == 2) g_app.ballVelocityY = std::max(2.4f, std::abs(g_app.ballVelocityY));
}

void updateProjectiles() {
    const auto& left = activePaddleSprite(0);
    const auto& right = activePaddleSprite(1);
    for (auto& projectile : g_app.projectiles) {
        if (!projectile.active) continue;
        const auto& sprite = activeProjectileSprite(projectile);
        projectile.x += projectile.velocityX;
        projectile.y += projectile.velocityY;
        ++projectile.frame;
        const int width = sprite ? sprite.width : 12;
        const int height = sprite ? sprite.height : 12;
        if (projectile.owner == 0 && projectile.x + width >= 520 - right.width &&
            projectile.y + height >= g_app.player2Y &&
            projectile.y <= g_app.player2Y + right.height) {
            projectile.active = false;
            damagePlayer(1, projectile.damage);
            g_app.super[0] = std::min(kMaximumSuper, g_app.super[0] + 8);
        } else if (projectile.owner == 1 && projectile.x <= 24 + left.width &&
                   projectile.y + height >= g_app.player1Y &&
                   projectile.y <= g_app.player1Y + left.height) {
            projectile.active = false;
            damagePlayer(0, projectile.damage);
            g_app.super[1] = std::min(kMaximumSuper, g_app.super[1] + 8);
        } else if (projectile.x < -width || projectile.x > 544.0f) {
            projectile.active = false;
        }
    }
}

void updateMatch() {
    if (g_app.screen != Screen::match) return;
    ++g_app.frameCounter;
    for (int player = 0; player < 2; ++player) {
        if (g_app.attackCooldown[player] > 0) --g_app.attackCooldown[player];
        if (g_app.animationTicks[player] > 0) {
            --g_app.animationTicks[player];
            g_app.animationFrame[player] = (48 - g_app.animationTicks[player]) / 2;
        }
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
            if (g_app.roundIntroStage >= 59) g_app.roundIntroActive = false;
        }
        invalidate();
        return;
    }

    constexpr float paddleSpeed = 4.2f;
    float player1Move = 0.0f;
    if (g_app.player1Pad >= 0) {
        player1Move = gamepadVertical(g_app.player1Pad);
    } else {
        if (GetAsyncKeyState('W') & 0x8000) player1Move -= 1.0f;
        if (GetAsyncKeyState('S') & 0x8000) player1Move += 1.0f;
    }
    const bool player1Turbo = g_app.player1Pad >= 0
        ? (g_app.gamepads[static_cast<std::size_t>(g_app.player1Pad)].Gamepad.wButtons &
           XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0
        : (GetAsyncKeyState('5') & 0x8000) != 0;
    const float player1Speed = player1Turbo && g_app.turbo[0] > 0
        ? paddleSpeed * 1.65f : paddleSpeed;
    if (player1Turbo && player1Move != 0.0f && g_app.turbo[0] > 0) {
        --g_app.turbo[0];
    } else if ((g_app.frameCounter & 3) == 0) {
        g_app.turbo[0] = std::min(kMaximumTurbo, g_app.turbo[0] + 1);
    }
    g_app.player1Y += player1Move * player1Speed;
    if (g_app.playerCount == 2) {
        float player2Move = 0.0f;
        if (g_app.player2Pad >= 0) {
            player2Move = gamepadVertical(g_app.player2Pad);
        } else {
            if (GetAsyncKeyState(VK_UP) & 0x8000) player2Move -= 1.0f;
            if (GetAsyncKeyState(VK_DOWN) & 0x8000) player2Move += 1.0f;
        }
        const bool player2Turbo = g_app.player2Pad >= 0
            ? (g_app.gamepads[static_cast<std::size_t>(g_app.player2Pad)].Gamepad.wButtons &
               XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0
            : (GetAsyncKeyState('0') & 0x8000) != 0;
        const float player2Speed = player2Turbo && g_app.turbo[1] > 0
            ? paddleSpeed * 1.65f : paddleSpeed;
        if (player2Turbo && player2Move != 0.0f && g_app.turbo[1] > 0) {
            --g_app.turbo[1];
        } else if ((g_app.frameCounter & 3) == 0) {
            g_app.turbo[1] = std::min(kMaximumTurbo, g_app.turbo[1] + 1);
        }
        g_app.player2Y += player2Move * player2Speed;
    } else {
        const float target = g_app.ballY - 22.0f;
        g_app.player2Y += std::clamp(target - g_app.player2Y, -2.85f, 2.85f);
    }
    g_app.player1Y = std::clamp(g_app.player1Y, 58.0f, 366.0f);
    g_app.player2Y = std::clamp(g_app.player2Y, 58.0f, 366.0f);
    constexpr std::array<int, 3> player1AttackKeys{'1', '2', '3'};
    constexpr std::array<int, 3> player2AttackKeys{'6', '7', '8'};
    constexpr std::array<WORD, 3> gamepadAttackButtons{
        XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B};
    for (int attack = 0; attack < 3; ++attack) {
        const bool player1Attack = g_app.player1Pad >= 0
            ? gamepadButtonPressedForPlayer(g_app.player1Pad,
                                            gamepadAttackButtons[static_cast<std::size_t>(attack)])
            : (GetAsyncKeyState(player1AttackKeys[static_cast<std::size_t>(attack)]) & 1) != 0;
        if (player1Attack) triggerPaddleAttack(0, attack);
        if (g_app.playerCount == 2) {
            const bool player2Attack = g_app.player2Pad >= 0
                ? gamepadButtonPressedForPlayer(g_app.player2Pad,
                                                gamepadAttackButtons[static_cast<std::size_t>(attack)])
                : (GetAsyncKeyState(player2AttackKeys[static_cast<std::size_t>(attack)]) & 1) != 0;
            if (player2Attack) triggerPaddleAttack(1, attack);
        }
    }
    const bool player1Super = g_app.player1Pad >= 0
        ? gamepadButtonPressedForPlayer(g_app.player1Pad, XINPUT_GAMEPAD_Y)
        : (GetAsyncKeyState('4') & 1) != 0;
    const bool player2Super = g_app.playerCount == 2 &&
        (g_app.player2Pad >= 0
             ? gamepadButtonPressedForPlayer(g_app.player2Pad, XINPUT_GAMEPAD_Y)
             : (GetAsyncKeyState('9') & 1) != 0);
    if (player1Super) launchProjectile(0);
    if (player2Super) launchProjectile(1);
    if (g_app.playerCount == 1 && g_app.super[1] >= 25 && g_app.attackCooldown[1] == 0 &&
        (g_app.frameCounter % 180) == 0) launchProjectile(1);
    updateProjectiles();
    if (g_app.cheats[3]) {
        invalidate();
        return;
    }
    const float timeScale = g_app.cheats[4] ? 0.45f : 1.0f;
    g_app.ballX += g_app.ballVelocityX * timeScale;
    g_app.ballY += g_app.ballVelocityY * timeScale;
    const int ballWidth = g_app.ballSprite ? g_app.ballSprite.width : 12;
    const int ballHeight = g_app.ballSprite ? g_app.ballSprite.height : 12;
    if (g_app.ballY <= 55.0f || g_app.ballY + ballHeight >= 428.0f) {
        g_app.ballY = std::clamp(g_app.ballY, 55.0f, 428.0f - ballHeight);
        g_app.ballVelocityY = -g_app.ballVelocityY;
    }
    const auto& left = g_app.standingPaddles[g_app.selectedCharacters[0]];
    const auto& right = g_app.standingPaddles[g_app.selectedCharacters[1]];
    const float leftEdge = 24.0f + std::max(8, left.width);
    const float rightEdge = 520.0f - std::max(8, right.width);
    if (g_app.ballVelocityX < 0 && g_app.ballX <= leftEdge && g_app.ballX + ballWidth >= 24.0f &&
        g_app.ballY + ballHeight >= g_app.player1Y &&
        g_app.ballY <= g_app.player1Y + std::max(54, left.height)) {
        g_app.ballX = leftEdge;
        g_app.ballVelocityX = std::min(6.4f, -g_app.ballVelocityX * 1.035f);
        g_app.ballVelocityY += (g_app.ballY - g_app.player1Y - 22.0f) * 0.025f;
        damagePlayer(0, 3);
        g_app.super[0] = std::min(kMaximumSuper, g_app.super[0] + 7);
    }
    if (g_app.ballVelocityX > 0 && g_app.ballX + ballWidth >= rightEdge &&
        g_app.ballX <= 520.0f && g_app.ballY + ballHeight >= g_app.player2Y &&
        g_app.ballY <= g_app.player2Y + std::max(54, right.height)) {
        g_app.ballX = rightEdge - ballWidth;
        g_app.ballVelocityX = std::max(-6.4f, -g_app.ballVelocityX * 1.035f);
        g_app.ballVelocityY += (g_app.ballY - g_app.player2Y - 22.0f) * 0.025f;
        damagePlayer(1, 3);
        g_app.super[1] = std::min(kMaximumSuper, g_app.super[1] + 7);
    }
    if (g_app.ballX < -ballWidth) {
        damagePlayer(0, 20);
        resetBall(1);
    } else if (g_app.ballX > 544.0f) {
        damagePlayer(1, 20);
        resetBall(-1);
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
                    invalidate();
                    return 0;
                }
                if (wParam == VK_DOWN) {
                    g_app.titleSelection = (g_app.titleSelection + 1) % 5;
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
                    invalidate();
                    return 0;
                }
                if (wParam == VK_RIGHT) {
                    character = (character + 1) % available;
                    invalidate();
                    return 0;
                }
                if (wParam == VK_RETURN || wParam == VK_SPACE) {
                    if (g_app.playerCount == 2 && g_app.selectingPlayer == 0) {
                        g_app.selectingPlayer = 1;
                        g_app.selectedCharacters[1] %= available;
                        invalidate();
                    } else if (g_app.playerCount == 2) {
                        g_app.screen = Screen::versusKode;
                        invalidate();
                    } else {
                        beginMatch();
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
                    invalidate();
                    return 0;
                }
                if (wParam == VK_RETURN || wParam == VK_SPACE) {
                    beginMatch();
                    return 0;
                }
                if (wParam == VK_ESCAPE) {
                    g_app.screen = Screen::characterSelect;
                    g_app.selectingPlayer = 1;
                    invalidate();
                    return 0;
                }
            } else if (g_app.screen == Screen::match) {
                if (wParam == VK_ESCAPE) {
                    g_app.screen = Screen::title;
                    playTitleMusic();
                    invalidate();
                    return 0;
                }
            } else if (g_app.screen == Screen::cheatMenu) {
                if (wParam == VK_UP) {
                    g_app.cheatSelection = (g_app.cheatSelection + 5) % 6;
                    invalidate();
                    return 0;
                }
                if (wParam == VK_DOWN) {
                    g_app.cheatSelection = (g_app.cheatSelection + 1) % 6;
                    invalidate();
                    return 0;
                }
                if (wParam == VK_RETURN || wParam == VK_SPACE) {
                    g_app.cheats[g_app.cheatSelection] = !g_app.cheats[g_app.cheatSelection];
                    if (g_app.cheatSelection == 5 && g_app.cheats[5]) g_app.allContentUnlocked = true;
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
                    invalidate();
                    return 0;
                }
                if (wParam == VK_DOWN) {
                    g_app.configSelection = (g_app.configSelection + 1) % 6;
                    invalidate();
                    return 0;
                }
                if (wParam == VK_LEFT) {
                    adjustConfiguration(-1);
                    return 0;
                }
                if (wParam == VK_RIGHT) {
                    adjustConfiguration(1);
                    return 0;
                }
                if (wParam == VK_RETURN || wParam == VK_SPACE) {
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
            }
            if (wParam == VK_ESCAPE && g_app.screen == Screen::title) {
                PostMessageW(window, WM_CLOSE, 0, 0);
                return 0;
            }
            break;
        case WM_TIMER:
            pollGamepads();
            postGamepadMenuInput();
            updateMatch();
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
    PlaySoundW(nullptr, nullptr, 0);
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
    g_app.configBanner = loadOriginalBitmap(instance, 2007, 303);
    for (int index = 0; index < 16; ++index) {
        g_app.portraits[index] = loadOriginalBitmap(instance, 2007, 1000 + index);
        g_app.standingPaddles[index] =
            loadOriginalSprite(instance, kCharacterResourceTypes[index], 1000);
        g_app.fighterNameSprites[index] = loadOriginalSprite(instance, 2023, 600 + index);
        const auto& attack = kCharacterPrimaryAttacks[static_cast<std::size_t>(index)];
        for (int frame = 0; frame < attack.frameCount; ++frame) {
            g_app.projectileFrames[index].push_back(
                loadOriginalSprite(instance, attack.resourceType,
                                   attack.firstResourceId + frame));
        }
        for (int frame = 0; frame < 24; ++frame) {
            g_app.paddleFrames[index][frame] =
                loadOriginalSprite(instance, kCharacterResourceTypes[index], 3000 + frame);
        }
    }
    g_app.portraits[16] = loadOriginalBitmap(instance, 2007, 2000);
    g_app.ballSprite = loadOriginalSprite(instance, 2004, 500);
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
    g_app.titleMusic = loadOriginalVoc(instance, 8000);
    g_app.matchMusic = loadOriginalVoc(instance, 8005);
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
