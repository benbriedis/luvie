use imgui::Context;
use imgui_winit_support::{HiDpiMode, WinitPlatform};
use std::time::Instant;
use winit::event::{Event, WindowEvent};
use winit::event_loop::EventLoop;
use winit::window::WindowAttributes;

fn main() {

    let event_loop = EventLoop::new().expect("Failed to create EventLoop");
    let window = event_loop
        .create_window(WindowAttributes::default())
        .expect("couldn't create window");

    let mut imgui = Context::create();
    // configure imgui-rs Context if necessary

    let mut platform = WinitPlatform::new(&mut imgui); // step 1
    platform.attach_window(imgui.io_mut(), &window, HiDpiMode::Default); // step 2

    let mut last_frame = Instant::now();
    event_loop.run(move |event, window_target| {
        match event {
            Event::NewEvents(_) => {
                // other application-specific logic
                let now = Instant::now();
                imgui.io_mut().update_delta_time(now - last_frame);
                last_frame = now;
            }
            Event::AboutToWait => {
                // other application-specific logic
                platform
                    .prepare_frame(imgui.io_mut(), &window) // step 4
                    .expect("Failed to prepare frame");
                window.request_redraw();
            }
            Event::WindowEvent {
                event: WindowEvent::RedrawRequested,
                ..
            } => {
                let ui = imgui.frame();
                // application-specific rendering *under the UI*

    ui.window("Hello")
        .build(|| {
            ui.text("Hello, world!");
        });

                // construct the UI

                platform.prepare_render(ui, &window); // step 5
                                                      // render the UI with a renderer
                let draw_data = imgui.render();
                // renderer.render(..., draw_data).expect("UI rendering failed");

                // application-specific rendering *over the UI*
            }
            Event::WindowEvent {
                event: WindowEvent::CloseRequested,
                ..
            } => {
                window_target.exit();
            }
            // other application-specific event handling
            event => {
                platform.handle_event(imgui.io_mut(), &window, &event); // step 3
                                                                        // other application-specific event handling
            }
        }
    })
    .expect("EventLoop error");
}
