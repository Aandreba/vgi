# Systems and Layers

VGI uses a modular system/layer model to organize engine functionality. **Systems** are high-level modules that run in the main loop, processing events and per-frame updates. For example, the engine's window management is implemented as a `vgi::window` system. **Layers** are components attached to a window system; they encapsulate rendering or logic for that window. In essence, *layers are to a window what systems are to the entire application*.
Each window can have multiple layers (often forming a stack), allowing you to separate different aspects of rendering (such as UI, game world, overlays) into distinct layer classes.

During runtime, VGI's main loop calls every active system's event and update methods each frame. All incoming SDL events are dispatched to **every system** via `system::on_event`, allowing each system to handle input as needed. For instance, a window system filters events that belong to its SDL window and forwards those to its layers, while a non-window system could handle global events (e.g. input not tied to a specific window). After events are processed, the engine calls `system::on_update` on each system once per frame. This design lets systems run independently - you could have multiple windows (each as a system with its own layers) or other systems for game logic, audio, etc., all updating concurrently.

## Implementing a Custom System

To create a new system, you subclass `vgi::system` and override the virtual methods for the behaviors you need. A system has two primary callbacks in its lifecycle:

- `on_event(const SDL_Event& event)` -- Called for each SDL event polled. Override this to handle input or OS events relevant to your system (e.g. keyboard input, quit events, etc.).
- `on_update(const vgi::timings& ts)` -- Called once per frame to update your systems state. The `timings` object provides timing info like frame delta time.

By default, these methods do nothing (the base class provides empty implementations), so override them in your subclass to implement your systems logic. A simple custom system might look like:

```cpp
struct MyGameLogicSystem : public vgi::system {
    void on_event(const SDL_Event& e) override {
        // e.g. respond to key presses
    }

    void on_update(const vgi::timings& ts) override {
        // e.g. update game state using ts.delta time
    }
};
```

### Lifecycle and Transitions

When a system is first added via `vgi::add_system` or `vgi::emplace_system`, it becomes active immediately on the next frame. There is no explicit "on_attach" for systems, so any initialization can be done in the constructor, and cleanup in the destructor. If a system needs to be removed or swapped out at runtime, use the transition API: `system::transition_to`. For example, within a system's update you might do:

```cpp
// Inside MyGameLogicSystem::on_update
if (levelComplete) {
    // Replace this system with another one at end of frame
    this->template transition_to<NextLevelSystem>(nextLevelParams...);
}
```

Calling `transition_to<NextLevelSystem>()` will queue the current system to be replaced with a new `NextLevelSystem` instance at the **end** of the frame. The old system's destructor will run after being replaced. If you simply want to remove a system without replacing it, call `detach()`, which is equivalent to calling `transition_to(nullptr)` -- the system will be removed from the main loop on the next frame. The engine checks for these transition requests each frame and handles the swap/removal before calling the next update.

## Implementing a Custom Layer

Custom layers are implemented by subclassing `vgi::layer`. Layers have a richer set of lifecycle callbacks since they integrate with the rendering pipeline of a window. The key virtual methods you can override are:

- `on_attach(vgi::window& win)` -- Called when the layer is attached to a window. Use this to initialize resources (e.g. load textures, create pipelines) using the provided window (which gives access to Vulkan device, etc.).
- `on_event(vgi::window& win, const SDL_Event& event)` -- Called for each event relevant to that window. Use this to handle input for the layer (e.g. camera controls on key/mouse events).
- `on_update(vgi::window& win, vk::CommandBuffer cmd, uint32_t frame_id, const vgi::timings& ts)` -- Called every frame **before rendering**, to update logic or GPU resources. You are given the Vulkan command buffer for the frame, the index of the current frame (for managing frame-local resources in double/triple buffering), and timing info.
 You can record non-draw commands here (e.g. updating uniform buffers).
