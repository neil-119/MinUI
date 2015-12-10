/**
 * MinGUI
 * Released under the MIT license, (c) Neil Rao
 *
 * A minimalistic, header-only UI wrapper intended to allow
 * students to procedurally build simple user interfaces.
 */

#ifndef MINGUI_H_
#define MINGUI_H_

#include <Rocket/Core.h>
#include <Rocket/Core/Input.h>
#include <Rocket/Debugger/Debugger.h>
#include <Rocket/Controls.h>
#include "SystemInterfaceSDL2.h"
#include "RenderInterfaceSDL2.h"
#include <SDL.h>
#include <GL/glew.h>
#include <string.h>

// basic config
#define DEFAULT_FONT	"Lacuna"
#define RENDERER		RocketSDL2Renderer
#define SYSTEMINTERFACE	RocketSDL2SystemInterface

// helper defines
#define FONT_BOLD				(1 << 0)
#define FONT_ITALIC				(1 << 1)
#define FONT_BOLD_AND_ITALIC	(FONT_BOLD | FONT_ITALIC)


// engine state is global
struct enstate
{
	Rocket::Core::Context* context;
	Rocket::Core::ElementDocument* document;
	int window_width;
	int window_height;
	int clear_r;
	int clear_g;
	int clear_b;
	std::string title;
	SDL_Window* screen;
	SDL_Renderer* renderer;
	RENDERER* rrenderer;
	SYSTEMINTERFACE* rsi;
	bool exit;
} static _enstate{0};

static inline struct enstate* GetEngineState()
{
	return &_enstate;
}

// nomenclature
typedef Rocket::Core::Element* Widget;
typedef Rocket::Core::Event& WidgetEvent;

// function ptr for events
typedef int(*basic_event_ptr)(const char*, Widget, WidgetEvent);

// fnctn ptr to game loop
typedef int(*game_loop_ptr)();


#ifdef _MSC_VER
// SDL doesn't declare __iob_func (cmake)
FILE _iob[] = { *stdin, *stdout, *stderr };
extern "C" FILE* __cdecl __iob_func(void) { return _iob; }

// It is typical to disable assert() in Release, but librocket
// defines it under MSVC (librocket project bug with latest cmake, maybe?).
namespace Rocket
{
	namespace Core
	{
		bool Assert(const char* msg, const char* file, int line)
		{
			Rocket::Core::String message(1024, "%s\n%s:%d", msg, file, line);
			return GetSystemInterface()->LogMessage(Log::LT_ASSERT, message);
		}
	}
}
#endif

// for event handling
class _Listener : public Rocket::Core::EventListener
{
private:
	basic_event_ptr ev;
public:
	_Listener(basic_event_ptr e) : ev(e) {}

	void ProcessEvent(Rocket::Core::Event& event)
	{
		if (ev != NULL && !ev(event.GetType().CString(), event.GetTargetElement(), event))
			event.StopPropagation();
	}
};


/**
 * Initializes and creates the window.
 */
static SDL_Window* _create_window(const char* title, int window_width, int window_height)
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* screen = SDL_CreateWindow(title, 20, 20, window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	SDL_GLContext glcontext = SDL_GL_CreateContext(screen);
	
	GLenum err = glewInit();

	if (err != GLEW_OK)
		fprintf(stderr, "GLEW ERROR: %s\n", glewGetErrorString(err));

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	glMatrixMode(GL_PROJECTION | GL_MODELVIEW);
	glLoadIdentity();
	glOrtho(0, window_width, window_height, 0, 0, 1);
	
	return screen;
}

/**
 * Initializes and returns the renderer.
 */
