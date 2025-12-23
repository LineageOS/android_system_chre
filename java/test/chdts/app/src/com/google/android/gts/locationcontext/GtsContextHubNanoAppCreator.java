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

import android.content.Context;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.NanoApp;
import android.os.Build;

import androidx.test.InstrumentationRegistry;

import com.google.android.utils.chre.ContextHubHostTestUtil;

import org.junit.Assert;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * This "class" contains a helper function to create a NanoApp object.
 *
 * @see create
 */
public class GtsContextHubNanoAppCreator {
    private static final String TAG = "GtsContextHubNanoAppCreator";

    // 0-based offset into the header where the "magic" value starts.
    private static final int HEADER_MAGIC_OFFSET = 4;

    // 0-based offset into the header where the app ID value starts.
    private static final int HEADER_APP_ID_OFFSET = 8;

    // NOTE: If this shifting seems odd (since it's actually "ONAN"), note
    //     that it matches how this is defined in context_hub.h.
    private static final int HEADER_MAGIC =
            (((int) 'N' <<  0) | ((int) 'A' <<  8) | ((int) 'N' << 16) | ((int) 'O' << 24));

    // Byte ordering established in context_hub.h.
    private static final ByteOrder HEADER_ORDER = ByteOrder.LITTLE_ENDIAN;

    private static long getAppId(ByteBuffer contents, String fullName) {
        // We get the App ID from the header.
        try {
            // First, we confirm that we have a legitimate header.
            Assert.assertEquals("Invalid nanoapp header for " + fullName,
                                HEADER_MAGIC,
                                contents.getInt(HEADER_MAGIC_OFFSET));
            // Now get the ID
            return contents.getLong(HEADER_APP_ID_OFFSET);
        } catch (IndexOutOfBoundsException e) {
            Assert.fail("Nanoapp " + fullName + " is severely undersized");
            // We never get here, but the Java compiler doesn't know that.
            throw e;
        }
    }

    private static ByteBuffer getFileContents(InputStream input, String fullName) {
        ByteArrayOutputStream contents = new ByteArrayOutputStream();

        try {
            byte[] buffer = new byte[1024];
            int bytesRead;
            while ((bytesRead = input.read(buffer, 0, buffer.length)) != -1) {
                contents.write(buffer, 0, bytesRead);
            }
        } catch (IOException e) {
            Assert.fail("IOException while reading " + fullName + ": "
                        + e.toString());
        }
        // We don't access or care about most of the file contents. But if these
        // contents have a header, then we will access and care about the values
        // in the header.  Thus, we set the byte order based on what the header is.
        return ByteBuffer.wrap(contents.toByteArray())
            .order(HEADER_ORDER);
    }


    /**
     * Create a "NanoApp" instance filled with the requested file.
     *
     * We use the given ContextHubInfo to determine the path for the nanoapp.
     * The given 'filename' tells us which nanoapp to load.  This nanoapp
     * must have a valid "header".
     *
     * This method will fatally end the test if:
     * - We can't find any nanoapp files for the current Context Hub.
     * - We can't find the specific nanoapp for the current Context Hub.
     * - There's an I/O error loading the nanoapp from disk into memory.
     * - There is not a valid "header" on the nanoapp.
     *
     * @param info  The ContextHubInfo we use to determine which platform
     *     version of the nanoapp we should load.
     * @param filename  The name of the nanoapp file we're loading.
     *
     * @return The NanoApp instance.  Note this method will never return null,
     *     and will instead abort internally with failure.
     */
    public static NanoApp create(ContextHubInfo info, String filename) {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        String fullName =
                ContextHubHostTestUtil.getNanoAppBinaryPath(context, info) + "/" + filename;
        InputStream inputStream =
                ContextHubHostTestUtil.getNanoAppInputStream(context, fullName);
        ByteBuffer fileContents = getFileContents(inputStream, fullName);
        long appId = getAppId(fileContents, fullName);

        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.N_MR1) {
            // Need to truncate the appId for the old NanoApp API
            int appIdHack = (int) (appId & 0xFFFFFFFF);
            return new NanoApp(appIdHack, fileContents.array());
        } else {
            return new NanoApp(appId, fileContents.array());
        }
    }
}
