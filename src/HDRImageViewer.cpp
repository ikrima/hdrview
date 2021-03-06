//
// Copyright (C) Wojciech Jarosz <wjarosz@gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE.txt file.
//

#include "HDRImageViewer.h"
#include "HDRViewer.h"
#include <tinydir.h>
#include <utility>
using namespace std;

namespace
{
const float MIN_ZOOM = 0.01f;

const float MAX_ZOOM = 512.f;
}

HDRImageViewer::HDRImageViewer(Widget* parent, HDRViewScreen* screen)
	: Widget(parent), m_screen(screen), m_zoom(1.f / m_screen->pixelRatio()), m_offset(Vector2f::Zero()),
	  m_exposureCallback(std::function<void(float)>()), m_gammaCallback(std::function<void(float)>()),
	  m_sRGBCallback(std::function<void(bool)>()), m_zoomCallback(std::function<void(float)>()),
	  m_pixelHoverCallback(std::function<void(const Vector2i&, const Color4&, const Color4&)>())
{

}

Vector2f HDRImageViewer::screenSizeF() const
{
	return m_screen->size().cast<float>();
}

Vector2f HDRImageViewer::imageCoordinateAt(const Vector2f& position) const
{
	auto imagePosition = position - (m_offset + centerOffset(m_currentImage));
	return imagePosition / m_zoom;
}

Vector2f HDRImageViewer::clampedImageCoordinateAt(const Vector2f& position) const
{
	auto imageCoordinate = imageCoordinateAt(position);
	return imageCoordinate.cwiseMax(Vector2f::Zero()).cwiseMin(imageSizeF(m_currentImage));
}

Vector2f HDRImageViewer::positionForCoordinate(const Vector2f& imageCoordinate) const
{
	return m_zoom * imageCoordinate + (m_offset + centerOffset(m_currentImage));
}

Vector2f HDRImageViewer::screenPositionForCoordinate(const Vector2f& imageCoordinate) const
{
	return positionForCoordinate(imageCoordinate) + positionF();
}

void HDRImageViewer::setImageCoordinateAt(const Vector2f& position, const Vector2f& imageCoordinate)
{
	// Calculate where the new offset must be in order to satisfy the image position equation.
	// Round the floating point values to balance out the floating point to integer conversions.
	m_offset = position - (imageCoordinate * m_zoom);

	// Clamp offset so that the image remains near the screen.
	m_offset = m_offset.cwiseMin(sizeF()).cwiseMax(-scaledImageSizeF(m_currentImage));

	m_offset -= centerOffset(m_currentImage);
}

void HDRImageViewer::center()
{
	m_offset = Vector2f(0.f, 0.f);
}

void HDRImageViewer::fit()
{
	// Calculate the appropriate scaling factor.
	m_zoom = (sizeF().cwiseQuotient(imageSizeF(m_currentImage))).minCoeff();
	center();
	m_zoomCallback(m_zoom);
}

void HDRImageViewer::moveOffset(const Vector2f& delta)
{
	// Apply the delta to the offset.
	m_offset += delta;

	// Prevent the image from going out of bounds.
	auto scaledSize = scaledImageSizeF(m_currentImage);
	if (m_offset.x() + scaledSize.x() < 0)
		m_offset.x() = -scaledSize.x();
	if (m_offset.x() > sizeF().x())
		m_offset.x() = sizeF().x();
	if (m_offset.y() + scaledSize.y() < 0)
		m_offset.y() = -scaledSize.y();
	if (m_offset.y() > sizeF().y())
		m_offset.y() = sizeF().y();
}

void HDRImageViewer::setZoomLevel(float level)
{
	m_zoom = clamp(std::pow(m_zoomSensitivity, level), MIN_ZOOM, MAX_ZOOM);
	m_zoomLevel = log(m_zoom) / log(m_zoomSensitivity);
	m_zoomCallback(m_zoom);
}

void HDRImageViewer::zoomBy(float amount, const Vector2f& focusPosition)
{
	auto focusedCoordinate = imageCoordinateAt(focusPosition);
	float scaleFactor = std::pow(m_zoomSensitivity, amount);
	m_zoom = clamp(scaleFactor * m_zoom, MIN_ZOOM, MAX_ZOOM);
	m_zoomLevel = log(m_zoom) / log(m_zoomSensitivity);
	setImageCoordinateAt(focusPosition, focusedCoordinate);
	m_zoomCallback(m_zoom);
}