static SDL_Renderer* init_renderer(SDL_Window* screen)
{
	int oglIdx = -1;
	int nRD = SDL_GetNumRenderDrivers();

	for (int i = 0; i < nRD; i++)
	{
		SDL_RendererInfo info;
		if (!SDL_GetRenderDriverInfo(i, &info))
		{
			if (!strcmp(info.name, "opengl"))
			{
				oglIdx = i;
			}
		}
	}

	return SDL_CreateRenderer(screen, oglIdx, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
}

/**
 * Creates a "window." This is really just the root node of the DOM, as the actual
 * rendering window is created elsewhere.
 */
void create_window(const char* title, int window_width = 1024, int window_height = 768)
{
	struct enstate* enstate = GetEngineState();

	enstate->screen = _create_window(title, window_width, window_height);
	enstate->renderer = init_renderer(enstate->screen);

	enstate->rrenderer = new RocketSDL2Renderer(enstate->renderer, enstate->screen);
	enstate->rsi = new RocketSDL2SystemInterface;

	Rocket::Core::SetRenderInterface(enstate->rrenderer);
	Rocket::Core::SetSystemInterface(enstate->rsi);

	if (!Rocket::Core::Initialise())
		return;

	Rocket::Controls::Initialise();

	Rocket::Core::Context* context = Rocket::Core::CreateContext("default", Rocket::Core::Vector2i(window_width, window_height));
	enstate->context = context;

	Rocket::Debugger::Initialise(context);

	enstate->window_height = window_height;
	enstate->window_width = window_width;
	enstate->title = title;

	// create the document
	enstate->document = enstate->context->CreateDocument();

	if (enstate->document != NULL)
		enstate->document->Show();
	else
		Rocket::Core::GetSystemInterface()->LogMessage(Rocket::Core::Log::Type::LT_ERROR, "Could not create the window.");
}

/**
 * Sets the background color of the window.
 */
void set_window_background_color(char background_r, char background_g, char background_b)
{
	struct enstate* enstate = GetEngineState();
	enstate->clear_r = background_r;
	enstate->clear_g = background_g;
	enstate->clear_b = background_b;
}

/**
 * Creates a text block.
 */
void load_font(const char* text)
{
	Rocket::Core::FontDatabase::LoadFontFace(text);
}

/**
 * Creates a text block.
 */
Widget create_text(const char* text)
{
	struct enstate* enstate = GetEngineState();

	Rocket::Core::Element* new_element = enstate->document->CreateElement("p");

	// default font
	new_element->SetProperty("font-family", DEFAULT_FONT);

	Rocket::Core::ElementText* new_text_element = enstate->document->CreateTextNode(text);

	new_element->AppendChild(new_text_element);
	enstate->document->AppendChild(new_element);

	return new_element;
}

/**
* Sets the font itself
*/
void set_font(Widget w, const char* name)
{
	w->SetProperty("font-family", name);
}

/**
 * Sets the text style. FONT_BOLD, FONT_ITALIC, or FONT_BOLD_AND_ITALIC.
 */
void set_text_style(Widget w, int style)
{
	w->SetProperty("font-weight", style & FONT_BOLD ? "bold" : "normal");
	w->SetProperty("font-style", style & FONT_ITALIC ? "italic" : "normal");
}

/**
* Sets the font size.
*/
void set_text_size(Widget w, int size)
{
	w->SetProperty("font-size", Rocket::Core::String(100, "%dpx", size).CString());
}

/**
* Sets the background color.
*/
void set_background_color(Widget w, const char* color)
{
	w->SetProperty("background-color", color);
}

/**
* Sets the border
*/
void set_border(Widget w, const char* color, int borderWidth)
{
	w->SetProperty("border", "solid");
	w->SetProperty("border-width", Rocket::Core::String(100, "%dpx", borderWidth).CString());
	w->SetProperty("border-color", color);
}

/**
* Sets the font color.
*/
void set_text_color(Widget w, const char* color)
{
	w->SetProperty("color", color);
}

/**
* Sets the widget's position
*/
void set_position(Widget w, int x, int y)
{
	w->SetProperty("position", "absolute");
	w->SetProperty("margin", Rocket::Core::String(100, "%dpx %dpx", y, x).CString());
}

/**
* Sets the widget's width in pixels
*/
void set_width(Widget w, int v)
{
	w->SetProperty("width", Rocket::Core::String(100, "%dpx", v).CString());
}

/**
* Sets the widget's height in pixels
*/
void set_height(Widget w, int v)
{
	w->SetProperty("height", Rocket::Core::String(100, "%dpx", v).CString());
}

/**
* Sets the widget's text to center
*/
void set_text_center(Widget w)
{
	w->SetProperty("text-align", "center");
}

/**
* Changes the text of the widget
*/
void set_text(Widget w, const char* text)
{
	if (w->GetTagName() == "input" && w->GetAttribute("type")->Get<Rocket::Core::String>() == "text")
		((Rocket::Controls::ElementFormControlInput*)w)->SetValue(text);
	else
		w->SetInnerRML(text);
}


/**
* Creates a button
*/
Widget create_button(const char* text)
{
	struct enstate* enstate = GetEngineState();
	Rocket::Core::Element* new_element = enstate->document->CreateElement("button");
	
	// default font
	new_element->SetProperty("font-family", DEFAULT_FONT);

	Rocket::Core::ElementText* new_text_element = enstate->document->CreateTextNode(text);

	new_element->AppendChild(new_text_element);
	enstate->document->AppendChild(new_element);
	return new_element;
}

/**
* Exits the game. 
*/
void exit_game()
{
	struct enstate* enstate = GetEngineState();
	enstate->exit = true;
}

/**
* Binds an event. 
*/
void bind_event(Widget w, const char* name, basic_event_ptr e)
{
	_Listener* l = new _Listener(e);
	w->AddEventListener(name, l);
}

/**
* Gets a property from an event.
*/
template<typename T>
void get_event_value(WidgetEvent e, const char* prop, T defaultValue)
{
	return e.GetParameter(prop, defaultValue);
}

/**
* Creates the desired element and optionally automatically adds it to the DOM.
*/
Widget create(const char* e, bool autoAdd = true)
{
	struct enstate* enstate = GetEngineState();
	Rocket::Core::Element* E = enstate->document->CreateElement(e);
	E->SetProperty("font-family", DEFAULT_FONT);
	if (autoAdd)
		enstate->document->AppendChild(E);
	return E;
}

/**
* Creates an unparented container
*/
Widget create_container(bool autoAttach = true)
{
	return create("div", autoAttach);
}

/**
* Parents an element to another element
*/
void attach(Widget subnode, Widget parentnode)
{
	parentnode->AppendChild(subnode);
}

/**
* Parents an element to the root node
*/
void attach(Widget subnode)
{
	GetEngineState()->document->AppendChild(subnode);
}

/**
* Creates a tab control
*/
Widget create_tabs_container()
{
	return create("tabset");
}

/**
* Adds a tab to a tab control
*/
Widget create_tab(Widget tabset, const char* title, Widget content)
{
	struct enstate* enstate = GetEngineState();

	Rocket::Controls::ElementTabSet* tab = (Rocket::Controls::ElementTabSet*)enstate->document->CreateElement("tab");
	tab->SetInnerRML(title);
	tabset->AppendChild(tab);

	Rocket::Core::Element* panel = enstate->document->CreateElement("panel");
	panel->AppendChild(content);
	tabset->AppendChild(panel);
	
	return tab;
}

/**
* Sets a widget's property
*/
inline void set_prop(Widget w, const char* prop, const char* val)
{
	w->SetProperty(prop, val);
}

/**
* Loads a document
*/
void load_document(const char* file)
{
	struct enstate* enstate = GetEngineState();
	enstate->context->UnloadAllDocuments();
	enstate->document = enstate->context->LoadDocument(file);
}

/**
* Creates a form element
*/
Widget create_form(const char* type)
{
	Widget w = create("input");
	w->SetAttribute("type", type);
	return w;
}

/**
* Creates an image element
*/
Widget create_image(const char* file)
{
	Widget w = create("img");
	w->SetAttribute("src", file);
	return w;
}

/**
* Sets layering
*/
void set_layer(Widget w, int layer)
{
	w->SetProperty("z-index", Rocket::Core::String(100, "%d", layer).CString());
}

/**
* Sets attribute
*/
void set_attribute(Widget w, const char* k, const char* v)
{
	w->SetAttribute(k, v);
}

/**
* Gets attribute
*/
const char* get_attribute(Widget w, const char* k)
{
	return w->GetAttribute(k)->Get<Rocket::Core::String>().CString();
}

/**
* Gets the class of a widget
*/
const char* get_widget_class(Widget w)
{
	return w->GetClassNames().CString();
}

/**
* Sets the class of a widget
*/
const char* set_widget_class(Widget w, const char* classes)
{
	w->SetClassNames(classes);
}

/**
* Gets a widget by id
*/
Widget get_widget_by_id(const char* id)
{
	return GetEngineState()->document->GetElementById(id);
}

/**
* Gets a child
*/
inline Widget get_child(Widget w, int idx)
{
	return w->GetChild(idx);
}

/**
* Gets the widget's id
*/
const char* get_id(Widget w)
{
	return w->GetId().CString();
}

/**
* Forces DOM to refresh
*/
void widget_refresh_all()
{
	Widget w = create("p");
	w->RemoveReference();
}




/**
 * Main loop.
 */
void StartGame(game_loop_ptr gamePtr)
{
	struct enstate* enstate = GetEngineState();

	if (enstate->context == NULL || enstate->document == NULL)
	{
		Rocket::Core::GetSystemInterface()->LogMessage(Rocket::Core::Log::Type::LT_ERROR, "Window was not initialized. Please call create_window() first.");
		return;
	}

	Rocket::Core::Context* context = enstate->context;
	SYSTEMINTERFACE* sysinterface = enstate->rsi;
	SDL_Renderer* renderer = enstate->renderer;

	while (!enstate->exit)
	{
		SDL_Event event;

		SDL_SetRenderDrawColor(renderer, enstate->clear_r, enstate->clear_g, enstate->clear_b, 255);
		SDL_RenderClear(renderer);
		context->Render();
		SDL_RenderPresent(renderer);

		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				enstate->exit = true;
				break;

			case SDL_MOUSEMOTION:
				context->ProcessMouseMove(event.motion.x, event.motion.y, sysinterface->GetKeyModifiers());
				break;
			case SDL_MOUSEBUTTONDOWN:
				context->ProcessMouseButtonDown(sysinterface->TranslateMouseButton(event.button.button), sysinterface->GetKeyModifiers());
				break;

			case SDL_MOUSEBUTTONUP:
				context->ProcessMouseButtonUp(sysinterface->TranslateMouseButton(event.button.button), sysinterface->GetKeyModifiers());
				break;

			case SDL_MOUSEWHEEL:
				context->ProcessMouseWheel(event.wheel.y, sysinterface->GetKeyModifiers());
				break;

			case SDL_KEYDOWN:
			{
				if (event.key.keysym.sym == SDLK_BACKQUOTE && event.key.keysym.mod == KMOD_LSHIFT)
				{
					Rocket::Debugger::SetVisible(!Rocket::Debugger::IsVisible());
					break;
				}

				context->ProcessKeyDown(sysinterface->TranslateKey(event.key.keysym.sym), sysinterface->GetKeyModifiers());

				if (event.key.keysym.sym >= SDLK_SPACE)
					context->ProcessTextInput((Rocket::Core::word)event.key.keysym.sym); // @todo handle shift key
				else if (event.key.keysym.sym == SDLK_RETURN)
					context->ProcessTextInput((Rocket::Core::word)'\n');

				break;
			}

			case SDL_KEYUP:
			{

				break;
			}

			default:
				break;
			}
		}
		
		context->Update();

		// run user's code.
		if (!gamePtr())
			enstate->exit = true;
	}

	context->UnloadDocument(enstate->document);
	context->RemoveReference();
	Rocket::Core::Shutdown();

	delete enstate->rrenderer;
	delete enstate->rsi;
	
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(enstate->screen);

	SDL_Quit();
}

#endif