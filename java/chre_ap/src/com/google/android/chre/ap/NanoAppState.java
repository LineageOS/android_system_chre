/*
 * Copyright 2017 The Android Open Source Project
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

package com.google.android.chre.ap;

import android.annotation.NonNull;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A class describing the nanoapp state information resulting from a query to a Context Hub
 * through {@link ContextHubAPManager#queryNanoApps()}. It contains metadata about
 * the nanoapp running on a Context Hub.
 *
 * See "struct chreNanoappInfo" in the CHRE API (system/chre/chre_api) for additional details.
 */

public final class NanoAppState {
    private long mNanoAppId;
    private String mName;
    private int mNanoAppVersion;
    private boolean mIsEnabled;
    private List<String> mNanoAppPermissions = new ArrayList<String>();

    /**
     * @param nanoAppId  The unique ID of this nanoapp, see {#getNanoAppId()}.
     * @param name       The name of the nanoapp.
     * @param appVersion The software version of this nanoapp, see {#getNanoAppVersion()}.
     * @param enabled    True if the nanoapp is enabled and running on the Context Hub.
     */
    public NanoAppState(long nanoAppId, String name, int appVersion, boolean enabled) {
        mNanoAppId = nanoAppId;
        mName = name;
        mNanoAppVersion = appVersion;
        mIsEnabled = enabled;
    }

    /**
     * @param nanoAppId          The unique ID of this nanoapp, see {#getNanoAppId()}.
     * @param name               The name of the nanoapp.
     * @param appVersion         The software version of this nanoapp, see {#getNanoAppVersion()}.
     * @param enabled            True if the nanoapp is enabled and running on the Context Hub.
     * @param nanoAppPermissions The list of permissions required to communicate with this
     *                           nanoapp.
     */
    public NanoAppState(long nanoAppId, String name, int appVersion, boolean enabled,
            @NonNull List<String> nanoAppPermissions) {
        mNanoAppId = nanoAppId;
        mName = name;
        mNanoAppVersion = appVersion;
        mIsEnabled = enabled;
        mNanoAppPermissions = Collections.unmodifiableList(nanoAppPermissions);
    }

    /**
     * @return the unique ID of this nanoapp, which must never change once released on Android.
     */
    public long getNanoAppId() {
        return mNanoAppId;
    }

    /**
     * @return the name of the nanoapp.
     */
    public String getName() {
        return mName;
    }

    /**
     * The software version of this service, which follows the sematic
     * versioning scheme (see semver.org). It follows the format
     * major.minor.patch, where major and minor versions take up one byte
     * each, and the patch version takes up the final 2 (lower) bytes.
     * I.e. the version is encoded as 0xMMmmpppp, where MM, mm, pppp are
     * the major, minor, patch versions, respectively.
     *
     * @return the app version
     */
    public long getNanoAppVersion() {
        return mNanoAppVersion;
    }

    /**
     * @return {@code true} if the app is enabled at the Context Hub, {@code false} otherwise
     */
    public boolean isEnabled() {
        return mIsEnabled;
    }

    /**
     * @return A read-only list of Android permissions that are all required to communicate with
     * this nanoapp.
     */
    public @NonNull List<String> getNanoAppPermissions() {
        return mNanoAppPermissions;
    }
}