void HDRImageViewer::zoomIn()
{
	// keep position at center of window fixed while zooming
	auto centerPosition = Vector2f(sizeF() / 2.f);
	auto centerCoordinate = imageCoordinateAt(centerPosition);

	// determine next higher power of 2 zoom level
	float levelForPow2Sensitivity = ceil(log(m_zoom) / log(2.f) + 0.5f);
	float newScale = std::pow(2.f, levelForPow2Sensitivity);
	m_zoom = clamp(newScale, MIN_ZOOM, MAX_ZOOM);
	m_zoomLevel = log(m_zoom) / log(m_zoomSensitivity);
	setImageCoordinateAt(centerPosition, centerCoordinate);
	m_zoomCallback(m_zoom);
}

void HDRImageViewer::zoomOut()
{
	// keep position at center of window fixed while zooming
	auto centerPosition = Vector2f(sizeF() / 2.f);
	auto centerCoordinate = imageCoordinateAt(centerPosition);

	// determine next lower power of 2 zoom level
	float levelForPow2Sensitivity = floor(log(m_zoom) / log(2.f) - 0.5f);
	float newScale = std::pow(2.f, levelForPow2Sensitivity);
	m_zoom = clamp(newScale, MIN_ZOOM, MAX_ZOOM);
	m_zoomLevel = log(m_zoom) / log(m_zoomSensitivity);
	setImageCoordinateAt(centerPosition, centerCoordinate);
	m_zoomCallback(m_zoom);
}

bool HDRImageViewer::mouseDragEvent(const Vector2i& p, const Vector2i& rel, int button, int /*modifiers*/)
{
	if (button & (1 << GLFW_MOUSE_BUTTON_LEFT))
	{
		setImageCoordinateAt((p + rel).cast<float>(), imageCoordinateAt(p.cast<float>()));
		return true;
	}
	return false;
}

bool HDRImageViewer::mouseMotionEvent(const Vector2i& p, const Vector2i& rel, int button, int modifiers)
{
	if (Widget::mouseMotionEvent(p, rel, button, modifiers))
		return true;

	if (!m_currentImage)
		return false;

	Vector2i pixel = imageCoordinateAt((p - mPos).cast<float>()).cast<int>();
	Color4 pixelVal(0.f);
	Color4 iPixelVal(0.f);
	if (m_currentImage->contains(pixel))
	{
		pixelVal = m_currentImage->image()(pixel.x(), pixel.y());
		iPixelVal = (pixelVal * pow(2.f, m_exposure) * 255).min(255.f).max(0.f);
	}

	m_pixelHoverCallback(pixel, pixelVal, iPixelVal);

	return false;
}

bool HDRImageViewer::scrollEvent(const Vector2i& p, const Vector2f& rel)
{
	if (Widget::scrollEvent(p, rel))
		return true;

	// query glfw directly to check if a modifier key is pressed
	int lState = glfwGetKey(m_screen->glfwWindow(), GLFW_KEY_LEFT_SHIFT);
	int rState = glfwGetKey(m_screen->glfwWindow(), GLFW_KEY_RIGHT_SHIFT);

	if (lState == GLFW_PRESS || rState == GLFW_PRESS)
	{
		// panning
		setImageCoordinateAt(p.cast<float>() + rel * 4.f, imageCoordinateAt(p.cast<float>()));
		return true;
	}
	else if (m_screen->modifiers() == 0)
	{
		// zooming
		float v = rel.y();
		if (std::abs(v) < 1)
			v = std::copysign(1.f, v);
		zoomBy(v / 4.f, (p - position()).cast<float>());

		return true;

	}

	return false;
}

bool HDRImageViewer::gridVisible() const
{
	return m_drawGrid && (m_gridThreshold != -1) && (m_zoom > m_gridThreshold);
}

bool HDRImageViewer::pixelInfoVisible() const
{
	return m_drawValues && (m_pixelInfoThreshold != -1) && (m_zoom > m_pixelInfoThreshold);
}

bool HDRImageViewer::helpersVisible() const
{
	return gridVisible() || pixelInfoVisible();
}

Vector2f HDRImageViewer::centerOffset(ConstImagePtr img) const
{
	return (sizeF() - scaledImageSizeF(std::move(img))) / 2;
}

