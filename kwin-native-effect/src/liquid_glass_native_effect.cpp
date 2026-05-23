#include "liquid_glass_native_effect.h"

#include <kwin/core/rendertarget.h>
#include <kwin/core/renderviewport.h>
#include <kwin/effect/effecthandler.h>
#include <kwin/opengl/glframebuffer.h>
#include <kwin/opengl/glshadermanager.h>
#include <kwin/opengl/glvertexbuffer.h>

#include <QFile>
#include <QVariant>
#include <QDateTime>
#include <QStandardPaths>
#include <QVector2D>
#include <array>
#include <chrono>

namespace KWin
{

LiquidGlassNativeEffect::LiquidGlassNativeEffect()
{
    reconfigure(ReconfigureAll);
}

LiquidGlassNativeEffect::~LiquidGlassNativeEffect()
{
    releaseShader();
    releaseCaptureBuffers();
}

bool LiquidGlassNativeEffect::supported()
{
    return effects && effects->isOpenGLCompositing();
}

void LiquidGlassNativeEffect::reconfigure(ReconfigureFlags flags)
{
    Q_UNUSED(flags)
    releaseShader();
    releaseCaptureBuffers();
}

bool LiquidGlassNativeEffect::isLiquidGlassWindow(const EffectWindow *window) const
{
    if (!window || window->isDeleted() || window->isDesktop() || window->isDock()) {
        return false;
    }

    const QString cls = window->windowClass().toLower();
    const QString cap = window->caption().toLower();
    return cls.contains(QStringLiteral("liquid_glass_gtk")) ||
           cls.contains(QStringLiteral("liquid-glass")) ||
           cap.contains(QStringLiteral("liquid glass"));
}

void LiquidGlassNativeEffect::ensureShader()
{
    if (m_shader) {
        return;
    }

    const QString fragPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
        QStringLiteral("kwin/shaders/liquidglassnative/liquidglass.frag"));
    if (fragPath.isEmpty()) {
        return;
    }

    QFile fragFile(fragPath);
    if (!fragFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    static const QByteArray vertexSource = R"(#version 140
in vec2 position;
in vec2 texcoord;
uniform mat4 projectionMatrix;
out vec2 texcoord0;
void main()
{
    texcoord0 = texcoord;
    gl_Position = projectionMatrix * vec4(position, 0.0, 1.0);
}
)";

    m_shader = ShaderManager::instance()->loadShaderFromCode(vertexSource, fragFile.readAll());
}

void LiquidGlassNativeEffect::releaseShader()
{
    m_shader.reset();
}

void LiquidGlassNativeEffect::releaseCaptureBuffers()
{
    m_captureBuffer.texture.reset();
    m_captureBuffer.framebuffer.reset();
    m_captureBuffer.size = QSize();
}

LiquidGlassNativeEffect::CaptureBuffer &LiquidGlassNativeEffect::captureBufferFor(const QSize &size)
{
    if (!m_captureBuffer.texture || m_captureBuffer.size != size) {
        m_captureBuffer.framebuffer.reset();
        m_captureBuffer.texture = GLTexture::allocate(GL_RGBA8, size);
        if (!m_captureBuffer.texture) {
            m_captureBuffer.size = QSize();
            return m_captureBuffer;
        }

        m_captureBuffer.texture->setFilter(GL_LINEAR);
        m_captureBuffer.texture->setWrapMode(GL_CLAMP_TO_EDGE);
        m_captureBuffer.framebuffer = std::make_unique<GLFramebuffer>(m_captureBuffer.texture.get());
        m_captureBuffer.size = size;
    }
    return m_captureBuffer;
}

