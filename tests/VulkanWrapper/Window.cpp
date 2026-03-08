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

#include "Window.hpp"

#include <cassert>
#include <cstdlib>

#if USE_HEADLESS_SURFACE

Window::Window(vk::Instance instance, vk::Extent2D windowSize)
    : instance(instance)
{
	vk::HeadlessSurfaceCreateInfoEXT surfaceCreateInfo;
	surface = instance.createHeadlessSurfaceEXT(surfaceCreateInfo);
	assert(surface);
}

Window::~Window()
{
	instance.destroySurfaceKHR(surface, nullptr);
}

vk::SurfaceKHR Window::getSurface()
{
	return surface;
}

void Window::show()
{
}

void Window::pumpEvents()
{
}

void Window::setTitle(const std::string &)
{
}

#elif defined(_WIN32)

Window::Window(vk::Instance instance, vk::Extent2D windowSize)
    : instance(instance)
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = DefWindowProc;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = moduleInstance;
	windowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	windowClass.lpszMenuName = NULL;
	windowClass.lpszClassName = "Window";
	windowClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

	RegisterClassEx(&windowClass);

	DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	DWORD extendedStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;

	RECT windowRect;
	windowRect.left = 0L;
	windowRect.top = 0L;
	windowRect.right = (long)windowSize.width;
	windowRect.bottom = (long)windowSize.height;

	AdjustWindowRectEx(&windowRect, style, FALSE, extendedStyle);
	uint32_t x = (GetSystemMetrics(SM_CXSCREEN) - windowRect.right) / 2;
	uint32_t y = (GetSystemMetrics(SM_CYSCREEN) - windowRect.bottom) / 2;

	window = CreateWindowEx(extendedStyle, "Window", "Hello",
	                        style | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
	                        x, y,
	                        windowRect.right - windowRect.left,
	                        windowRect.bottom - windowRect.top,
	                        NULL, NULL, moduleInstance, NULL);

	SetForegroundWindow(window);
	SetFocus(window);

	// Create the Vulkan surface
	vk::Win32SurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.hinstance = moduleInstance;
	surfaceCreateInfo.hwnd = window;
	surface = instance.createWin32SurfaceKHR(surfaceCreateInfo);
	assert(surface);
}

Window::~Window()
{
	instance.destroySurfaceKHR(surface, nullptr);
	DestroyWindow(window);
	UnregisterClass("Window", moduleInstance);
}

vk::SurfaceKHR Window::getSurface()
{
	return surface;
}

void Window::show()
{
	ShowWindow(window, SW_SHOW);
}

void Window::pumpEvents()
{
	MSG msg;
	while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void Window::setTitle(const std::string &title)
{
	SetWindowTextA(window, title.c_str());
}

#elif defined(VK_USE_PLATFORM_XCB_KHR)

namespace {

xcb_screen_t *getDefaultScreen(xcb_connection_t *connection)
{
	const xcb_setup_t *setup = xcb_get_setup(connection);
	xcb_screen_iterator_t iterator = xcb_setup_roots_iterator(setup);
	return iterator.data;
}

}  // namespace

Window::Window(vk::Instance instance, vk::Extent2D windowSize)
    : instance(instance)
{
	connection = xcb_connect(nullptr, nullptr);
	assert(connection);
	assert(xcb_connection_has_error(connection) == 0);

	screen = getDefaultScreen(connection);
	assert(screen);

	window = xcb_generate_id(connection);
	uint32_t valueMask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	uint32_t valueList[] = {
		screen->black_pixel,
		XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_KEY_PRESS,
	};

	xcb_create_window(connection,
	                  XCB_COPY_FROM_PARENT,
	                  window,
	                  screen->root,
	                  0,
	                  0,
	                  static_cast<uint16_t>(windowSize.width),
	                  static_cast<uint16_t>(windowSize.height),
	                  0,
	                  XCB_WINDOW_CLASS_INPUT_OUTPUT,
	                  screen->root_visual,
	                  valueMask,
	                  valueList);
	setTitle("SwiftShader Benchmark");
	xcb_flush(connection);

	vk::XcbSurfaceCreateInfoKHR surfaceCreateInfo;
	surfaceCreateInfo.connection = connection;
	surfaceCreateInfo.window = window;
	surface = instance.createXcbSurfaceKHR(surfaceCreateInfo);
	assert(surface);
}

Window::~Window()
{
	instance.destroySurfaceKHR(surface, nullptr);
	if(connection && window != 0)
	{
		xcb_destroy_window(connection, window);
		xcb_flush(connection);
	}
	if(connection)
	{
		xcb_disconnect(connection);
	}
}

vk::SurfaceKHR Window::getSurface()
{
	return surface;
}

void Window::show()
{
	xcb_map_window(connection, window);
	xcb_flush(connection);
}

void Window::pumpEvents()
{
	while(xcb_generic_event_t *event = xcb_poll_for_event(connection))
	{
		free(event);
	}
	xcb_flush(connection);
}

void Window::setTitle(const std::string &title)
{
	xcb_change_property(connection,
	                    XCB_PROP_MODE_REPLACE,
	                    window,
	                    XCB_ATOM_WM_NAME,
	                    XCB_ATOM_STRING,
	                    8,
	                    static_cast<uint32_t>(title.size()),
	                    title.c_str());
	xcb_flush(connection);
}

#else
#	error Window class unimplemented for this platform
#endif
