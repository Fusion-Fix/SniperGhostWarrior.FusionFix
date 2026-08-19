module;

#include <common.hxx>
#include <d3d9.h>

// Internal render resolution independent of the window and the backbuffer, for supersampling
// above what the display can show or downscaling.

export module internalres;

import common;
import chrome;
import settings;

template<typename T>
static void SafeRelease(T*& ptr)
{
    if (ptr) { ptr->Release(); ptr = nullptr; }
}

// ---------------------------------------------------------------------------------------------
// State

enum ScalingFilter
{
    FILTER_POINT = 0,
    FILTER_BILINEAR = 1,
};

// Renderer fields, reached through the pattern's own base register.
static constexpr ptrdiff_t nRendererScreenWidth = 0x637C;
static constexpr ptrdiff_t nRendererScreenHeight = 0x6380;

static int32_t nRequestedW = 0;         // from the ini; 0 leaves the game entirely alone
static int32_t nRequestedH = 0;
static int32_t nFilter = FILTER_BILINEAR;

static uint32_t nOutputW = 0;           // what the swapchain actually presents
static uint32_t nOutputH = 0;
static bool bSubstituted = false;

// The frame the engine draws, not the display. Two uses share the one pair:
//   - the backbuffer viewport in RSetRenderTargets, whose two operands are repointed at the
//     elements in place of the present parameters;
//   - what the camera projections multiply normalised coordinates by. They read game+0x7C
//     directly instead of calling IGame::GetScreenWidth, so they are handed this array in place
//     of the settings. Crosshair, waypoint and objective markers and their distance text.
// With no substitution installed it holds the real backbuffer size.
static int32_t nFrameSize[2] = { 0, 0 };

// Read out of the operands of the instructions that use them. ASLR moves the globals.
static IDirect3DDevice9** ppDevice = nullptr;
static IDirect3DSurface9** ppBackBuffer = nullptr;
static IDirect3DSurface9** ppDepthStencil = nullptr;

// Called from the mode change as well, before the device knows anything, so the size arriving is
// the output one either way.
static bool WantsInternalResolution(uint32_t nModeW, uint32_t nModeH)
{
    if (nRequestedW <= 0 || nRequestedH <= 0)
        return false;

    // Nothing to gain, and substituting would only cost a copy.
    return static_cast<uint32_t>(nRequestedW) != nModeW || static_cast<uint32_t>(nRequestedH) != nModeH;
}

// ---------------------------------------------------------------------------------------------
// The resolve pass

class CResolver
{
    struct ScreenVertex { float x, y, z, rhw, u, v; };
    static constexpr DWORD nScreenFVF = D3DFVF_XYZRHW | D3DFVF_TEX1;

    // Four halvings covers 16x per axis, far past anything a GPU will render this game at.
    static constexpr size_t nMaxChain = 4;

    struct Target
    {
        IDirect3DTexture9* pTex = nullptr;
        IDirect3DSurface9* pSurf = nullptr;
        UINT nWidth = 0;
        UINT nHeight = 0;
        D3DFORMAT eFormat = D3DFMT_UNKNOWN;

        void Release()
        {
            SafeRelease(pSurf);
            SafeRelease(pTex);
            nWidth = 0;
            nHeight = 0;
            eFormat = D3DFMT_UNKNOWN;
        }

        bool Ensure(IDirect3DDevice9* pDevice, UINT w, UINT h, D3DFORMAT fmt)
        {
            if (pTex && pSurf && nWidth == w && nHeight == h && eFormat == fmt)
                return true;

            Release();

            if (FAILED(pDevice->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, fmt,
                                              D3DPOOL_DEFAULT, &pTex, nullptr)))
                return false;

            if (FAILED(pTex->GetSurfaceLevel(0, &pSurf)))
            {
                Release();
                return false;
            }

