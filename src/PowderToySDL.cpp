#include "Config.h"
#include "common/tpt-minmax.h"

#include <algorithm>
#include <map>
#include <optional>
#include <ctime>
#include <climits>
#include <cstdint>
#ifdef WIN
#include <direct.h>
#endif
#include "SDLCompat.h"

#ifdef X86_SSE
#include <xmmintrin.h>
#endif
#ifdef X86_SSE3
#include <pmmintrin.h>
#endif

#include <iostream>
#if defined(LIN)
# include "icon_exe.png.h"
#endif
#include <csignal>
#include <stdexcept>

#ifndef WIN
# include <unistd.h>
#endif
#ifdef MACOSX
# ifdef DEBUG
#  undef DEBUG
#  define DEBUG 1
# endif
# include <CoreServices/CoreServices.h>
#endif
#include <sys/stat.h>
#ifdef __EMSCRIPTEN__
# include <emscripten.h>
#endif

#include "Format.h"
#include "Misc.h"

#include "client/Client.h"
#include "client/GameSave.h"
#include "client/SaveFile.h"
#include "client/SaveInfo.h"
#include "common/Platform.h"
#include "graphics/Graphics.h"
#include "gui/Style.h"

#include "gui/game/GameController.h"
#include "gui/game/GameModel.h"
#include "gui/game/Menu.h"
#include "gui/game/Tool.h"
#include "graphics/Renderer.h"
#include "simulation/Simulation.h"
#include "common/tpt-rand.h"
#include "simulation/SaveRenderer.h"
#include "simulation/SimulationData.h"
#include "gui/game/GameView.h"
#include "gui/dialogues/ConfirmPrompt.h"
#include "gui/dialogues/ErrorMessage.h"
#include "gui/interface/Engine.h"
#include "gui/interface/Keys.h"

#include "SDLCompat.h"

int desktopWidth = 1280, desktopHeight = 1024;

SDL_Window * sdl_window;
SDL_Renderer * sdl_renderer;
SDL_Texture * sdl_texture;
int scale = 1;
bool fullscreen = false;
bool altFullscreen = false;
bool forceIntegerScaling = true;
bool resizable = false;
bool momentumScroll = true;
bool showAvatars = true;

void StartTextInput()
{
	SDL_StartTextInput();
}

void StopTextInput()
{
	SDL_StopTextInput();
}

void SetTextInputRect(int x, int y, int w, int h)
{
	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;
	SDL_SetTextInputRect(&rect);
}

void ClipboardPush(ByteString text)
{
	SDL_SetClipboardText(text.c_str());
}

ByteString ClipboardPull()
{
	return ByteString(SDL_GetClipboardText());
}

int GetModifiers()
{
	return SDL_GetModState();
}

void LoadWindowPosition()
{
#ifdef __EMSCRIPTEN__
	return; // the canvas has no window position or borders (SDL_GetWindowBordersSize is unsupported)
#endif
	int savedWindowX = Client::Ref().GetPrefInteger("WindowX", INT_MAX);
	int savedWindowY = Client::Ref().GetPrefInteger("WindowY", INT_MAX);

	int borderTop, borderLeft;
	SDL_GetWindowBordersSize(sdl_window, &borderTop, &borderLeft, nullptr, nullptr);
	// Sometimes (Windows), the border size may not be reported for 200+ frames
	// So just have a default of 5 to ensure the window doesn't get stuck where it can't be moved
	if (borderTop == 0)
		borderTop = 5;

	int numDisplays = SDL_GetNumVideoDisplays();
	SDL_Rect displayBounds;
	bool ok = false;
	for (int i = 0; i < numDisplays; i++)
	{
		SDL_GetDisplayBounds(i, &displayBounds);
		if (savedWindowX + borderTop > displayBounds.x && savedWindowY + borderLeft > displayBounds.y &&
				savedWindowX + borderTop < displayBounds.x + displayBounds.w &&
				savedWindowY + borderLeft < displayBounds.y + displayBounds.h)
		{
			ok = true;
			break;
		}
	}
	if (ok)
		SDL_SetWindowPosition(sdl_window, savedWindowX + borderLeft, savedWindowY + borderTop);
}

void SaveWindowPosition()
{
#ifdef __EMSCRIPTEN__
	return;
#endif
	int x, y;
	SDL_GetWindowPosition(sdl_window, &x, &y);

	int borderTop, borderLeft;
	SDL_GetWindowBordersSize(sdl_window, &borderTop, &borderLeft, nullptr, nullptr);

	Client::Ref().SetPref("WindowX", x - borderLeft);
	Client::Ref().SetPref("WindowY", y - borderTop);
}

void CalculateMousePosition(int *x, int *y)
{
	int globalMx, globalMy;
	SDL_GetGlobalMouseState(&globalMx, &globalMy);
	int windowX, windowY;
	SDL_GetWindowPosition(sdl_window, &windowX, &windowY);

	if (x)
		*x = (globalMx - windowX) / scale;
	if (y)
		*y = (globalMy - windowY) / scale;
}

#ifdef __EMSCRIPTEN__
// Set by the page while it records the canvas (tpt_set_frame_callback): the WebGL canvas can only be read
// back in the same task that drew it, so the page copies each frame from Module.tptOnFrame.
static bool webFrameCallback = false;
#endif

void blit(pixel * vid)
{
	SDL_UpdateTexture(sdl_texture, NULL, vid, WINDOWW * sizeof (Uint32));
	// need to clear the renderer if there are black edges (fullscreen, or resizable window)
	if (fullscreen || resizable)
		SDL_RenderClear(sdl_renderer);
	SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
	SDL_RenderPresent(sdl_renderer);
#ifdef __EMSCRIPTEN__
	if (webFrameCallback)
	{
		EM_ASM({
			if (Module['tptOnFrame'])
				Module['tptOnFrame']();
		});
	}
#endif
}