void HDRImageViewer::draw(NVGcontext* ctx)
{
	Widget::draw(ctx);
	nvgEndFrame(ctx); // Flush the NanoVG draw stack, not necessary to call nvgBeginFrame afterwards.

	Vector2f screenSize = m_screen->size().cast<float>();
	Vector2f positionInScreen = absolutePosition().cast<float>();

	glEnable(GL_SCISSOR_TEST);
	float r = m_screen->pixelRatio();
	glScissor(positionInScreen.x() * r, (screenSize.y() - positionInScreen.y() - size().y()) * r, size().x() * r,
			  size().y() * r);

	glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	if (m_currentImage && !m_currentImage->isNull())
	{
		Vector2f pCurrent, sCurrent;
		imagePositionAndScale(pCurrent, sCurrent, m_currentImage);

		if (m_referenceImage)
		{
			Vector2f pReference, sReference;
			imagePositionAndScale(pReference, sReference, m_referenceImage);
			m_shader
				.draw(m_currentImage->glTextureId(), m_referenceImage->glTextureId(), sCurrent, pCurrent, sReference,
					  pReference, powf(2.0f, m_exposure), m_gamma, m_sRGB, m_dither, m_channel, m_blendMode);
		}
		else
		{
			m_shader.draw(m_currentImage->glTextureId(), sCurrent, pCurrent, powf(2.0f, m_exposure), m_gamma, m_sRGB,
						  m_dither, m_channel, m_blendMode);
		}

		drawImageBorder(ctx);

		if (helpersVisible())
			drawHelpers(ctx);
	}

	glDisable(GL_SCISSOR_TEST);

	drawWidgetBorder(ctx);
}

void HDRImageViewer::imagePositionAndScale(Vector2f& position, Vector2f& scale, shared_ptr<const GLImage> image)
{
	scale = scaledImageSizeF(image).cwiseQuotient(screenSizeF());
	position = (absolutePosition().cast<float>() + m_offset + centerOffset(image)).cwiseQuotient(screenSizeF());
}

void HDRImageViewer::drawWidgetBorder(NVGcontext* ctx) const
{
	// Draw an inner drop shadow. (adapted from nanogui::Window) and tev
	int ds = mTheme->mWindowDropShadowSize, cr = mTheme->mWindowCornerRadius;
	NVGpaint shadowPaint =
		nvgBoxGradient(ctx, mPos.x(), mPos.y(), mSize.x(), mSize.y(), cr * 2, ds * 2, mTheme->mTransparent,
					   mTheme->mDropShadow);

	nvgSave(ctx);
	nvgResetScissor(ctx);
	nvgBeginPath(ctx);
	nvgRect(ctx, mPos.x(), mPos.y(), mSize.x(), mSize.y());
	nvgRoundedRect(ctx, mPos.x() + ds, mPos.y() + ds, mSize.x() - 2 * ds, mSize.y() - 2 * ds, cr);
	nvgPathWinding(ctx, NVG_HOLE);
	nvgFillPaint(ctx, shadowPaint);
	nvgFill(ctx);
	nvgRestore(ctx);
}

void HDRImageViewer::drawImageBorder(NVGcontext* ctx) const
{
	int ds = mTheme->mWindowDropShadowSize, cr = mTheme->mWindowCornerRadius;

	Vector2i borderPosition = mPos + (m_offset + centerOffset(m_currentImage)).cast<int>();
	Vector2i borderSize = scaledImageSizeF(m_currentImage).cast<int>();

	if (m_referenceImage)
	{
		borderPosition = borderPosition.cwiseMin(mPos + (m_offset + centerOffset(m_referenceImage)).cast<int>());
		borderSize = borderSize.cwiseMax(scaledImageSizeF(m_referenceImage).cast<int>());
	}

	// Draw a drop shadow
	NVGpaint shadowPaint =
		nvgBoxGradient(ctx, borderPosition.x(), borderPosition.y(), borderSize.x(), borderSize.y(), cr * 2, ds * 2,
					   mTheme->mDropShadow, mTheme->mTransparent);

	nvgSave(ctx);
	nvgBeginPath(ctx);
	nvgScissor(ctx, mPos.x(), mPos.y(), mSize.x(), mSize.y());
	nvgRect(ctx, borderPosition.x() - ds, borderPosition.y() - ds, borderSize.x() + 2 * ds, borderSize.y() + 2 * ds);
	nvgRoundedRect(ctx, borderPosition.x(), borderPosition.y(), borderSize.x(), borderSize.y(), cr);
	nvgPathWinding(ctx, NVG_HOLE);
	nvgFillPaint(ctx, shadowPaint);
	nvgFill(ctx);
	nvgRestore(ctx);

	// draw a line border
	nvgSave(ctx);
	nvgBeginPath(ctx);
	nvgScissor(ctx, mPos.x(), mPos.y(), mSize.x(), mSize.y());
	nvgStrokeWidth(ctx, 2.0f);
	nvgRect(ctx, borderPosition.x() - 0.5f, borderPosition.y() - 0.5f, borderSize.x() + 1, borderSize.y() + 1);
	nvgStrokeColor(ctx, Color(0.5f, 0.5f, 0.5f, 1.0f));
	nvgStroke(ctx);
	nvgResetScissor(ctx);
	nvgRestore(ctx);
}