            nWidth = w;
            nHeight = h;
            eFormat = fmt;
            return true;
        }
    };

    // The two surfaces handed to the engine in place of the ones the device made. Nothing reads
    // the depth buffer back, so it needs no texture behind it.
    static inline Target Substitute;
    static inline IDirect3DSurface9* pSubstituteDepth = nullptr;
    static inline Target Chain[nMaxChain];

    static inline IDirect3DStateBlock9* pStateBlock = nullptr;

    // One textured quad from pSrc into a rect of pDst: the whole target for a halving stage, the
    // aspect-preserving fit for the final pass.
    static bool Blit(IDirect3DDevice9* pDevice, IDirect3DSurface9* pDst, UINT nTargetW, UINT nTargetH,
                     int32_t nDstX, int32_t nDstY, UINT nDstW, UINT nDstH,
                     IDirect3DTexture9* pSrc, int32_t eFilter)
    {
        // Depth first every time. D3D9 rejects a render target smaller than the depth-stencil
        // bound with it, and while supersampling every target in this chain is smaller.
        pDevice->SetDepthStencilSurface(nullptr);

        if (FAILED(pDevice->SetRenderTarget(0, pDst)))
            return false;

        // Whole target rather than the fitted rect, so the clear below reaches the bars.
        const D3DVIEWPORT9 viewport = { 0, 0, nTargetW, nTargetH, 0.0f, 1.0f };
        pDevice->SetViewport(&viewport);

        // The swapchain is D3DSWAPEFFECT_DISCARD, so bars hold whatever the driver last left
        // there unless they are written every frame.
        if (nDstX != 0 || nDstY != 0 || nDstW != nTargetW || nDstH != nTargetH)
            pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 1.0f, 0);

        pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        pDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
        pDevice->SetRenderState(D3DRS_CLIPPING, TRUE);
        pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0000000F);
        // The frame is already in whatever space the game finished in, and the engine's own
        // surface copies write no sRGB either, so converting here would shift every pixel.
        pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

        const DWORD nSampler = (eFilter == FILTER_POINT) ? D3DTEXF_POINT : D3DTEXF_LINEAR;

        pDevice->SetTexture(0, pSrc);
        pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, nSampler);
        pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, nSampler);
        pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        pDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

        // Clears the vertex shader and the declaration with it.
        pDevice->SetFVF(nScreenFVF);

        // Restored by the state block afterwards. Wrapping would make the 0-to-1 texcoord take
        // the short way round.
        pDevice->SetRenderState(D3DRS_WRAP0, 0);
        pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

        pDevice->SetPixelShader(nullptr);
        pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        // Disabling stage 1 ends the chain, so the higher stages sample nothing whatever is left
        // bound to them.
        pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

        // D3D9's half-pixel texel-centre offset, folded into the rect origin.
        const auto fLeft = static_cast<float>(nDstX) - 0.5f;
        const auto fTop = static_cast<float>(nDstY) - 0.5f;
        const auto fRight = fLeft + static_cast<float>(nDstW);
        const auto fBottom = fTop + static_cast<float>(nDstH);

        const ScreenVertex verts[4] =
        {
            { fLeft,  fTop,    0.0f, 1.0f, 0.0f, 0.0f },
            { fRight, fTop,    0.0f, 1.0f, 1.0f, 0.0f },
            { fLeft,  fBottom, 0.0f, 1.0f, 0.0f, 1.0f },
            { fRight, fBottom, 0.0f, 1.0f, 1.0f, 1.0f },
        };

        pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(ScreenVertex));

        // The next stage of the chain renders into this texture.
        pDevice->SetTexture(0, nullptr);
        return true;
    }

