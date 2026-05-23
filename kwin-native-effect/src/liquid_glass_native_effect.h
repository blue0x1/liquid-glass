#pragma once

#include <kwin/effect/effect.h>
#include <kwin/effect/effectwindow.h>
#include <kwin/opengl/glframebuffer.h>
#include <kwin/opengl/glshader.h>
#include <kwin/opengl/gltexture.h>
#include <kwin/opengl/glvertexbuffer.h>

#include <QHash>
#include <QSize>
#include <memory>

namespace KWin
{

class GLShader;

class LiquidGlassNativeEffect : public Effect
{
    Q_OBJECT

public:
    LiquidGlassNativeEffect();
    ~LiquidGlassNativeEffect() override;

    static bool supported();

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintWindow(EffectWindow *window, WindowPrePaintData &data, std::chrono::milliseconds presentTime) override;
    void paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window, int mask, QRegion region, WindowPaintData &data) override;
    void postPaintWindow(EffectWindow *window) override;

private:
    struct CaptureBuffer
    {
        std::unique_ptr<GLTexture> texture;
        std::unique_ptr<GLFramebuffer> framebuffer;
        QSize size;
    };

    bool isLiquidGlassWindow(const EffectWindow *window) const;
    void ensureShader();
    void drawBackdropLayer(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window);
    CaptureBuffer &captureBufferFor(const QSize &size);
    void releaseShader();
    void releaseCaptureBuffers();

    std::unique_ptr<GLShader> m_shader;
    QHash<EffectWindow *, qint64> m_windowStart;
    CaptureBuffer m_captureBuffer;
};

}
