# Nanoapp Common Design Patterns

This guide highlights common architectural patterns and optimization strategies
observed in production-grade CHRE nanoapps. These practices focus on maximizing
modularity, ensuring power efficiency, and adhering to the strict resource
constraints of the Context Hub Runtime Environment.

## Architectural Patterns

### The "Manager-Logic-Interface" Separation

Avoid monolithic designs where the CHRE event callback (`nanoappHandleEvent`)
directly contains business logic. Instead, enforce a strict separation of
controllers, models and interfaces to improve testability and portability.

*   **AppManager (The Controller):**
    *   **Role:** Acts as the central hub and owner of global state.
    *   **Implementation:** Use the **Singleton Pattern**. The standard CHRE
        entry points (`nanoappStart`, `nanoappHandleEvent`) could act as thin
        bridges that delegate to `AppManager::get()->HandleEvent(...)`.
    *   **Responsibility:** Routes events (e.g., timers, sensor data, host
        messages) to specific sub-components and manages the lifecycle of the
        nanoapp.
    *   **Top Level State & Coordination:** The `AppManager` is the coordinator
        for anything outside the scope of a single sub-component. For example,
        if the nanoapp serves multiple consumers (e.g., host and other
        nanoapps), the Manager could maintain a `ClientList` (a form of
        top-level state). It merges conflicting requirements (e.g., different
        sampling rates) to determine the "strictest" configuration needed,
        coordinating the requests from various clients.
*   **Logic Engines (The Model):**
    *   **Role:** Contains the core algorithmic processing (e.g.,
        `MotionDetectorLogic`).
    *   **Constraint:** These classes should be self-contained where possible,
        operating only on given inputs and producing well-defined outputs
        without any side effects. However, this is not possible or desired in
        all cases, for example some modules necessarily need to interact with
        the system to accomplish their goals.
    *   **Interaction:** Use **Listener Interfaces** (C++ pure virtual classes).
        For example, the Logic engine calls `listener->onEventDetected()`, which
        the `AppManager` implements to perform the actual CHRE system call. The
        listener paradigm helps avoid circular dependencies between the
        AppManager and the given subcomponent.
        *   *Considerations:* While offering these benefits, be mindful that
            virtual method calls incur costs (increased memory footprint,
            potential for reduced compiler optimization, and additional CPU
            cycles per invocation) compared to direct function calls. Given that
            tooling exists to easily mock/fake CHRE APIs, if virtual interfaces
            are used, structuring the code to allow for devirtualization by the
            compiler where possible.
*   **Messaging Component:**
    *   **Role:** Handles Protocol Buffer (Nanopb) encoding/decoding and nanoapp
        abstractions.
    *   Decoding, encoding, and sending nanopb messages requires a fair bit of
        overhead that is good to separate organizationally.

## Advanced Power & Sensor Management

### The "Leading Indicator" Gating Pattern

To balance latency and power, high-power sensors (e.g. Audio, high-ODR Gyro/Mag)
should never run continuously.

*   **Tiered Sensing:** Use a low-power "trigger" sensor (e.g., Stationary
    Detect Sensor, Instant Motion Detect Sensor) to gate high-power sensors.
    *   Example: If stationary, disable all other sensors.
    *   Example: Subscribe to the very low-power Instant Motion Detect sensor.
        When this sensor triggers, enables the accelerometer.
*   **Managing Trigger Sensors:** Trigger sensors should not be re-enabled
    immediately after an event. The system should implement a debouncing or
    "hold-off" state to prevent the trigger sensor from firing continuously.

### Passive & Opportunistic Messaging

Minimize active requests to the underlying system to prevent waking the AP
unnecessarily, as waking the AP is energy intensive, and can reduce the benefits
of CHRE's low-power design if done too frequently. Track the AP's state and
prefer to send messages "Opportunistically" when the AP is already awake.

*   **Opportunistic Flushing:** Monitor `chreIsHostAwake()`.
*   **Sleep Mode:** If the Host is asleep, buffer non-critical data.
*   **Wake Mode:** Listen for `CHRE_EVENT_HOST_AWAKE`. If the Host wakes for
    another reason, flush the buffer immediately. This makes the data transfer
    energy cost negligible.