public:
    static IDirect3DSurface9* Surface() { return Substitute.pSurf; }
    static IDirect3DSurface9* DepthSurface() { return pSubstituteDepth; }

    // Depth is discardable, like the auto depth-stencil it replaces: the engine clears it every
    // frame rather than reading the previous one. The state block is taken here rather than in
    // the resolve so a failure means no substitution at all, instead of a frame with no way back
    // to the engine's state.
    static bool Ensure(IDirect3DDevice9* pDevice, UINT w, UINT h, D3DFORMAT fmt, D3DFORMAT eDepthFormat)
    {
        SafeRelease(pSubstituteDepth);
        SafeRelease(pStateBlock);

        return Substitute.Ensure(pDevice, w, h, fmt)
            && SUCCEEDED(pDevice->CreateDepthStencilSurface(w, h, eDepthFormat, D3DMULTISAMPLE_NONE, 0,
                                                            TRUE, &pSubstituteDepth, nullptr))
            && SUCCEEDED(pDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock));
    }

    // Called at the entry of the renderer's present function. EndScene has already run and the
    // only thing left in that function is Present itself, so this is the last point at which the
    // frame is still the engine's and the first at which it is safe to draw with.
    static void Resolve(IDirect3DDevice9* pDevice)
    {
        if (!pDevice || !Substitute.pSurf || !pStateBlock)
            return;

        IDirect3DSurface9* pBackBuffer = nullptr;
        if (FAILED(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)) || !pBackBuffer)
            return;

        // Matching sizes are no reason to skip the copy. The frame is in the substitute and
        // nothing else will put it in front of the player.
        D3DSURFACE_DESC desc{};
        pBackBuffer->GetDesc(&desc);

        // The largest centred rect of the backbuffer with the internal frame's shape, so 4:3
        // internal on a 16:9 display gets pillarboxed rather than stretched. Taken from the
        // substitute rather than the chain's output, whose halving stages round.
        UINT nFitW = desc.Width;
        UINT nFitH = desc.Height;
        {
            const auto w = static_cast<uint64_t>(Substitute.nWidth);
            const auto h = static_cast<uint64_t>(Substitute.nHeight);

            if (w * desc.Height > h * desc.Width)
                nFitH = (std::max)(1u, static_cast<UINT>((desc.Width * h + w / 2) / w));
            else if (h * desc.Width > w * desc.Height)
                nFitW = (std::max)(1u, static_cast<UINT>((desc.Height * w + h / 2) / h));
        }

        const auto nFitX = static_cast<int32_t>((desc.Width - nFitW) / 2);
        const auto nFitY = static_cast<int32_t>((desc.Height - nFitH) / 2);

        pStateBlock->Capture();

        // A state block does not cover render targets, so those are saved by hand.
        IDirect3DSurface9* pOldTarget = nullptr;
        IDirect3DSurface9* pOldDepth = nullptr;
        pDevice->GetRenderTarget(0, &pOldTarget);
        pDevice->GetDepthStencilSurface(&pOldDepth);

        // EndScene 0x102F2CC0 has already run by the time the present function is entered, so
        // the draws below need a scene of their own.
        const bool bOpenedScene = SUCCEEDED(pDevice->BeginScene());

        IDirect3DTexture9* pSrc = Substitute.pTex;
        UINT nSrcW = Substitute.nWidth;
        UINT nSrcH = Substitute.nHeight;

        // A bilinear tap is an exact 2x2 box average only at half size, so anything past 2:1 is
        // halved through scratch targets first. Four taps cannot represent a sixteen texel
        // footprint, which is what makes a single stretch look no better than no supersampling.
        // The chain ends at the fitted rect, not the backbuffer, since that is the reduction the
        // final pass has to cover. Either axis at 2:1 is reason enough: requiring both would let
        // a frame oversampled on one axis only take its whole reduction on four taps.
        for (auto& stage : Chain)
        {
            if (nSrcW < nFitW * 2 && nSrcH < nFitH * 2)
                break;

            const UINT dw = (nSrcW >= nFitW * 2) ? (std::max)(nSrcW / 2, nFitW) : nSrcW;
            const UINT dh = (nSrcH >= nFitH * 2) ? (std::max)(nSrcH / 2, nFitH) : nSrcH;

            // A stage that did not run still holds the previous frame.
            if (!stage.Ensure(pDevice, dw, dh, Substitute.eFormat)
                || !Blit(pDevice, stage.pSurf, dw, dh, 0, 0, dw, dh, pSrc, FILTER_BILINEAR))
                break;

            pSrc = stage.pTex;
            nSrcW = dw;
            nSrcH = dh;
        }

        Blit(pDevice, pBackBuffer, desc.Width, desc.Height, nFitX, nFitY, nFitW, nFitH, pSrc, nFilter);

        if (bOpenedScene)
            pDevice->EndScene();

        // Both or neither. Slot 0 cannot be unbound, so restoring depth against a failed
        // GetRenderTarget would bind the engine's larger buffer to the output-sized backbuffer and
        // be rejected, costing the rest of the frame its depth.
        if (pOldTarget)
        {
            pDevice->SetRenderTarget(0, pOldTarget);
            pDevice->SetDepthStencilSurface(pOldDepth);
        }

        pStateBlock->Apply();

        SafeRelease(pOldDepth);
        SafeRelease(pOldTarget);
        SafeRelease(pBackBuffer);
    }

    // Process teardown, and the tail of the pre-reset release below. The device may already be
    // gone, so nothing is asked of it.
    static void ReleaseAll()
    {
        for (auto& target : Chain)
            target.Release();

        Substitute.Release();
        SafeRelease(pSubstituteDepth);
        SafeRelease(pStateBlock);
    }

    // Runs at the entry of RChangeDeviceState 0x10348A40, which resets or recreates the device.
    // Everything in D3DPOOL_DEFAULT has to be gone before Reset, the substitute's render target
    // binding included: left bound, Reset returns D3DERR_INVALIDCALL and the screen stays black
    // for good after the first alt-tab.
    //
    // The references held by the two renderer globals are not dropped and the surfaces are left
    // installed. The engine's own pre-reset pass releases both globals with no null check, and
    // that release is what destroys them.
    static void ReleaseForReset(IDirect3DDevice9* pDevice)
    {
        if (pDevice)
        {
            // Depth first: a render target smaller than the bound depth buffer is rejected.
            pDevice->SetDepthStencilSurface(nullptr);

            // The engine's release pass unbinds slots 1 to 3 and the depth buffer, leaving slot
            // 0 alone. That is only safe when slot 0 holds the device's own backbuffer.
            IDirect3DSurface9* pBackBuffer = nullptr;
            if (SUCCEEDED(pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)) && pBackBuffer)
            {
                pDevice->SetRenderTarget(0, pBackBuffer);
                pBackBuffer->Release();
            }

            for (DWORD i = 1; i < 4; ++i)
                pDevice->SetRenderTarget(i, nullptr);

            pDevice->SetTexture(0, nullptr);
        }

        // Only this module's own references. The globals keep theirs, and the engine's
        // unconditional release a few instructions from here is what takes them to zero.
        ReleaseAll();
    }
};