- `on_render(vgi::window& win, vk::CommandBuffer cmd, uint32_t frame_id, const vgi::timings& ts)`-- Called every frame to record **rendering** commands for this layer. This is where you issue draw calls via the provided command buffer. The engine sets the appropriate render pass, viewport, and scissor for your layer before calling this method.
- `on_detach(vgi::window& win)` -- Called when the layer is removed from the window (either via explicit removal or when the window is closing). Clean up any resources here (e.g. free GPU resources, detach event listeners).

All of these methods have default no-op implementations, so you only override what you need. For example, a custom layer might be defined as:

```cpp
struct MyGameLayer : public vgi::layer {
    void on_attach(vgi::window& win) override {
        // Setup graphics pipeline, load meshes, etc.
    }

    void on_event(vgi::window& win, const SDL_Event& e) override {
        // Handle input for this layer (e.g. camera movement)
    }

    void on_update(vgi::window& win, vk::CommandBuffer cmd, uint32_t frame_id, const vgi::timings& ts) override {
        // Update uniforms or game objects, record non-render commands if needed
    }

    void on_render(vgi::window& win, vk::CommandBuffer cmd, uint32_t frame_id, const vgi::timings& ts) override {
        // Issue draw calls for this layer
    }

    void on_detach(vgi::window& win) override {
        // Cleanup resources (destroy pipelines, buffers, etc.)
    }
};
```

### Layer Transitions

Similar to systems, a layer can replace itself or remove itself at runtime. Call `layer::transition_to<NewLayer>()` to schedule swapping this layer with a new one at the end of the frame. The engine will call the old layer's `on_detach`, attach the new layer (calling its `on_attach`), and from the next frame onward the new layer will be updated/rendered in place of the old. To simply remove a layer, use `layer::detach()`, which is equivalent to transitioning to `nullptr`. For example, a UI layer might call `detach()` when the user closes that UI, and the engine will remove it cleanly after finishing the frame.

## Managing Systems and Layers

You manage systems and layers through the vgi API at runtime. Typically, you first initialize the VGI environment with `vgi::init(...)`, then create your initial systems (at least one window system for graphics). You can add a system in two ways:

- `vgi::add_system(std::unique_ptr<vgi::system>&& sys)` -- returns an ID (index handle) for the new system.
- `vgi::add_system<T>(args...)` or `vgi::emplace_system<T>(args...)` -- constructs a new system of type `T` in-place. `emplace_system` is convenient because it returns a reference to the newly created system.

For example, to create a window and add a layer to it in one go, you can do:

```cpp
const vgi::device& gpu = /* select a device */;
vgi::window& window = vgi::emplace_system<vgi::window>(
    gpu, u8"Game Window", 1280, 720, SDL_WINDOW_RESIZABLE
);
window.template add_layer<MyGameLayer>();
```

Once your systems and layers are set up, call `vgi::run()` to enter the main loop. The engine will then start pumping events and updates as described in the overview. Systems (and their layers) will continue running until you remove them or shut down the engine. You can add or remove systems/layers at runtime as needed:

- To add a layer to an existing window at runtime, simply call `window.add_layer<NewLayer>(args...)` on the `vgi::window` object. The layer's `on_attach` will be called immediately upon addition.
- To remove or replace a layer during the game, from within that layer call `detach()` or `transition_to<OtherLayer>()` as discussed, or from outside you can remove it by its ID if you kept the handle (the `add_layer` method returns a layer ID).
- To remove a system (for example, closing a window), you can call `vgi::get_system(id).detach()` or have the system itself call `detach()`. The `vgi::window` system internally calls `detach()` on itself when the window is closed by the user, which signals the main loop to remove that window system.

Finally, to gracefully shut down the entire engine (e.g. when your game wants to exit), you can call `vgi::shutdown()`. This sets a flag that breaks out of the main loop after the current frame. After `vgi::run()` returns, call `vgi::quit()` to clean up all remaining systems and release resources.
