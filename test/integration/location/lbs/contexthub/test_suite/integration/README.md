# Nanoapp Integration tests

The nanoapp integration testing framework is used to verify the correct end-to-end functioning of nanoapps. For integration test the author must provide the reading/data at different time, and the test framework will use this data to simulate the environment and verify the nanoapp's behavior. Integration tests are deterministic simulations used to verify end-to-end nanoapp behavior. The test author defines a timeline of sensor readings and events (the Data Feed), and the framework injects these into the nanoapp to verify how it responds.

## Build Configuration
To start, lets take a look at the example [Android.bp] file. The `cc_test_host` rule named `count_aps_nanoapp_test` defines the integration test target. It specifies the source files, static nanoapps to include, and necessary dependencies for the test. `count_aps_nanoapp.cc` and `count_aps_static_nanoapp.cc` are included directly in the srcs list. This causes the nanoapp's code to be compiled and statically linked immediately alongside the test code.

## Usage Guide

### `INTEGRATION_TEST`

The integration test macro takes three arguments: the test fixture class, a unique test name, and a derived instance of `DataFeedBase` class. It then runs the simulation using the specified data feed to guide the test framework. Once it completes, it executes all code in the body of this macro, which is where all the verification will live, verification function can be found in [here]. All the tests in the examples will only be concerned with `GetHostMessages()` which returns the messages received by the test framework (which is from the nanoapps), indexed by time.

### DataFeed Class
The DataFeed class is responsible for defining the timeline of events and sensor readings that will be injected into the nanoapp during the simulation. By deriving from `DataFeedBase`, you specify the test behavior using two mechanisms:

1.  **Overrides (Capabilities):** You override methods like `GetCapabilitiesWifi()` to tell the nanoapp what features the simulated device supports.
2.  **Constructor (Timeline):** You populate member maps (like `wifi_scan_events_`) in the constructor. This creates a "script" of events that the framework will inject at specific timestamps.

### ScenarioThree: Simulating Passive Environment Data
`ScenarioThree` demonstrates a more complex interaction: Simulating Passive Environment Data. In this scenario, we verify that the nanoapp correctly handles WiFi scan results that occur asynchronously. To do this, our DataFeed class must do two things:

* **Advertise Capabilities (Override):** We override `GetCapabilitiesWifi` to return `CHRE_WIFI_CAPABILITIES_SCAN_MONITORING`. If we returned `NONE`, the nanoapp would assume WiFi is unavailable.

* **Schedule the Timeline (Constructor):** Inside the constructor, we populate the `wifi_scan_events_` map. This schedules specific scan results to appear at specific times.

You can view the full implementation, showing how the constructor populates the event map, in [count_aps_nanoapp_test.cc].

### Integrating Static Nanoapp
For integration tests, the nanoapp is compiled and linked directly under test and is loaded as a static nanoapp within the test framework. This allows the test environment to directly interact with the nanoapp's entry points. An example of a static nanoapp can be found in [count_aps_static_nanoapp.cc].

### Running Tests

In the sample code shown below in order to build the given nanoapp integration test you can use the command.
```shell
m count_aps_nanoapp_test
```

In order to build and run the sample nanoapp integration test you must use the command.
```shell
atest count_aps_nanoapp_test
```

### Sample Code
We can look at the sample code in [count_aps_static_nanoapp.cc] and [count_aps_nanoapp_test.cc] for a complete example of how to set up and run an integration test for a nanoapp. The static nanoapp code defines the behavior of the nanoapp, while the test code sets up the integration test environment, defines the data feed, and verifies the nanoapp's responses.

[Android.bp]: examples/codelab/count_aps/Android.bp
[here]: verify/verification_data.h
[count_aps_static_nanoapp.cc]: examples/codelab/count_aps/count_aps_static_nanoapp.cc
[count_aps_nanoapp_test.cc]: examples/codelab/count_aps/count_aps_nanoapp_test.cc