bool RecreateWindow();
void SDLOpen()
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		fprintf(stderr, "Initializing SDL (video subsystem): %s\n", SDL_GetError());
		exit(-1);
	}

	if (!RecreateWindow())
	{
		fprintf(stderr, "Creating SDL window: %s\n", SDL_GetError());
		exit(-1);
	}

	int displayIndex = SDL_GetWindowDisplayIndex(sdl_window);
	if (displayIndex >= 0)
	{
		SDL_Rect rect;
		if (!SDL_GetDisplayUsableBounds(displayIndex, &rect))
		{
			desktopWidth = rect.w;
			desktopHeight = rect.h;
		}
	}

#ifdef LIN
	std::vector<pixel> imageData;
	int imgw, imgh;
	if (PngDataToPixels(imageData, imgw, imgh, reinterpret_cast<const char *>(icon_exe_png), icon_exe_png_size, false))
	{
		SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(&imageData[0], imgw, imgh, 32, imgw * sizeof(pixel), 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
		SDL_SetWindowIcon(sdl_window, icon);
		SDL_FreeSurface(icon);
	}
#endif
}

void SDLSetScreen(int scale_, bool resizable_, bool fullscreen_, bool altFullscreen_, bool forceIntegerScaling_)
{
//	bool changingScale = scale != scale_;
	bool changingFullscreen = fullscreen_ != fullscreen || (altFullscreen_ != altFullscreen && fullscreen);
	bool changingResizable = resizable != resizable_;
	scale = scale_;
	fullscreen = fullscreen_;
	altFullscreen = altFullscreen_;
	resizable = resizable_;
	forceIntegerScaling = forceIntegerScaling_;
	// Recreate the window when toggling fullscreen, due to occasional issues
	// Also recreate it when enabling resizable windows, to fix bugs on windows,
	//  see https://github.com/jacob1/The-Powder-Toy/issues/24
	if (changingFullscreen || altFullscreen || (changingResizable && resizable && !fullscreen))
	{
		RecreateWindow();
		return;
	}
	if (changingResizable)
		SDL_RestoreWindow(sdl_window);

	SDL_SetWindowSize(sdl_window, WINDOWW * scale, WINDOWH * scale);
	SDL_RenderSetIntegerScale(sdl_renderer, forceIntegerScaling && fullscreen ? SDL_TRUE : SDL_FALSE);
	unsigned int flags = 0;
	if (fullscreen)
		flags = altFullscreen ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP;
	SDL_SetWindowFullscreen(sdl_window, flags);
	if (fullscreen)
		SDL_RaiseWindow(sdl_window);
	SDL_SetWindowResizable(sdl_window, resizable ? SDL_TRUE : SDL_FALSE);
}

bool RecreateWindow()
{
	unsigned int flags = 0;
	if (fullscreen)
		flags = altFullscreen ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP;
	if (resizable && !fullscreen)
		flags |= SDL_WINDOW_RESIZABLE;

	if (sdl_texture)
		SDL_DestroyTexture(sdl_texture);
	if (sdl_renderer)
		SDL_DestroyRenderer(sdl_renderer);
	if (sdl_window)
	{
		SaveWindowPosition();
		SDL_DestroyWindow(sdl_window);
	}

	sdl_window = SDL_CreateWindow(APPNAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOWW * scale, WINDOWH * scale,
	                              flags);
	if (!sdl_window)
	{
		return false;
	}
	sdl_renderer = SDL_CreateRenderer(sdl_window, -1, 0);
	if (!sdl_renderer)
	{
		fprintf(stderr, "SDL_CreateRenderer failed; available renderers:\n");
		int num = SDL_GetNumRenderDrivers();
		for (int i = 0; i < num; ++i)
		{
			SDL_RendererInfo info;
			SDL_GetRenderDriverInfo(i, &info);
			fprintf(stderr, " - %s\n", info.name);
		}
		return false;
	}
	SDL_RenderSetLogicalSize(sdl_renderer, WINDOWW, WINDOWH);
	if (forceIntegerScaling && fullscreen)
		SDL_RenderSetIntegerScale(sdl_renderer, SDL_TRUE);
	sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WINDOWW, WINDOWH);
	SDL_RaiseWindow(sdl_window);
	//Uncomment this to enable resizing
	//SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	//SDL_SetWindowResizable(sdl_window, SDL_TRUE);

	if (!Client::Ref().IsFirstRun())
		LoadWindowPosition();

	return true;
}

unsigned int GetTicks()
{
	return SDL_GetTicks();
}

int elapsedTime = 0, currentTime = 0, lastTime = 0, currentFrame = 0;
uint64_t lastTick = 0;
uint64_t lastFpsUpdate = 0;
float fps = 0;
ui::Engine * engine = NULL;
bool showLargeScreenDialog = false;
float currentWidth, currentHeight;

int mousex = 0, mousey = 0;
int mouseButton = 0;
bool mouseDown = false;

bool calculatedInitialMouse = false, delay = false;
bool hasMouseMoved = false;

#ifdef __EMSCRIPTEN__
// The web page's element browser (see the exports at the end of this file) selects tools through a
// queued SDL event, so the game state only changes inside the game loop.
static GameController *webGameController = nullptr;
static Uint32 webSelectToolEvent = (Uint32)-1;
static Uint32 webCommandEvent = (Uint32)-1;
static void WebSelectTool(const char *identifier, int selection);
#endif

