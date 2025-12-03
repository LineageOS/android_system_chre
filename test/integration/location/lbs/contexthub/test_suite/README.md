# Nanoapp Unit Tests

This directory contains the infrastructure for running nanoapp unit tests on the host machine. These tests allow you to verify nanoapp logic in isolation without needing a physical device or a full CHRE simulator.

### How It Works: The Fake API

The core of the host-side unit test framework is the **Fake API**.
For every CHRE API call (e.g. `chreGetTime`, `chreGnssGetCapabilities`), the test framework provides its own version that is exposed to the unit test. When your nanoapp code calls any of these functions, our **fake test version** is called instead of the real system implementation.

## Usage Guide

### Using `TEST_NANOAPP`

`TEST_NANOAPP` is for your unit test it is specialized to ensure your test run with the correct CHRE simulation enviorment. This macro automates the setup process by creating the nanoapp context, initializing dependencies, and starting the test loop.

### Using helper function
In an ideal case, your required behavior is already packaged inside a nice,ready-to-go helper function. All helper functions can be found in [this directory]. There is no limit to how many helper function you want to call, they complement each other and they stack.

### Using `EXPECT_CALL`

By using `TEST_NANOAPP`, a variable called `chre_api_fake_detctor_` is defined in every unit test instance. We use `EXPECT_CALL` to verify that a CHRE API function is called a certain number of time, as the object and CHRE as the function.

As an example, to verify that `chreGetTime` is called twice, you would add the following to the test: `EXPECT_CALL(*chre_api_fake_detector_, chreGetTime()).Times(0);`

### Running Tests

To build the nanoapp unit test you can use the command.
```shell
m nanoapp_unit_test
```

In order to build and run the nanoapp unit test you must use the command.
```shell
atest nanoapp_unit_test
```

### Sample Code
We can look at the sample code in [sample_unit_test.cc] for a complete example of how to set up and run a unit test for a nanoapp. The sample code demonstrates how to use the `TEST_NANOAPP` macro, call helper functions, and verify CHRE API calls using `EXPECT_CALL`.

[this directory]: system/chre/test/integration/location/lbs/contexthub/test_suite/chre_fake_api/helpers
[sample_unit_test.cc]: nanoapp_unit_test/sample/sample_unit_test.cc