// ---------------------------------------------------------------------------------------------
// Hook bodies

// RChangeDeviceState's tail, with the device created or reset and both surfaces just acquired
// from it. Size and format come from the surface rather than the mode, so a fullscreen resolution
// the driver rounded to a real adapter mode is described by what the device actually made.
static void OnDeviceChanged()
{
    bSubstituted = false;
    nOutputW = 0;
    nOutputH = 0;

    if (!ppDevice || !ppBackBuffer || !ppDepthStencil)
        return;

    auto* pDevice = *ppDevice;
    auto* pRealBackBuffer = *ppBackBuffer;
    if (!pDevice || !pRealBackBuffer)
        return;

    D3DSURFACE_DESC bbDesc{};
    pRealBackBuffer->GetDesc(&bbDesc);

    nOutputW = bbDesc.Width;
    nOutputH = bbDesc.Height;

    nFrameSize[0] = static_cast<int32_t>(nOutputW);
    nFrameSize[1] = static_cast<int32_t>(nOutputH);

    if (!WantsInternalResolution(nOutputW, nOutputH))
        return;

    // The device's depth buffer is the output size, too small for the substitute on every
    // supersampled frame. Format read off the real surface, so the engine keeps its stencil bits.
    auto* pRealDepth = *ppDepthStencil;
    auto eDepthFormat = D3DFMT_D24S8;
    if (pRealDepth)
    {
        D3DSURFACE_DESC dsDesc{};
        pRealDepth->GetDesc(&dsDesc);
        eDepthFormat = dsDesc.Format;
    }

    if (!CResolver::Ensure(pDevice, static_cast<UINT>(nRequestedW), static_cast<UINT>(nRequestedH),
                           bbDesc.Format, eDepthFormat))
    {
        CResolver::ReleaseAll();
        return;
    }

    nFrameSize[0] = nRequestedW;
    nFrameSize[1] = nRequestedH;

    // Each global owns one reference and the engine releases it unconditionally on the next
    // device change, so the reference it was handed is replaced rather than added to.
    auto* pSubstitute = CResolver::Surface();
    pSubstitute->AddRef();
    *ppBackBuffer = pSubstitute;
    pRealBackBuffer->Release();

    auto* pSubstituteDepth = CResolver::DepthSurface();
    pSubstituteDepth->AddRef();
    *ppDepthStencil = pSubstituteDepth;
    if (pRealDepth)
        pRealDepth->Release();

    // The device comes back with its own backbuffer bound at slot 0 and the engine only rebinds
    // through the globals above, so a frame that never asks for the backbuffer by name would
    // render into the real one. Bound here as well as installed; the resolve puts it back.
    pDevice->SetRenderTarget(0, pSubstitute);
    pDevice->SetDepthStencilSurface(pSubstituteDepth);

    bSubstituted = true;
}

