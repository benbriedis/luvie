use glium;
use imgui;
use imgui_glium_renderer;
use imgui_winit_support;

fn main() {
  let mut imgui_context = imgui::Context::create();

  let event_loop = glium::glutin::event_loop::EventLoop::new();
  let window_builder = glium::glutin::window::WindowBuilder::new()
    .with_inner_size(glium::glutin::dpi::LogicalSize::new(1024.0, 768.0))
    .with_title("Hello World");
  let context_builder = glium::glutin::ContextBuilder::new();

  let display = glium::Display::new(window_builder, context_builder, &event_loop).unwrap();

  let mut platform = imgui_winit_support::WinitPlatform::init(&mut imgui_context);
  {
    let gl_window = display.gl_window();
    let window = gl_window.window();

    platform.attach_window(
      imgui_context.io_mut(),
      window,
      imgui_winit_support::HiDpiMode::Default,
    );
  }

  let mut renderer = imgui_glium_renderer::Renderer::init(&mut imgui_context, &display).unwrap();

  let mut last_frame = std::time::Instant::now();

  event_loop.run(move |event, _, control_flow| match event {
    glium::glutin::event::Event::NewEvents(_) => {
      imgui_context
        .io_mut()
        .update_delta_time(last_frame.elapsed());
      last_frame = std::time::Instant::now();
    }

    glium::glutin::event::Event::MainEventsCleared => {
      let gl_window = display.gl_window();
      platform
        .prepare_frame(imgui_context.io_mut(), gl_window.window())
        .expect("Failed to prepare frame");
      gl_window.window().request_redraw();
    }

    glium::glutin::event::Event::RedrawRequested(_) => {
      let ui = imgui_context.frame();

    // Added this line to try to render some text
//    ui.text("test");

      let gl_window = display.gl_window();
      let mut target = display.draw();
      platform.prepare_render(&ui, gl_window.window());
      let draw_data = ui.render();
//XXX FAILING HERE:      
      renderer
        .render(&mut target, draw_data)
        .expect("UI rendering failed");
      target.finish().expect("Failed to swap buffers");
    }

    glium::glutin::event::Event::WindowEvent {
      event: glium::glutin::event::WindowEvent::CloseRequested,
      ..
    } => {
      *control_flow = glium::glutin::event_loop::ControlFlow::Exit;
    }

    event => {
      let gl_window = display.gl_window();
      platform.handle_event(imgui_context.io_mut(), gl_window.window(), &event);
    }
  });
}