void LiquidGlassNativeEffect::drawBackdropLayer(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window)
{
    if (!m_shader) {
        return;
    }

    if (!renderTarget.texture()) {
        return;
    }

    const QRect windowRect = window->frameGeometry().toAlignedRect();
    if (windowRect.isEmpty()) {
        return;
    }

    constexpr int backdropPad = 48;
    const QRect captureRect = windowRect.adjusted(-backdropPad, -backdropPad, backdropPad, backdropPad);
    const QRect captureTexRect = viewport.mapToRenderTargetTexture(captureRect);
    if (captureTexRect.isEmpty()) {
        return;
    }

    CaptureBuffer &buffer = captureBufferFor(captureTexRect.size());
    if (!buffer.texture || !buffer.framebuffer) {
        return;
    }

    buffer.framebuffer->blitFromRenderTarget(
        renderTarget,
        viewport,
        captureRect,
        QRect(QPoint(0, 0), buffer.size));

    const QRectF targetRect = viewport.mapToRenderTarget(windowRect);
    const QRectF sourceTexRect = viewport.mapToRenderTargetTexture(windowRect);
    const QPointF captureOffset = captureTexRect.topLeft();
    const QPointF windowOffset = sourceTexRect.topLeft() - captureOffset;

    const float invW = buffer.size.width() > 0 ? 1.0f / float(buffer.size.width()) : 1.0f;
    const float invH = buffer.size.height() > 0 ? 1.0f / float(buffer.size.height()) : 1.0f;

    auto uv = [&](qreal x, qreal y) {
        return QVector2D(float((windowOffset.x() + x) * invW), float((windowOffset.y() + y) * invH));
    };

    const std::array<GLVertex2D, 6> vertices = {
        GLVertex2D{QVector2D(float(targetRect.left()), float(targetRect.top())), uv(0, 0)},
        GLVertex2D{QVector2D(float(targetRect.left() + targetRect.width()), float(targetRect.top())), uv(sourceTexRect.width(), 0)},
        GLVertex2D{QVector2D(float(targetRect.left() + targetRect.width()), float(targetRect.top() + targetRect.height())), uv(sourceTexRect.width(), sourceTexRect.height())},
        GLVertex2D{QVector2D(float(targetRect.left()), float(targetRect.top())), uv(0, 0)},
        GLVertex2D{QVector2D(float(targetRect.left() + targetRect.width()), float(targetRect.top() + targetRect.height())), uv(sourceTexRect.width(), sourceTexRect.height())},
        GLVertex2D{QVector2D(float(targetRect.left()), float(targetRect.top() + targetRect.height())), uv(0, sourceTexRect.height())},
    };

    ShaderBinder binder(m_shader.get());
    m_shader->setUniform("projectionMatrix", viewport.projectionMatrix());
    m_shader->setUniform("u_backdrop", 0);
    m_shader->setUniform("u_texture_size", QVector2D(buffer.size.width(), buffer.size.height()));
    m_shader->setUniform("u_window_size", QVector2D(float(targetRect.width()), float(targetRect.height())));
    m_shader->setUniform("u_time", GLfloat((QDateTime::currentMSecsSinceEpoch() - m_windowStart.value(window)) / 1000.0));
    m_shader->setUniform("u_opacity", GLfloat(0.82));
    m_shader->setUniform("u_light_dir", QVector2D(-0.65, -0.35));

    glActiveTexture(GL_TEXTURE0);
    buffer.texture->bind();

    GLVertexBuffer vbo(GLVertexBuffer::Stream);
    vbo.setVertices(vertices);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    vbo.render(GL_TRIANGLES);
}

void LiquidGlassNativeEffect::prePaintWindow(EffectWindow *window, WindowPrePaintData &data,
    std::chrono::milliseconds presentTime)
{
    if (isLiquidGlassWindow(window)) {
        data.setTranslucent();
        window->setData(KWin::WindowForceBlurRole, true);
        window->setData(KWin::WindowForceBackgroundContrastRole, true);
    } else {
        window->setData(KWin::WindowForceBlurRole, QVariant());
        window->setData(KWin::WindowForceBackgroundContrastRole, QVariant());
    }
    effects->prePaintWindow(window, data, presentTime);
}

void LiquidGlassNativeEffect::paintWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *window, int mask, QRegion region, WindowPaintData &data)
{
    if (!isLiquidGlassWindow(window) || !effects->isOpenGLCompositing()) {
        effects->paintWindow(renderTarget, viewport, window, mask, region, data);
        return;
    }

    ensureShader();
    if (!m_shader) {
        effects->paintWindow(renderTarget, viewport, window, mask, region, data);
        return;
    }

    if (!m_windowStart.contains(window)) {
        m_windowStart.insert(window, QDateTime::currentMSecsSinceEpoch());
    }

    drawBackdropLayer(renderTarget, viewport, window);
    effects->paintWindow(renderTarget, viewport, window, mask, region, data);
}

void LiquidGlassNativeEffect::postPaintWindow(EffectWindow *window)
{
    if (window->isDeleted()) {
        m_windowStart.remove(window);
    }
    effects->postPaintWindow(window);
}

}

KWIN_EFFECT_FACTORY_SUPPORTED(KWin::LiquidGlassNativeEffect,
                              "package/liquidglassnative.json",
                              return KWin::LiquidGlassNativeEffect::supported();)
