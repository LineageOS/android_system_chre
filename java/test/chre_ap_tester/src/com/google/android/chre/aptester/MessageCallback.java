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

package com.google.android.chre.aptester;

import android.hardware.location.ContextHubClient;
import android.hardware.location.ContextHubClientCallback;
import android.hardware.location.NanoAppMessage;
import android.widget.TextView;

class MessageCallback extends ContextHubClientCallback {

    private TextView mMessageTextView;

    MessageCallback(TextView mMessageTextView) {
        this.mMessageTextView = mMessageTextView;
    }

    public void onMessageFromNanoApp(ContextHubClient client, NanoAppMessage message) {
        mMessageTextView.append("\nReceived message from nanoapp, nanoapp ID: "
                + message.getNanoAppId()
                + ", type: " + message.getMessageType()
                + ", messageSize: " + message.getMessageBody().length);
    }
}
