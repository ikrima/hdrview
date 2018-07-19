//
// Copyright (C) Wojciech Jarosz <wjarosz@gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE.txt file.
//
// This file is adapted from tev:
// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the BSD 3-Clause License within the LICENSE file.

#pragma once

#include "common.h"

#include <nanogui/widget.h>

#include <string>

class ImageButton : public nanogui::Widget
{
public:
	ImageButton(nanogui::Widget *parent, const std::string &caption);

	Eigen::Vector2i preferredSize(NVGcontext *ctx) const override;
	bool mouseButtonEvent(const Eigen::Vector2i &p, int button, bool down, int modifiers) override;
	void draw(NVGcontext *ctx) override;

	float progress()                        { return m_progress; }
	void setProgress(float progress)        { m_progress = progress; }

	/// Set the button's text caption/filename
	void setCaption(const std::string &caption) { m_caption = caption; recomputeStringClipping(); }
	const std::string & caption() const     { return m_caption; }
	void setId(size_t id)                   { m_id = id; }
	void setIsModified(bool b)              { m_isModified = b; }
	bool isModified() const                 { return m_isModified; }
	bool isSelected() const                 { return m_isSelected; }
	void setIsSelected(bool isSelected)     { m_isSelected = isSelected; }
	bool isReference() const                { return m_isReference; }
	void setIsReference(bool isReference)   { m_isReference = isReference; }

	void recomputeStringClipping();

	void setSelectedCallback(const std::function<void(int)> & callback)
	{
		m_selectedCallback = callback;
	}

	void setReferenceCallback(const std::function<void(int)> & callback)
	{
		m_referenceCallback = callback;
	}

private:
	std::string m_caption;

	bool m_isModified = false;
	bool m_isSelected = false;
	bool m_isReference = false;
	bool m_canBeReference;
	std::function<void(int)> m_selectedCallback;
	std::function<void(int)> m_referenceCallback;

	size_t m_id = 0;
	size_t m_cutoff = 0;
	Eigen::Vector2i m_sizeForWhichCutoffWasComputed = Eigen::Vector2i::Constant(0);

	float m_progress = -1.f;

public:
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};
