# CHDTS

## What is CHDTS?

Context Hub Development Test Suite (CHDTS) it a test suite that tests for basic
functionality and stress testing of CHRE for development/testing purpose. It
shares the same implementation with CHQTS but decoupled with GTS so that non-GMS
devices could also benefit from it.


## Where is the code?

*   Test nanoapps: system/chre/apps/test/chqts/, the test nanoapps are shared with GTS tests.
*   Test code: system/chre/java/test/chdts/app/

## How to run CHDTS

1.  Flash device to a build to test.

    A userdebug or eng build is needed to run CHDTS test app in privilege mode.

2.  Compile CHDTS

    ```
    source build/envsetup.sh
    lunch aosp_arm64
    m -j45 chdts
    ```

3.  Run CHDTS:

    NOTE: If running below commands fails with `Unable to find aapt in path.`,
    run`make aapt` first and try again.

    Running all of the CHDTS tests:

    ```
    chdts-tradefed run chdts --skip-system-status-check com.android.compatibility.common.tradefed.targetprep.NetworkConnectivityChecker --primary-abi-only
    ```

    To run a single test (for example):

    ```
    chdts-tradefed run chdts --skip-system-status-check com.android.compatibility.common.tradefed.targetprep.NetworkConnectivityChecker --module chdts-tradefed-tests --primary-abi-only --test com.android.chre.chdts.ChdtsHostTestCases#testContextHubBusyStartupNanoAppTest
    ```

## Running CHDTS in No-Reboot Mode

Standard CHDTS execution requires pushing test artifacts (APKs) and configuration files to the device, followed by a cleanup phase. These setup operations necessitate a device reboot to apply permissions and changes effectively.

To reduce iteration time and help troubleshoot flaky tests, we provide a "No-Reboot" mode. This decouples the setup phase from the execution phase:

1.  **One-time Environment Setup:** Run this command to push the necessary files and permissions. This step requires a reboot but only needs to be performed once.

    ```
    chdts-tradefed run chdts --module chdts-setup
    ```

2.  **Test Execution:** Once the environment is set up, you can run the tests repeatedly without rebooting

    ```
    chdts-tradefed run chdts --skip-system-status-check com.android.compatibility.common.tradefed.targetprep.NetworkConnectivityChecker --module chdts-test-only --primary-abi-only
    ```

Note that some test files are not cleaned up after the test, and you need to re-run the setup phase again if you switch to a newer version of CHDTS package.

## Using external nanoapps

You can also use external nanoapps not bundled into the test APK. This allows
partners to debug tests.

```
chdts-tradefed run chdts --skip-system-status-check com.android.compatibility.common.tradefed.targetprep.NetworkConnectivityChecker --module chdts-tradefed-tests --primary-abi-only --test com.android.chre.chdts.ChdtsHostTestCases#testContextHubBusyStartupNanoAppTest --module-arg chdts-tradefed-tests:set-option:com.android.chre.chdts.ChdtsHostTestCases:externalNanoAppPath:/vendor/etc/chre/chdts
```

## Enable stress test

The stress test is by default disabled, and the stress test returns ASSUMPTION_FAILURE status.
To enable the stress test, `stressTestDurationSeconds` parameter requires a positive number.
You could tune this parameter to run the stress test longer.

```
chdts-tradefed run chdts --skip-system-status-check com.android.compatibility.common.tradefed.targetprep.NetworkConnectivityChecker --module chdts-tradefed-tests --primary-abi-only --test com.android.chre.chdts.ChdtsHostTestCases#testContextHubStress --module-arg chdts-tradefed-tests:set-option:com.android.chre.chdts.ChdtsHostTestCases:stressTestDurationSeconds:30
```
