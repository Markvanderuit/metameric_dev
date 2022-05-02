#pragma once

#include <metameric/gl/detail/fwd.h>
#include <metameric/gl/detail/handle.h>
#include <string>

namespace metameric::gl {
  struct WindowCreateInfo {
    /* Window creation settings */
    Array2i size = { 1, 1 };
    std::string title = "";
    uint swap_interval = 1;
    uint msaa_samples = 0;

    /* OpenGL context creation settings */
    ProfileType profile_type = ProfileType::eAny;
    uint profile_version_major = 1;
    uint profile_version_minor = 0;
    bool is_main_context = true;
    const Window *shared_context = nullptr;

    /* Remainder of settings */
    WindowFlags flags = { };
  };

  class Window : public Handle<void *> {
    using Base = Handle<void *>;

    Array2i _window_pos;
    Array2i _window_size;
    Array2i _framebuffer_size;

    std::string _title;
    uint _swap_interval;
    
    bool _is_visible;
    bool _is_maximized;
    bool _is_focused;

    bool _should_close;
    bool _is_main_context;
    
    bool _did_window_resize;  
    bool _did_framebuffer_resize;

    void init_callbacks();
    void dstr_callbacks();

  public:
    /* constr/destr */

    Window() = default;
    Window(WindowCreateInfo info);
    ~Window();

    /* context/renderloop handling */    

    void swap_buffers();
    void poll_events();

    void set_context_current(bool context_current);
    bool is_context_current() const;

    /* getters/setters */

    inline Array2i window_pos() const { return _window_pos; }
    inline Array2i window_size() const { return _window_size; }
    inline Array2i framebuffer_size() const { return _framebuffer_size; }
    void set_window_pos(Array2i window_pos);
    void set_window_size(Array2i window_size);
    bool did_window_resize() const { return _did_window_resize; }
    bool did_framebuffer_resize() const { return _did_framebuffer_resize; }

    inline uint swap_interval() const { return _swap_interval; }
    void set_swap_interval(uint swap_interval);
    
    inline bool visible() const { return _is_visible; }
    inline bool maximized() const { return _is_maximized; }
    inline bool focused() const { return _is_focused; }
    void set_visible(bool visible);
    void set_maximized();
    void set_focused();
    
    inline bool should_close() const { return _should_close; }
    void set_should_close();
    
    inline std::string title() const { return _title; }
    void set_title(const std::string &title);

    /* miscellaneous */
    
    void request_attention() const; // Notify user of an event without focusing

    inline void swap(Window &o) {
      using std::swap;

      dstr_callbacks();
      o.dstr_callbacks();

      Base::swap(o);
      swap(_window_pos, o._window_pos);
      swap(_window_size, o._window_size);
      swap(_framebuffer_size, o._framebuffer_size);
      swap(_title, o._title);
      swap(_swap_interval, o._swap_interval);
      swap(_is_visible, o._is_visible);
      swap(_is_maximized, o._is_maximized);
      swap(_is_focused, o._is_focused);
      swap(_should_close, o._should_close);
      swap(_is_main_context, o._is_main_context);
      swap(_did_window_resize, o._did_window_resize);
      swap(_did_framebuffer_resize, o._did_framebuffer_resize);
      
      init_callbacks();
      o.init_callbacks();
    }

    inline bool operator==(const Window &o) const {
      using std::tie;
      return Base::operator==(o)
        && _window_pos.isApprox(o._window_pos) 
        && _window_size.isApprox(o._window_size)
        && _framebuffer_size.isApprox(o._framebuffer_size)
        && std::tie(_title, _swap_interval, _is_visible, 
                    _is_maximized, _is_focused, _should_close,
                    _is_main_context, _did_window_resize, _did_framebuffer_resize)
        == std::tie(o._title, o._swap_interval, o._is_visible, 
                    o._is_maximized, o._is_focused, o._should_close,
                    o._is_main_context, o._did_window_resize, o._did_framebuffer_resize);
    }
    MET_NONCOPYABLE_CONSTR(Window);
  };
} // namespace metameric::gl