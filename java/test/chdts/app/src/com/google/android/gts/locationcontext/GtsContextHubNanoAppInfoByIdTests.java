/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.android.gts.locationcontext;

import static com.google.android.utils.chre.ContextHubHostTestUtil.createNanoAppBinary;

import com.android.compatibility.common.util.GmsTest;

import com.google.android.chre.test.chqts.ContextHubInfoByIdTestExecutor;
import com.google.android.chre.test.chqts.ContextHubTestConstants.TestNames;
import com.google.android.utils.chre.ContextHubHostTestUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.Parameters;

import java.util.Arrays;
import java.util.Collection;

/**
 * GTS test suite for verifying Context Hub HAL APIs related to querying Nanoapp information
 * by App ID and Instance ID.
 */
@RunWith(Parameterized.class)
public class GtsContextHubNanoAppInfoByIdTests
        extends GtsContextHubTestBase {
    @Rule
    public final GtsContextHubTestModeRule mTestModeRule = new GtsContextHubTestModeRule();

    private final ContextHubInfoByIdTestExecutor mExecutor;

    private static final int STANDARD_TIMEOUT = 5; // seconds

    private static final long GENERAL_TEST_NANOAPP_ID = 0x476f6f6754000000L;

    /**
     * Provides the parameters for the parameterized tests.
     *
     * Each parameter represents a specific {@link TestNames} enum value,
     * corresponding to a distinct test case (e.g., query by App ID vs. Instance ID).
     * The {@code name = "{0}"} argument formats the test name displayed by the runner
     * using the enum's string representation.
     *
     * @return A collection of {@link TestNames} enums, where each enum represents a test case.
     */
    @Parameters(name = "{0}")
    public static Collection<TestNames> testNames() {
        return Arrays.asList(TestNames.NANOAPP_INFO_BY_APP_ID,
                TestNames.NANOAPP_INFO_BY_INSTANCE_ID);
    }

    /**
     * Constructs a new GtsContextHubNanoAppInfoByIdTests instance for a specific parameterized
     * test case.
     *
     * @param testName The specific {@link TestNames} enum injected by the {@link Parameterized}
     *                 runner, identifying the test scenario to execute (e.g.,
     *                 NANOAPP_INFO_BY_APP_ID).
     */
    public GtsContextHubNanoAppInfoByIdTests(TestNames testName) {
        // Initializes the test executor with the context hub details, the nanoapp binary,
        // and the specific test name for this parameterized run.
        if (ContextHubHostTestUtil.isStaticNanoappsMode()) {
            mExecutor = new ContextHubInfoByIdTestExecutor(getContextHubManager(),
                    getContextHubInfo(),
                    GENERAL_TEST_NANOAPP_ID, testName);
        } else {
            mExecutor = new ContextHubInfoByIdTestExecutor(getContextHubManager(),
                    getContextHubInfo(),
                    createNanoAppBinary(getContextHubInfo(), "general_test.napp"), testName);
        }
    }

    @Before
    public void setUp() {
        mExecutor.init();
    }

    @GmsTest(requirements = {"GMS-6.17-001"})
    @Test
    public void runNanoAppInfoByIdTest() throws InterruptedException {
        mExecutor.run(STANDARD_TIMEOUT);
    }

    @After
    public void tearDown() {
        mExecutor.deinit();
    }
}