// What GameDLL_x86.dll lays every menu screen out from. UIMainMenuScreen::RecalcMetrics
// 0x101FC3F0 scales its 800x600 design metrics by GetScreenWidth()/800 and GetScreenHeight()/600,
// and every menu class derives from it, so the whole family is sized for the display and drawn
// into the internal frame: off the edge below it, half size and cornered above it. The intro movie
// 0x10174BCA and the letterbox belts come from the same two calls.
//
// Nothing inside engine_x86.dll calls either, so the settings struct they read is left alone and
// the window, the video menu and the next mode change still see the output resolution.
static safetyhook::InlineHook fnGetScreenWidth{};
static safetyhook::InlineHook fnGetScreenHeight{};

// EAX holds the settings each projection is about to read its screen pair out of.
static void RedirectProjectionSize(SafetyHookContext& regs)
{
    if (bSubstituted)
        regs.eax = reinterpret_cast<uintptr_t>(nFrameSize);
}

static int __fastcall GetScreenWidth(void* pThis, void* pUnused)
{
    return bSubstituted ? nRequestedW : fnGetScreenWidth.thiscall<int>(pThis);
}

static int __fastcall GetScreenHeight(void* pThis, void* pUnused)
{
    return bSubstituted ? nRequestedH : fnGetScreenHeight.thiscall<int>(pThis);
}