*   **Hard-Capped Opportunism:** While opportunistic sending is power-efficient,
    it's crucial to implement a hard cap on how long data can be buffered.
    Messages should be sent even if the host hasn't woken up when either the
    buffer reaches a certain fullness or a timing requirement of the nanoapp's
    contract dictates (e.g., data must be delivered within X seconds).

## Data Conditioning & Embedded ML

### Signal Conditioning Pipeline

Nanoapps may receive sensor data at rates higher than requested (due to shared
power domains running at the fastest requested HW rate). Resampling is required
to normalize this stream for the ML model, but it should generally be an integer
multiple of their requested rate.

*   **Resampling:** Implement a **Linear Interpolator** to align variable-rate
    incoming samples onto a fixed-frequency grid before feeding them into ML
    models. Prioritize integer factor downsampling where possible to minimize
    signal artifacts.
*   **Batching Strategy:** Decouple sensor events from inference. Accumulate
    interpolated samples into a fixed-size window (e.g., `kNumSamplesPerBatch`).
    Run the expensive inference step only when the window is full.

### TFLM (TensorFlow Lite for Micro) Optimization

*   **Model Storage:** Compile models as C++ byte arrays. Use
    `__attribute__((aligned(4)))` or `DATA_ALIGN_ATTRIBUTE` to satisfy TFLM
    architecture requirements.
*   **Persistent Interpreters:** Allocate the TFLM Interpreter and Tensor Arena
    *once* at startup and reuse them. Never re-allocate per inference to avoid
    fragmentation.
*   **Quantization:** Decompress weights on-the-fly. Store weights as `uint16_t`
    in flash and expand to `float` only during the inference pass using linear
    scaling.

## Memory Management & Safety

### Static over Dynamic

Heap fragmentation is a primary cause of long-term instability in always-on
nanoapps.

*   **Containers:** Prefer `chre::FixedSizeVector<T, N>` and
    `chre::ArrayQueue<T, N>` over `std::vector` or `std::deque`. These
    structures allocate on the stack or BSS and guarantee zero fragmentation.
*   **Scratch Arenas:** For heavy math operations (e.g., FFT), use a single
    static "scratch" buffer, rather than allocating temporary buffers inside the
    event loop.
*   **Large Data:** For nanoapps that collected large batches or buffers of data
    prefer statically allocated memory. This also guarantees that the required
    memory is available at load time.

### Zero-Copy Nanopb Usage

*   **Callbacks:** For repeated fields (e.g., lists of WiFi APs), use Nanopb
    `callback` functions to stream data directly from internal structures to the
    output buffer. Avoid creating intermediate "Proto Structs" that duplicate
    data.
*   **Buffer Sizing:** Calculate the message size on the fly and allocate the
    exact required capacity, using either dynamic memory or partitions within a
    pre-allocated buffer.

## Extensibility & Communication

### Nanoapp-as-a-Service

Complex nanoapps can expose services to other nanoapps, not just the Host.

*   **Service Registry:** Implement a central service that maintains a list of
    registered nanoapp Instance IDs.
*   **Client Libraries:** Provide a matching C++ client library for other
    nanoapps. This library abstracts the handshake (`chreSendEvent`), version
    negotiation, and type safety, hiding the IPC complexity from the consumer.

## Debugging & Observability

### Contextual Debug Dumps

Real-time logging via `chreLog` allows for tracing and there are also other ways
to reveal the internal state of nanoapps:

*   **Snapshotting:** Implement `HandleDebugDumpEvent`. When triggered (via
    `dumpsys`), write a snapshot of internal state (current buffer usage, active
    states, last 5 inference results) to the provided builder.
*   **Circular Event Log:** Maintain a small in-memory circular buffer of the
    last state transitions or errors. Print this only during the Debug Dump to
    provide context on "how the system arrived at this state."

### Timer Identification via Cookies

Avoid managing complex maps of Timer IDs to timer purposes.

*   **Cookie Pattern:** Pass a pointer to a specific state structure or a
    "Cookie" wrapper (containing the timer type) to `chreTimerSet`.
*   **Retrieval:** Inside `nanoappHandleEvent`, cast the `eventData` pointer
    back to your Cookie type to immediately identify the timer's purpose.