void HDRImageViewer::drawHelpers(NVGcontext* ctx) const
{
	if (gridVisible())
		drawPixelGrid(ctx);
	if (pixelInfoVisible())
		drawPixelInfo(ctx);
}

void HDRImageViewer::drawPixelGrid(NVGcontext* ctx) const
{
	Vector2f xy0 = screenPositionForCoordinate(Vector2f::Zero());
	int minJ = max(0, int(-xy0.y() / m_zoom));
	int maxJ = min(m_currentImage->height(), int(ceil((m_screen->size().y() - xy0.y()) / m_zoom)));
	int minI = max(0, int(-xy0.x() / m_zoom));
	int maxI = min(m_currentImage->width(), int(ceil((m_screen->size().x() - xy0.x()) / m_zoom)));

	nvgBeginPath(ctx);

	// draw vertical lines
	for (int i = minI; i <= maxI; ++i)
	{
		Vector2f sxy0 = screenPositionForCoordinate(Vector2f(i, minJ));
		Vector2f sxy1 = screenPositionForCoordinate(Vector2f(i, maxJ));
		nvgMoveTo(ctx, sxy0.x(), sxy0.y());
		nvgLineTo(ctx, sxy1.x(), sxy1.y());
	}

	// draw horizontal lines
	for (int j = minJ; j <= maxJ; ++j)
	{
		Vector2f sxy0 = screenPositionForCoordinate(Vector2f(minI, j));
		Vector2f sxy1 = screenPositionForCoordinate(Vector2f(maxI, j));
		nvgMoveTo(ctx, sxy0.x(), sxy0.y());
		nvgLineTo(ctx, sxy1.x(), sxy1.y());
	}

	nvgStrokeWidth(ctx, 2.0f);
	float factor = clamp01((m_zoom - m_gridThreshold)/(2*m_gridThreshold));
	float alpha = lerp(0.0f, 0.2f, smoothStep(0.0f, 1.0f, factor));
	nvgStrokeColor(ctx, Color(1.0f, 1.0f, 1.0f, alpha));
	nvgStroke(ctx);
}

void HDRImageViewer::drawPixelInfo(NVGcontext* ctx) const
{
	Vector2f xy0 = screenPositionForCoordinate(Vector2f::Zero());
	int minJ = max(0, int(-xy0.y() / m_zoom));
	int maxJ = min(m_currentImage->height() - 1, int(ceil((m_screen->size().y() - xy0.y()) / m_zoom)));
	int minI = max(0, int(-xy0.x() / m_zoom));
	int maxI = min(m_currentImage->width() - 1, int(ceil((m_screen->size().x() - xy0.x()) / m_zoom)));

	float factor = clamp01((m_zoom - m_pixelInfoThreshold)/(2*m_pixelInfoThreshold));
	float alpha = lerp(0.0f, 0.5f, smoothStep(0.0f, 1.0f, factor));

	for (int j = minJ; j <= maxJ; ++j)
	{
		for (int i = minI; i <= maxI; ++i)
		{
			Color4 pixel = m_currentImage->image()(i, j);
			float luminance = pixel.luminance() * pow(2.0f, m_exposure);
			string text = fmt::format("{:1.3f}\n{:1.3f}\n{:1.3f}", pixel[0], pixel[1], pixel[2]);

			auto pos = screenPositionForCoordinate(Vector2f(i, j));
			nvgFontFace(ctx, "sans");
			nvgFontSize(ctx, m_zoom / 31.0f * 10);
			nvgFillColor(ctx, luminance > 0.5f ? Color(0.0f, 0.0f, 0.0f, alpha) : Color(1.0f, 1.0f, 1.0f, alpha));
			nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			nvgTextBox(ctx, pos.x(), pos.y(), m_zoom, text.c_str(), nullptr);
		}
	}
}