class InternalResolution
{
public:
    InternalResolution()
    {
        FusionFix::onEngineInitEvent() += []()
        {
            // RChangeDeviceState 0x10348D75: GetBackBuffer, GetDepthStencilSurface, GetGammaRamp,
            // reached by the create path and the reset path alike. Operands at +4, +18 and +31
            // carry the backbuffer, device and depth globals.
            auto deviceTail = engine_pattern("8B 51 48 68 ? ? ? ? 6A 00 6A 00 6A 00 50 FF D2 A1 ? ? ? ? 8B 08 8B 91 A0 00 00 00 68 ? ? ? ? 50 FF D2");

            // RChangeDeviceState 0x10348A40, ahead of the engine's pre-reset release pass.
            auto deviceChange = engine_pattern("51 8B 44 24 14 8B 0D ? ? ? ? 53 55 8B 6C 24 18 56 8B D8 C1 EB 05");

            // CRenderer::SetMode 0x102F15E0, past the second of its two block copies. The new
            // mode has landed in the renderer's fields and the caller's struct still holds the
            // pair RChangeDeviceState is about to be given.
            auto setMode = engine_pattern("8D 83 7C 63 00 00 8B F0 B9 07 00 00 00 8D 7C 24 1C F3 A5 8B F2 8B F8 B9 07 00 00 00 F3 A5");

            // RSetRenderTargets 0x1034A20A, where the backbuffer's viewport comes from the
            // present parameters. Every other viewport comes from a render target already sized
            // by the renderer's resolution.
            auto backBufferViewport = engine_pattern("8B 0D ? ? ? ? 3B CB 89 1D ? ? ? ? 0F 84 ? ? ? ? A1 ? ? ? ? E9");

            // 0x10110ECE, the screen size the game object hands the UI at +0x70/+0x74, written
            // alongside the aspect ratio at +0xC0 that comes from the same pair.
            auto uiScreenSize = engine_pattern("57 50 52 89 56 70 89 46 74 8B 39 8B 97 EC 00 00 00");

            // PointToScreen 0x1003045F, PointToScreenClampToFrustum 0x10030BC9 and
            // GetScreenExtents 0x1003E658, each just past the load of the video settings pointer
            // they read the pair out of. Redirecting the pointer leaves the arithmetic untouched.
            auto pointToScreen = engine_pattern("8B 0D ? ? ? ? D8 F9 8B 41 7C D9 04 24 D8 C9 D9 C9 D8 4C 24 04");
            auto pointToScreenClamped = engine_pattern("8B 0D ? ? ? ? D9 C9 8B 41 7C D8 C2 D9 C9 DE E2 DA 08");
            auto screenExtents = engine_pattern("8B 0D ? ? ? ? 8B 41 7C DB 00 8D 74 24 38 BF 08 00 00 00 D9 5C 24 30 DB 40 04");

            // IGame::GetScreenWidth 0x101189E0 and GetScreenHeight 0x101189F0, adjacent and each
            // followed by its own padding, so one scan finds both and a 5 byte detour fits in
            // either without reaching the next body.
            auto screenSizeGetters = engine_pattern("8B 41 04 8B 48 7C 8B 01 C3 CC CC CC CC CC CC CC 8B 41 04 8B 48 7C 8B 41 04 C3");

            // 0x1011AA0F and 0x1011AF52, where the two apply paths ask the renderer what
            // resolution it ended up at and write the answer into the video settings. One then
            // resizes the window from it, the other hands it to the UI, and the next mode change
            // is taken from the same pair. Left alone the internal resolution travels back out
            // through the settings: the window shrinks to it and the swapchain is asked for it.
            auto videoSettingsApply = engine_pattern("8B 82 00 01 00 00 FF D0 8B 0D ? ? ? ? 89 03 89 45 00 8B 11 8B 82 04 01 00 00 FF D0 8B 0D ? ? ? ? 89 45 20 89 45 04");
            auto videoSettingsReapply = engine_pattern("8B 82 00 01 00 00 FF D0 8B 0D ? ? ? ? 89 43 1C 89 03 8B 11 8B 82 04 01 00 00 FF D0 8B 0D ? ? ? ? 89 43 20 89 43 04");

            // CRenderer::Present 0x102F3820. EndScene has run and Present is all that is left.
            auto present = engine_pattern("55 8B EC 83 E4 F8 83 EC 34 53 56 8B F1 8B 0D ? ? ? ? 85 C9");

            // All or nothing: a half-installed substitution renders into a surface no pass puts
            // back on screen.
            if (deviceTail.empty() || deviceChange.empty() || setMode.empty() || backBufferViewport.empty()
                || uiScreenSize.empty() || pointToScreen.empty() || pointToScreenClamped.empty()
                || screenExtents.empty() || screenSizeGetters.empty() || videoSettingsApply.empty()
                || videoSettingsReapply.empty() || present.empty())
                return;

            ppBackBuffer = *deviceTail.get_first<IDirect3DSurface9**>(4);
            ppDevice = *deviceTail.get_first<IDirect3DDevice9**>(18);
            ppDepthStencil = *deviceTail.get_first<IDirect3DSurface9**>(31);

            // Seeded from the present parameters the operands are about to stop pointing at,
            // which hold nothing until the device has been created once.
            nFrameSize[0] = **backBufferViewport.get_first<int32_t*>(2);
            nFrameSize[1] = **backBufferViewport.get_first<int32_t*>(21);

            injector::WriteMemory<uintptr_t>(backBufferViewport.get_first(2), reinterpret_cast<uintptr_t>(&nFrameSize[0]), true);
            injector::WriteMemory<uintptr_t>(backBufferViewport.get_first(21), reinterpret_cast<uintptr_t>(&nFrameSize[1]), true);

            static auto InternalResolutionCB = []()
            {
                nRequestedW = FusionFixSettings.GetInt(PREF_INTERNALRESOLUTIONX);
                nRequestedH = FusionFixSettings.GetInt(PREF_INTERNALRESOLUTIONY);
                nFilter = FusionFixSettings.GetInt(PREF_SCALINGFILTER);

                // One axis without the other is a typo rather than a request.
                if (nRequestedW <= 0 || nRequestedH <= 0)
                {
                    nRequestedW = 0;
                    nRequestedH = 0;
                }
            };

            InternalResolutionCB();

            // EBX is the renderer. Replacing the fields here leaves the caller's pair, which is
            // what RChangeDeviceState gets, describing the output.
            static auto SetModeHook = safetyhook::create_mid(setMode.get_first(30), [](SafetyHookContext& regs)
            {
                if (regs.ebx == 0)
                    return;

                auto* pWidth = reinterpret_cast<uint32_t*>(regs.ebx + nRendererScreenWidth);
                auto* pHeight = reinterpret_cast<uint32_t*>(regs.ebx + nRendererScreenHeight);

                if (!WantsInternalResolution(*pWidth, *pHeight))
                    return;

                *pWidth = static_cast<uint32_t>(nRequestedW);
                *pHeight = static_cast<uint32_t>(nRequestedH);
            });

            // EDX and EAX are the width and height on their way into the game object and into
            // the viewport the same call sets, so one rewrite covers both.
            static auto UiScreenSizeHook = safetyhook::create_mid(uiScreenSize.get_first(0), [](SafetyHookContext& regs)
            {
                if (!WantsInternalResolution(static_cast<uint32_t>(regs.edx), static_cast<uint32_t>(regs.eax)))
                    return;

                regs.edx = static_cast<uintptr_t>(nRequestedW);
                regs.eax = static_cast<uintptr_t>(nRequestedH);
            });

            // Pending pair at +0x00, active pair at +0x1C, both answered with what the swapchain
            // was created at. IGame::GetScreenWidth reads +0x00 and is detoured separately.
            static auto RestoreSettingsResolution = [](uintptr_t pSettings)
            {
                if (!bSubstituted || pSettings == 0 || nOutputW == 0 || nOutputH == 0)
                    return;

                *reinterpret_cast<uint32_t*>(pSettings + 0x00) = nOutputW;
                *reinterpret_cast<uint32_t*>(pSettings + 0x04) = nOutputH;
                *reinterpret_cast<uint32_t*>(pSettings + 0x1C) = nOutputW;
                *reinterpret_cast<uint32_t*>(pSettings + 0x20) = nOutputH;
            };

            // EBP holds the settings struct in the apply path, EBX in the reapply path; neither
            // function keeps a frame pointer of its own.
            static auto VideoSettingsApplyHook = safetyhook::create_mid(videoSettingsApply.get_first(41), [](SafetyHookContext& regs)
            {
                RestoreSettingsResolution(regs.ebp);
            });

            static auto VideoSettingsReapplyHook = safetyhook::create_mid(videoSettingsReapply.get_first(41), [](SafetyHookContext& regs)
            {
                RestoreSettingsResolution(regs.ebx);
            });

            static auto PointToScreenHook = safetyhook::create_mid(pointToScreen.get_first(11), RedirectProjectionSize);
            static auto PointToScreenClampedHook = safetyhook::create_mid(pointToScreenClamped.get_first(11), RedirectProjectionSize);
            static auto ScreenExtentsHook = safetyhook::create_mid(screenExtents.get_first(9), RedirectProjectionSize);

            fnGetScreenWidth = safetyhook::create_inline(screenSizeGetters.get_first(0), reinterpret_cast<void*>(GetScreenWidth));
            fnGetScreenHeight = safetyhook::create_inline(screenSizeGetters.get_first(16), reinterpret_cast<void*>(GetScreenHeight));

            static auto DeviceChangeHook = safetyhook::create_mid(deviceChange.get_first(0), [](SafetyHookContext& regs)
            {
                // Unconditional. Nothing this module owns may outlive the Reset that follows.
                CResolver::ReleaseForReset(ppDevice ? *ppDevice : nullptr);
                bSubstituted = false;
            });

            static auto DeviceTailHook = safetyhook::create_mid(deviceTail.get_first(38), [](SafetyHookContext& regs)
            {
                OnDeviceChanged();
            });

            static auto PresentHook = safetyhook::create_mid(present.get_first(0), [](SafetyHookContext& regs)
            {
                if (bSubstituted)
                    CResolver::Resolve(ppDevice ? *ppDevice : nullptr);
            });

            // Read when the mode is set, so a change lands on the next launch or mode change.
            FusionFix::onIniFileChange() += []()
            {
                InternalResolutionCB();
            };
        };

        FusionFix::onShutdownEvent() += []()
        {
            CResolver::ReleaseAll();
        };
    }
} InternalResolution;