void EventProcess(SDL_Event event)
{
#ifdef __EMSCRIPTEN__
	if (webSelectToolEvent != (Uint32)-1 && event.type == webSelectToolEvent)
	{
		WebSelectTool((const char *)event.user.data1, event.user.code);
		SDL_free(event.user.data1);
		return;
	}
	if (webCommandEvent != (Uint32)-1 && event.type == webCommandEvent)
	{
		if (webGameController)
			webGameController->RunCommand(ByteString((const char *)event.user.data1).FromUtf8());
		SDL_free(event.user.data1);
		return;
	}
#endif
	switch (event.type)
	{
	case SDL_QUIT:
		if (engine->GetFastQuit() || engine->CloseWindow())
			engine->Exit();
		break;
	case SDL_KEYDOWN:
		if (SDL_GetModState() & KMOD_GUI)
		{
			break;
		}
		if (!event.key.repeat && event.key.keysym.sym == 'q' && (event.key.keysym.mod&KMOD_CTRL))
			engine->ConfirmExit();
		else
			engine->onKeyPress(event.key.keysym.sym, event.key.keysym.scancode, event.key.repeat, event.key.keysym.mod&KMOD_SHIFT, event.key.keysym.mod&KMOD_CTRL, event.key.keysym.mod&KMOD_ALT);
		break;
	case SDL_KEYUP:
		if (SDL_GetModState() & KMOD_GUI)
		{
			break;
		}
		engine->onKeyRelease(event.key.keysym.sym, event.key.keysym.scancode, event.key.repeat, event.key.keysym.mod&KMOD_SHIFT, event.key.keysym.mod&KMOD_CTRL, event.key.keysym.mod&KMOD_ALT);
		break;
	case SDL_TEXTINPUT:
		if (SDL_GetModState() & KMOD_GUI)
		{
			break;
		}
		engine->onTextInput(ByteString(event.text.text).FromUtf8());
		break;
	case SDL_TEXTEDITING:
		if (SDL_GetModState() & KMOD_GUI)
		{
			break;
		}
		engine->onTextEditing(ByteString(event.edit.text).FromUtf8(), event.edit.start);
		break;
	case SDL_MOUSEWHEEL:
	{
		// int x = event.wheel.x;
		int y = event.wheel.y;
		if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
		{
			// x *= -1;
			y *= -1;
		}

		engine->onMouseWheel(mousex, mousey, y); // TODO: pass x?
		break;
	}
	case SDL_MOUSEMOTION:
		mousex = event.motion.x;
		mousey = event.motion.y;
		engine->onMouseMove(mousex, mousey);

		hasMouseMoved = true;
		break;
	case SDL_DROPFILE:
		engine->onFileDrop(event.drop.file);
		SDL_free(event.drop.file);
		break;
	case SDL_MOUSEBUTTONDOWN:
		// if mouse hasn't moved yet, sdl will send 0,0. We don't want that
		if (hasMouseMoved)
		{
			mousex = event.button.x;
			mousey = event.button.y;
		}
		mouseButton = event.button.button;
		engine->onMouseClick(mousex, mousey, mouseButton);

		mouseDown = true;
#if !defined(NDEBUG) && !defined(DEBUG)
		SDL_CaptureMouse(SDL_TRUE);
#endif
		break;
	case SDL_MOUSEBUTTONUP:
		// if mouse hasn't moved yet, sdl will send 0,0. We don't want that
		if (hasMouseMoved)
		{
			mousex = event.button.x;
			mousey = event.button.y;
		}
		mouseButton = event.button.button;
		engine->onMouseUnclick(mousex, mousey, mouseButton);

		mouseDown = false;
#if !defined(NDEBUG) && !defined(DEBUG)
		SDL_CaptureMouse(SDL_FALSE);
#endif
		break;
	case SDL_WINDOWEVENT:
	{
		switch (event.window.event)
		{
		case SDL_WINDOWEVENT_SHOWN:
			if (!calculatedInitialMouse)
			{
				//initial mouse coords, sdl won't tell us this if mouse hasn't moved
				CalculateMousePosition(&mousex, &mousey);
				engine->initialMouse(mousex, mousey);
				engine->onMouseMove(mousex, mousey);
				calculatedInitialMouse = true;
			}
			break;
		// This event would be needed in certain glitchy cases of window resizing
		// But for all currently tested cases, it isn't needed
		/*case SDL_WINDOWEVENT_RESIZED:
		{
			float width = event.window.data1;
			float height = event.window.data2;

			currentWidth = width;
			currentHeight = height;
			// this "* scale" thing doesn't really work properly
			// currently there is a bug where input doesn't scale properly after resizing, only when double scale mode is active
			inputScaleH = (float)WINDOWW * scale / currentWidth;
			inputScaleV = (float)WINDOWH * scale / currentHeight;
			std::cout << "Changing input scale to " << inputScaleH << "x" << inputScaleV << std::endl;
			break;
		}*/
		// This would send a mouse up event when focus is lost
		// Not even sdl itself will know when the mouse was released if it happens in another window
		// So it will ignore the next mouse down (after tpt is re-focused) and not send any events at all
		// This is more unintuitive than pretending the mouse is still down when it's not, so this code is commented out
		/*case SDL_WINDOWEVENT_FOCUS_LOST:
			if (mouseDown)
			{
				mouseDown = false;
				engine->onMouseUnclick(mousex, mousey, mouseButton);
			}
			break;*/
		}
		break;
	}
	}
}

void LargeScreenDialog()
{
	StringBuilder message;
	message << "Switching to " << scale << "x size mode since your screen was determined to be large enough: ";
	message << desktopWidth << "x" << desktopHeight << " detected, " << WINDOWW*scale << "x" << WINDOWH*scale << " required";
	message << "\nTo undo this, hit Cancel. You can change this in settings at any time.";
	if (!ConfirmPrompt::Blocking("Large screen detected", message.Build()))
	{
		Client::Ref().SetPref("Scale", 1);
		engine->SetScale(1);
	}
}

