// Copyright 2021 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef BENCHMARKS_WINDOW_HPP_
#define BENCHMARKS_WINDOW_HPP_

#include "VulkanHeaders.hpp"

#include <string>

#if defined(_WIN32)
#	define WIN32_LEAN_AND_MEAN
#	include <Windows.h>
#elif defined(VK_USE_PLATFORM_XCB_KHR)
#	include <xcb/xcb.h>
#endif

#if USE_HEADLESS_SURFACE
class Window
{
public:
	Window(vk::Instance instance, vk::Extent2D windowSize);
	~Window();
	vk::SurfaceKHR getSurface();
	void show();
	void pumpEvents();
	void setTitle(const std::string &title);

private:
	const vk::Instance instance;
	vk::SurfaceKHR surface;
};

#elif defined(_WIN32)

class Window
{
public:
	Window(vk::Instance instance, vk::Extent2D windowSize);
	~Window();
	vk::SurfaceKHR getSurface();
	void show();
	void pumpEvents();
	void setTitle(const std::string &title);

private:
	HWND window;
	HINSTANCE moduleInstance;
	WNDCLASSEX windowClass;
	const vk::Instance instance;
	vk::SurfaceKHR surface;
};

#elif defined(VK_USE_PLATFORM_XCB_KHR)

class Window
{
public:
	Window(vk::Instance instance, vk::Extent2D windowSize);
	~Window();
	vk::SurfaceKHR getSurface();
	void show();
	void pumpEvents();
	void setTitle(const std::string &title);

private:
	xcb_connection_t *connection = nullptr;
	xcb_screen_t *screen = nullptr;
	xcb_window_t window = 0;
	const vk::Instance instance;
	vk::SurfaceKHR surface;
};

#else
#	error Window class unimplemented for this platform
#endif

#endif  // BENCHMARKS_WINDOW_HPP_