#ifdef __EMSCRIPTEN__
// Yields to the browser (input, drawing, the page's own code) and resumes as soon as it is idle. SDL_Delay(0)
// resumes from a timer, and browsers make back-to-back timers wait at least 4 ms; with no speed limit that
// wait, not the simulation, decided how many ticks ran per second. A message on a MessageChannel has no
// such minimum. With JSPI the whole C++ stack is suspended while the promise is pending.
EM_ASYNC_JS(void, web_yield, (), {
	if (!Module['tptYield'])
	{
		const channel = new MessageChannel();
		const waiting = [];
		channel.port1.onmessage = () => waiting.shift()();
		Module['tptYield'] = () => new Promise((resolve) => {
			waiting.push(resolve);
			channel.port2.postMessage(0);
		});
	}
	await Module['tptYield']();
});
#endif

void EngineProcess()
{
	double correctedFrameTimeAvg = 0;
	SDL_Event event;

	uint64_t drawingTimer = 0;
	auto frameStart = uint64_t(SDL_GetTicks()) * UINT64_C(1'000'000);

	while(engine->Running())
	{
		if(engine->Broken()) { engine->UnBreak(); break; }
		event.type = 0;
		while (SDL_PollEvent(&event))
		{
			EventProcess(event);
			event.type = 0; //Clear last event
		}
		if(engine->Broken()) { engine->UnBreak(); break; }

		engine->Tick();

		int drawcap = ui::Engine::Ref().GetDrawingFrequencyLimit();
		if (!drawcap || drawingTimer > 1e9f / drawcap)
		{
			engine->Draw();
			drawingTimer = 0;

			if (scale != engine->Scale || fullscreen != engine->Fullscreen ||
					altFullscreen != engine->GetAltFullscreen() ||
					forceIntegerScaling != engine->GetForceIntegerScaling() || resizable != engine->GetResizable())
			{
				SDLSetScreen(engine->Scale, engine->GetResizable(), engine->Fullscreen, engine->GetAltFullscreen(),
							 engine->GetForceIntegerScaling());
			}

			blit(engine->g->vid);
#ifdef __EMSCRIPTEN__
			// Tell the page once that the game is on screen and taking input (SDL's event listeners
			// exist from here on); the module's ready promise resolves before main() gets this far.
			static bool firstFrameReported = false;
			if (!firstFrameReported)
			{
				firstFrameReported = true;
				EM_ASM({
					if (Module['tptOnFirstFrame'])
						Module['tptOnFirstFrame']();
				});
			}
#endif
		}
		auto fpsLimit = ui::Engine::Ref().FpsLimit;
		auto now = uint64_t(SDL_GetTicks()) * UINT64_C(1'000'000);
		auto oldFrameStart = frameStart;
		frameStart = now;
		if (fpsLimit > 2)
		{
			auto timeBlockDuration = uint64_t(UINT64_C(1'000'000'000) / fpsLimit);
			auto oldFrameStartTimeBlock = oldFrameStart / timeBlockDuration;
			auto frameStartTimeBlock = oldFrameStartTimeBlock + 1U;
			frameStart = std::max(frameStart, frameStartTimeBlock * timeBlockDuration);
#ifdef __EMSCRIPTEN__
			// Timers can't wait less than about 4 ms (see web_yield), which held every cap above 60 at about
			// 60 ticks per second. With a short frame (cap above about 83), sleep on a timer only while plenty
			// of time is left, then yield until the frame's start time. Longer frames (the default 60) sleep
			// on the timer as before: yielding repeatedly keeps the CPU busy.
			if (timeBlockDuration >= UINT64_C(12'000'000))
				SDL_Delay((frameStart - now) / UINT64_C(1'000'000));
			else for (;;)
			{
				auto left = int64_t(frameStart) - int64_t(uint64_t(SDL_GetTicks()) * UINT64_C(1'000'000));
				if (left <= 0)
					break;
				if (left > INT64_C(6'000'000))
					SDL_Delay(Uint32((left - INT64_C(5'000'000)) / INT64_C(1'000'000)));
				else
					web_yield();
			}
#else
			SDL_Delay((frameStart - now) / UINT64_C(1'000'000));
#endif
		}
#ifdef __EMSCRIPTEN__
		else
		{
			// No frame limit: still yield to the browser once per frame (with JSPI this suspends the whole
			// C++ stack, nested modal loops included, and resumes it from the event loop)
			web_yield();
		}
#endif
		auto correctedFrameTime = frameStart - oldFrameStart;
		drawingTimer += correctedFrameTime;
		correctedFrameTimeAvg = correctedFrameTimeAvg + (correctedFrameTime - correctedFrameTimeAvg) * 0.05;
		if (frameStart - lastFpsUpdate > UINT64_C(200'000'000))
		{
			engine->SetFps(1e9f / correctedFrameTimeAvg);
			lastFpsUpdate = frameStart;
		}
		if (frameStart - lastTick > UINT64_C(100'000'000))
		{
			lastTick = frameStart;
			Client::Ref().Tick();
		}
		if (showLargeScreenDialog)
		{
			showLargeScreenDialog = false;
			LargeScreenDialog();
		}
	}
#ifdef DEBUG
	std::cout << "Breaking out of EngineProcess" << std::endl;
#endif
}

void BlueScreen(String detailMessage)
{
	ui::Engine * engine = &ui::Engine::Ref();
	engine->g->fillrect(0, 0, engine->GetWidth(), engine->GetHeight(), 17, 114, 169, 210);

	String errorTitle = "ERROR";
	String errorDetails = "Details: " + detailMessage;
	String errorHelp = "An unrecoverable fault has occurred, please report the error by visiting the website below\n"
		SCHEME SERVER;
	int currentY = 0, width, height;
	int errorWidth = 0;
	Graphics::textsize(errorHelp, errorWidth, height);

	engine->g->drawtext((engine->GetWidth()/2)-(errorWidth/2), ((engine->GetHeight()/2)-100) + currentY, errorTitle.c_str(), 255, 255, 255, 255);
	Graphics::textsize(errorTitle, width, height);
	currentY += height + 4;

	engine->g->drawtext((engine->GetWidth()/2)-(errorWidth/2), ((engine->GetHeight()/2)-100) + currentY, errorDetails.c_str(), 255, 255, 255, 255);
	Graphics::textsize(errorTitle, width, height);
	currentY += height + 4;

	engine->g->drawtext((engine->GetWidth()/2)-(errorWidth/2), ((engine->GetHeight()/2)-100) + currentY, errorHelp.c_str(), 255, 255, 255, 255);
	Graphics::textsize(errorTitle, width, height);
	currentY += height + 4;

	//Death loop
	SDL_Event event;
	while(true)
	{
		while (SDL_PollEvent(&event))
			if(event.type == SDL_QUIT)
				exit(-1);
		blit(engine->g->vid);
#ifdef __EMSCRIPTEN__
		SDL_Delay(100); // keep the page responsive while showing the error
#endif
	}
}

void SigHandler(int signal)
{
	switch(signal){
	case SIGSEGV:
		BlueScreen("Memory read/write error");
		break;
	case SIGFPE:
		BlueScreen("Floating point exception");
		break;
	case SIGILL:
		BlueScreen("Program execution exception");
		break;
	case SIGABRT:
		BlueScreen("Unexpected program abort");
		break;
	}
}

constexpr int SCALE_MAXIMUM = 10;
constexpr int SCALE_MARGIN = 30;

int GuessBestScale()
{
	const int widthNoMargin = desktopWidth - SCALE_MARGIN;
	const int widthGuess = widthNoMargin / WINDOWW;

	const int heightNoMargin = desktopHeight - SCALE_MARGIN;
	const int heightGuess = heightNoMargin / WINDOWH;

	int guess = std::min(widthGuess, heightGuess);
	if(guess < 1 || guess > SCALE_MAXIMUM)
		guess = 1;

	return guess;
}

#ifdef main
# undef main // thank you sdl
#endif

int main(int argc, char * argv[])
{
#if defined(DEBUG) && defined(_MSC_VER)
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
#endif
	currentWidth = WINDOWW;
	currentHeight = WINDOWH;


	// https://bugzilla.libsdl.org/show_bug.cgi?id=3796
	if (SDL_Init(0) < 0)
	{
		fprintf(stderr, "Initializing SDL: %s\n", SDL_GetError());
		return 1;
	}

	Platform::originalCwd = Platform::GetCwd();

	using Argument = std::optional<ByteString>;
	std::map<ByteString, Argument> arguments;

	for (auto i = 1; i < argc; ++i)
	{
		auto str = ByteString(argv[i]);
		if (str.BeginsWith("file://"))
		{
			arguments.insert({ "open", format::URLDecode(str.substr(7 /* length of the "file://" prefix */)) });
		}
		else if (str.BeginsWith("ptsave:"))
		{
			arguments.insert({ "ptsave", str });
		}
		else if (auto split = str.SplitBy(':'))
		{
			arguments.insert({ split.Before(), split.After() });
		}
		else if (auto split = str.SplitBy('='))
		{
			arguments.insert({ split.Before(), split.After() });
		}
		else if (str == "open" || str == "ptsave" || str == "ddir")
		{
			if (i + 1 < argc)
			{
				arguments.insert({ str, argv[i + 1] });
				i += 1;
			}
			else
			{
				std::cerr << "no value provided for command line parameter " << str << std::endl;
			}
		}
		else
		{
			arguments.insert({ str, "" }); // so .has_value() is true
		}
	}

	auto ddirArg = arguments["ddir"];
	if (ddirArg.has_value())
	{
#ifdef WIN
		int failure = _chdir(ddirArg.value().c_str());
#else
		int failure = chdir(ddirArg.value().c_str());
#endif
		if (!failure)
			Platform::sharedCwd = Platform::GetCwd();
		else
			perror("failed to chdir to requested ddir");
	}
	else
	{
		char *ddir = SDL_GetPrefPath(NULL, APPDATA);
#ifdef WIN
		struct _stat s;
		if (_stat("powder.pref", &s) != 0)
#else
		struct stat s;
		if (stat("powder.pref", &s) != 0)
#endif
		{
			if (ddir)
			{
#ifdef WIN
				int failure = _chdir(ddir);
#else
				int failure = chdir(ddir);
#endif
				if (failure)
				{
					perror("failed to chdir to default ddir");
					SDL_free(ddir);
					ddir = nullptr;
				}
			}
		}

		if (ddir)
		{
			Platform::sharedCwd = ddir;
			SDL_free(ddir);
		}
	}

	scale = Client::Ref().GetPrefInteger("Scale", 1);
	resizable = Client::Ref().GetPrefBool("Resizable", false);
	fullscreen = Client::Ref().GetPrefBool("Fullscreen", false);
	altFullscreen = Client::Ref().GetPrefBool("AltFullscreen", false);
	forceIntegerScaling = Client::Ref().GetPrefBool("ForceIntegerScaling", true);
	momentumScroll = Client::Ref().GetPrefBool("MomentumScroll", true);
	showAvatars = Client::Ref().GetPrefBool("ShowAvatars", true);

	auto true_string = [](ByteString str) {
		str = str.ToLower();
		return str == "true" ||
		       str == "t" ||
		       str == "on" ||
		       str == "yes" ||
		       str == "y" ||
		       str == ""; // standalone "redirect" or "disable-bluescreen" or similar arguments
	};
	auto true_arg = [&true_string](Argument arg) {
		return arg.has_value() && true_string(arg.value());
	};

	auto kioskArg = arguments["kiosk"];
	if (kioskArg.has_value())
	{
		fullscreen = true_string(kioskArg.value());
		Client::Ref().SetPref("Fullscreen", fullscreen);
	}

	if (true_arg(arguments["redirect"]))
	{
		FILE *new_stdout = freopen("stdout.log", "w", stdout);
		FILE *new_stderr = freopen("stderr.log", "w", stderr);
		if (!new_stdout || !new_stderr)
		{
			exit(42);
		}
	}

	auto scaleArg = arguments["scale"];
	if (scaleArg.has_value())
	{
		try
		{
			scale = scaleArg.value().ToNumber<int>();
			Client::Ref().SetPref("Scale", scale);
		}
		catch (const std::runtime_error &e)
		{
			std::cerr << "failed to set scale: " << e.what() << std::endl;
		}
	}

	auto clientConfig = [](Argument arg, ByteString name, ByteString defaultValue) {
		ByteString value;
		if (arg.has_value())
		{
			value = arg.value();
			if (value == "")
			{
				value = defaultValue;
			}
			Client::Ref().SetPref(name, value);
		}
		else
		{
			value = Client::Ref().GetPrefByteString(name, defaultValue);
		}
		return value;
	};
	ByteString proxyString = clientConfig(arguments["proxy"], "Proxy", "");
	ByteString cafileString = clientConfig(arguments["cafile"], "CAFile", "");
	ByteString capathString = clientConfig(arguments["capath"], "CAPath", "");

	bool disableNetwork = true_arg(arguments["disable-network"]);

	Client::Ref().Initialise(proxyString, cafileString, capathString, disableNetwork);

	// TODO: maybe bind the maximum allowed scale to screen size somehow
	if(scale < 1 || scale > SCALE_MAXIMUM)
		scale = 1;

	SDLOpen();

#ifdef __EMSCRIPTEN__
	// The page scales the canvas with CSS to fit the browser window, so don't pick a larger internal
	// scale on first run (that would also open a blocking "Large screen detected" prompt at startup).
	if (false)
#else
	if (Client::Ref().IsFirstRun())
#endif
	{
		scale = GuessBestScale();
		if (scale > 1)
		{
			Client::Ref().SetPref("Scale", scale);
			SDL_SetWindowSize(sdl_window, WINDOWW * scale, WINDOWH * scale);
			showLargeScreenDialog = true;
		}
	}

	StopTextInput();

	ui::Engine::Ref().g = new Graphics();
	ui::Engine::Ref().Scale = scale;
	ui::Engine::Ref().SetResizable(resizable);
	ui::Engine::Ref().Fullscreen = fullscreen;
	ui::Engine::Ref().SetAltFullscreen(altFullscreen);
	ui::Engine::Ref().SetForceIntegerScaling(forceIntegerScaling);
	ui::Engine::Ref().MomentumScroll = momentumScroll;
	ui::Engine::Ref().ShowAvatars = showAvatars;

	engine = &ui::Engine::Ref();
	engine->SetMaxSize(desktopWidth, desktopHeight);
	engine->Begin(WINDOWW, WINDOWH);
	engine->SetFastQuit(Client::Ref().GetPrefBool("FastQuit", true));
	// Simulation speed and drawing caps from the Simulation options (0 = no limit / draw after every tick)
	int speedCap = Client::Ref().GetPrefInteger("SimulationSpeedCap", 60);
	engine->FpsLimit = speedCap > 2 ? float(speedCap) : 2.0f;
	engine->SetDrawingFrequencyLimit(Client::Ref().GetPrefInteger("DrawingCap", 0));

#if !defined(DEBUG)
	bool enableBluescreen = !true_arg(arguments["disable-bluescreen"]);
	if (enableBluescreen)
	{
		//Get ready to catch any dodgy errors
		signal(SIGSEGV, SigHandler);
		signal(SIGFPE, SigHandler);
		signal(SIGILL, SigHandler);
		signal(SIGABRT, SigHandler);
	}
#endif

#ifdef X86_SSE
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif
#ifdef X86_SSE3
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif

	GameController * gameController = NULL;

	auto wrapWithBluescreen = [&]() {
		gameController = new GameController();
		engine->ShowWindow(gameController->GetView());
#ifdef __EMSCRIPTEN__
		webGameController = gameController;
		webSelectToolEvent = SDL_RegisterEvents(1);
		webCommandEvent = SDL_RegisterEvents(1);
#endif

		auto openArg = arguments["open"];
		if (openArg.has_value())
		{
#ifdef DEBUG
			std::cout << "Loading " << openArg.value() << std::endl;
#endif
			if (Platform::FileExists(openArg.value()))
			{
				try
				{
					std::vector<char> gameSaveData;
					if (!Platform::ReadFile(gameSaveData, openArg.value()))
					{
						new ErrorMessage("Error", "Could not read file");
					}
					else
					{
						SaveFile * newFile = new SaveFile(openArg.value());
						GameSave * newSave = new GameSave(std::move(gameSaveData));
						newFile->SetGameSave(newSave);
						gameController->LoadSaveFile(newFile);
						delete newFile;
					}

				}
				catch (std::exception & e)
				{
					new ErrorMessage("Error", "Could not open save file:\n" + ByteString(e.what()).FromUtf8()) ;
				}
			}
			else
			{
				new ErrorMessage("Error", "Could not open file");
			}
		}

		auto ptsaveArg = arguments["ptsave"];
		if (ptsaveArg.has_value())
		{
			engine->g->fillrect((engine->GetWidth()/2)-101, (engine->GetHeight()/2)-26, 202, 52, 0, 0, 0, 210);
			engine->g->drawrect((engine->GetWidth()/2)-100, (engine->GetHeight()/2)-25, 200, 50, 255, 255, 255, 180);
			engine->g->drawtext((engine->GetWidth()/2)-(Graphics::textwidth("Loading save...")/2), (engine->GetHeight()/2)-5, "Loading save...", style::Colour::InformationTitle.Red, style::Colour::InformationTitle.Green, style::Colour::InformationTitle.Blue, 255);

			blit(engine->g->vid);
			try
			{
				ByteString saveIdPart;
				if (ByteString::Split split = ptsaveArg.value().SplitBy(':'))
				{
					if (split.Before() != "ptsave")
						throw std::runtime_error("Not a ptsave link");
					saveIdPart = split.After().SplitBy('#').Before();
				}
				else
					throw std::runtime_error("Invalid save link");

				if (!saveIdPart.size())
					throw std::runtime_error("No Save ID");
#ifdef DEBUG
				std::cout << "Got Ptsave: id: " << saveIdPart << std::endl;
#endif
				int saveId = saveIdPart.ToNumber<int>();

				SaveInfo * newSave = Client::Ref().GetSave(saveId, 0);
				if (!newSave)
					throw std::runtime_error("Could not load save info");
				auto saveData = Client::Ref().GetSaveData(saveId, 0);
				if (!saveData.size())
					throw std::runtime_error(("Could not load save\n" + Client::Ref().GetLastError()).ToUtf8());
				GameSave * newGameSave = new GameSave(std::move(saveData));
				newSave->SetGameSave(newGameSave);

				gameController->LoadSave(newSave);
				delete newSave;
			}
			catch (std::exception & e)
			{
				new ErrorMessage("Error", ByteString(e.what()).FromUtf8());
			}
		}

		EngineProcess();
		SaveWindowPosition();
	};

#if !defined(DEBUG)
	if (enableBluescreen)
	{
		try
		{
			wrapWithBluescreen();
		}
		catch (const std::exception &e)
		{
			BlueScreen(ByteString(e.what()).FromUtf8());
		}
	}
	else
#endif
	wrapWithBluescreen(); // the else branch of the if in the #if !defined(DEBUG)

	ui::Engine::Ref().CloseWindow();
	delete gameController;
	delete ui::Engine::Ref().g;
	Client::Ref().Shutdown();
	if (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_OPENGL)
	{
		// * nvidia-460 egl registers callbacks with x11 that end up being called
		//   after egl is unloaded unless we grab it here and release it after
		//   sdl closes the display. this is an nvidia driver weirdness but
		//   technically an sdl bug. glfw has this fixed:
		//   https://github.com/glfw/glfw/commit/9e6c0c747be838d1f3dc38c2924a47a42416c081
		SDL_GL_LoadLibrary(NULL);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		SDL_GL_UnloadLibrary();
	}
	SDL_Quit();
#ifdef __EMSCRIPTEN__
	EM_ASM({
		if (Module['tptOnQuit'])
			Module['tptOnQuit']();
	});
#endif
	return 0;
}

#ifdef __EMSCRIPTEN__
extern "C"
{
	// Called by the web page after it has written a file into the virtual filesystem, e.g. a save
	// imported with the file picker or dropped onto the page. Queued as an SDL_DROPFILE event, the
	// same path dropping a file onto the desktop game's window takes.
	EMSCRIPTEN_KEEPALIVE void tpt_open_file(const char *path)
	{
		SDL_Event event;
		SDL_zero(event);
		event.type = SDL_DROPFILE;
		event.drop.file = SDL_strdup(path);
		if (SDL_PushEvent(&event) != 1)
		{
			SDL_free(event.drop.file);
		}
	}

	// Every visible menu (except Favorites) and its tools, as JSON for the page's element browser:
	// {"menus":[{"id":1,"name":"Electronics","tools":[{"id":"DEFAULT_PT_METL","name":"METL","desc":"…","color":"#rrggbb"},…]},…]}
	// The returned string stays valid until the next call.
	EMSCRIPTEN_KEEPALIVE const char *tpt_element_list()
	{
		static std::string json;
		json = "{\"menus\":[";
		if (webGameController)
		{
			auto escape = [](const String &text) {
				std::string out;
				for (char c : text.ToUtf8())
				{
					switch (c)
					{
					case '"': out += "\\\""; break;
					case '\\': out += "\\\\"; break;
					case '\n': out += "\\n"; break;
					default:
						if ((unsigned char)c < 0x20)
						{
							char buf[8];
							snprintf(buf, sizeof(buf), "\\u%04x", c);
							out += buf;
						}
						else
						{
							out += c;
						}
					}
				}
				return out;
			};
			bool firstMenu = true;
			auto menus = webGameController->GetMenuList();
			for (int i = 0; i < (int)menus.size(); i++)
			{
				if (!menus[i]->GetVisible() || i == SC_FAVORITES || menus[i]->GetToolList().empty())
					continue;
				json += firstMenu ? "" : ",";
				firstMenu = false;
				json += "{\"id\":" + std::to_string(i) + ",\"name\":\"" + escape(menus[i]->GetDescription()) + "\",\"tools\":[";
				bool firstTool = true;
				for (auto *tool : menus[i]->GetToolList())
				{
					char colour[8];
					snprintf(colour, sizeof(colour), "#%02x%02x%02x", tool->colRed & 0xFF, tool->colGreen & 0xFF, tool->colBlue & 0xFF);
					json += firstTool ? "" : ",";
					firstTool = false;
					json += "{\"id\":\"" + escape(tool->GetIdentifier().FromUtf8()) + "\",\"name\":\"" + escape(tool->GetName()) +
						"\",\"desc\":\"" + escape(tool->GetDescription()) + "\",\"color\":\"" + colour + "\"}";
				}
				json += "]}";
			}
		}
		json += "]}";
		return json.c_str();
	}

	// Turns the per-frame Module.tptOnFrame() call (see blit) on or off.
	EMSCRIPTEN_KEEPALIVE void tpt_set_frame_callback(int enabled)
	{
		webFrameCallback = enabled != 0;
	}

	// Identifiers of the left and right mouse tools, separated by a newline.
	EMSCRIPTEN_KEEPALIVE const char *tpt_active_tools()
	{
		static std::string out;
		out.clear();
		if (webGameController)
		{
			for (int i = 0; i < 2; i++)
			{
				auto *tool = webGameController->GetActiveTool(i);
				out += (i ? "\n" : "") + (tool ? tool->GetIdentifier() : ByteString());
			}
		}
		return out.c_str();
	}

	// Selects a tool for the left (0) or right (1) mouse button, as clicking it in its menu would.
	EMSCRIPTEN_KEEPALIVE void tpt_select_tool(const char *identifier, int selection)
	{
		if (webSelectToolEvent == (Uint32)-1 || selection < 0 || selection > 1)
			return;
		SDL_Event event;
		SDL_zero(event);
		event.type = webSelectToolEvent;
		event.user.code = selection;
		event.user.data1 = SDL_strdup(identifier);
		if (SDL_PushEvent(&event) != 1)
		{
			SDL_free(event.user.data1);
		}
	}

	// Runs Lua code in the game, as if typed into the console, at the next turn of the game loop (the page's
	// challenge panel uses it to talk to challenges.lua).
	EMSCRIPTEN_KEEPALIVE void tpt_lua_command(const char *code)
	{
		if (webCommandEvent == (Uint32)-1)
			return;
		SDL_Event event;
		SDL_zero(event);
		event.type = webCommandEvent;
		event.user.data1 = SDL_strdup(code);
		if (SDL_PushEvent(&event) != 1)
		{
			SDL_free(event.user.data1);
		}
	}

	// Renders the save or stamp file at path into a PNG of at most width x height pixels at outPath, keeping
	// its aspect ratio; for the page's "My saves" gallery. Drawn like the game's own local save browser does
	// (SaveButton.cpp): decorations on, fire off (the fire pass draws the particles 15 more times, twice the
	// cost). Returns 1 on success, 0 if the file is missing or not a save.
	EMSCRIPTEN_KEEPALIVE int tpt_save_thumbnail(const char *path, const char *outPath, int width, int height)
	{
		std::vector<char> data;
		if (!Platform::ReadFile(data, path))
			return 0;
		try
		{
			GameSave save(data);
			std::unique_ptr<VideoBuffer> thumb(SaveRenderer::Ref().Render(&save, true, false));
			if (!thumb)
				return 0;
			thumb->Resize(width, height, true, true);
			return thumb->WritePNG(outPath) ? 1 : 0;
		}
		catch (const std::exception &e)
		{
			std::cerr << "tpt_save_thumbnail: " << path << ": " << e.what() << std::endl;
			return 0;
		}
	}

	// Deletes the stamp with the given name (file name without ".stm") through the game's stamp list, so the
	// in-game stamp browser stays in step; also removes a stamp file the list doesn't know (e.g. an imported one).
	EMSCRIPTEN_KEEPALIVE void tpt_delete_stamp(const char *name)
	{
		Client::Ref().DeleteStamp(name);
		remove(ByteString::Build(STAMPS_DIR, PATH_SEP, name, ".stm").c_str());
	}

	// Runs the simulation (what = 0) or the particle renderer (what = 1) for the given number of frames
	// as fast as possible and returns the milliseconds taken; for benchmarks
	// (tests/browser/perf.spec.mjs). Ignores pause, like single-frame stepping.
	EMSCRIPTEN_KEEPALIVE double tpt_benchmark(int frames, int what)
	{
		if (!webGameController)
			return -1;
		auto *sim = webGameController->GetModel()->GetSimulation();
		auto *ren = webGameController->GetModel()->GetRenderer();
		auto start = emscripten_get_now();
		for (int i = 0; i < frames; i++)
		{
			if (what == 0)
			{
				sim->framerender = 1;
				sim->BeforeSim();
				sim->UpdateParticles(0, NPART - 1);
				sim->AfterSim();
			}
			else
			{
				ren->clearScreen(1.0f);
				ren->RenderBegin();
				ren->RenderEnd();
			}
		}
		sim->framerender = 0;
		return emscripten_get_now() - start;
	}

	// Seeds the simulation's random number generators (RNG::Ref() and random_gen, both seeded from the clock
	// at start) and resets the frame counters that depend on how long the game has been running: the sand
	// colour cycle (advances on every drawn frame, even while paused, and tints some created particles), the
	// timer (time dilation skips frames by it) and the tick count (element recounts). A scene stepped with
	// tpt_benchmark right after this replays exactly; for checking that a speed-up leaves the results
	// unchanged (docs/handoff/tools/samecheck.mjs).
	EMSCRIPTEN_KEEPALIVE void tpt_rng_seed(unsigned int seed)
	{
		RNG::Ref().seed(seed);
		random_gen.seed(seed);
		if (webGameController)
		{
			auto *sim = webGameController->GetModel()->GetSimulation();
			sim->sandcolour = 0;
			sim->sandcolour_frame = 0;
			sim->timer = 0;
			sim->currentTick = 0;
		}
	}
}

static void WebSelectTool(const char *identifier, int selection)
{
	auto *tool = webGameController->GetModel()->GetToolFromIdentifier(identifier);
	if (!tool)
		return;
	auto menus = webGameController->GetMenuList();
	for (int i = 0; i < (int)menus.size(); i++)
	{
		auto tools = menus[i]->GetToolList();
		if (i != SC_FAVORITES && menus[i]->GetVisible() && std::find(tools.begin(), tools.end(), tool) != tools.end())
		{
			webGameController->SetActiveMenu(i);
			break;
		}
	}
	webGameController->SetActiveTool(selection, tool);
}
#endif
