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

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Gravity;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import com.google.android.chre.ap.ContextHubAPManager;
import com.google.android.chre.ap.ContextHubAPNative;
import com.google.android.chre.ap.ContextHubTransaction;
import com.google.android.chre.ap.NanoAppMessage;
import com.google.android.chre.ap.NanoAppState;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

@SuppressLint({"SetTextI18n", "DefaultLocale"})
public class ContextHubAPTester extends Activity {
    private static final String TAG = "ContextHubAPTester";

    // Declare a TextView to display the result
    private TextView mResultTextView;
    private Spinner mNanoappSpinner;
    private LinearLayout mNanoAppListLayout;
    private final Handler mMainHandler = new Handler(Looper.getMainLooper());
    private TextView mMessageTextView;
    private Thread mEventLoopThread = null;

    private Set<String> getBundledSoFileNames() {
        Set<String> soFileNames = new HashSet<>();

        ApplicationInfo appInfo = getApplicationInfo();
        String apkPath = appInfo.sourceDir;
        Log.d(TAG, "APK Path: " + apkPath);

        ZipFile zipFile = null;
        try {
            zipFile = new ZipFile(new File(apkPath));

            Enumeration<? extends ZipEntry> entries = zipFile.entries();
            while (entries.hasMoreElements()) {
                ZipEntry entry = entries.nextElement();
                String entryName = entry.getName();
                if (entryName.endsWith(".so")) {
                    File soFile = new File(entryName);
                    String fileName = soFile.getName();
                    if (fileName.startsWith("nanoapp_")) {
                        soFileNames.add(fileName);
                    }
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "Failed to read APK contents", e);
        } finally {
            if (zipFile != null) {
                try {
                    zipFile.close();
                } catch (IOException e) {
                    // ignore
                }
            }
        }
        return soFileNames;
    }

    private void populateDynamicNanoappList() {
        String[] nanoappFiles = getBundledSoFileNames().toArray(new String[0]);
        ArrayAdapter<String> adapter = new ArrayAdapter<>(
                this,
                android.R.layout.simple_spinner_item,
                nanoappFiles
        );
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mNanoappSpinner.setAdapter(adapter);
    }

    private void populateNanoAppListView() {
        mMainHandler.postDelayed(() -> {
            mNanoAppListLayout.removeAllViews();

            NanoAppState[] nanoAppInfos = ContextHubAPNative.listNanoapps();

            if (nanoAppInfos == null || nanoAppInfos.length == 0) {
                TextView emptyView = new TextView(this);
                emptyView.setText("No nanoapps currently loaded.");
                emptyView.setPadding(8, 8, 8, 8);
                mNanoAppListLayout.addView(emptyView);
                return;
            }

            for (NanoAppState nanoAppInfo : nanoAppInfos) {
                LinearLayout rowLayout = new LinearLayout(this);
                rowLayout.setOrientation(LinearLayout.HORIZONTAL);
                rowLayout.setLayoutParams(new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT));
                rowLayout.setGravity(Gravity.CENTER_VERTICAL);

                TextView infoTextView = new TextView(this);
                LinearLayout.LayoutParams textParams = new LinearLayout.LayoutParams(
                        0, LinearLayout.LayoutParams.WRAP_CONTENT, 1.0f);
                infoTextView.setLayoutParams(textParams);
                infoTextView.setText(String.format("Instance ID: %d, Name: %s",
                        nanoAppInfo.getNanoAppId(), nanoAppInfo.getName()));
                infoTextView.setPadding(8, 8, 8, 8);

                Button unloadButton = new Button(this);
                unloadButton.setText("Unload");
                unloadButton.setLayoutParams(new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT));

                unloadButton.setOnClickListener(v -> {
                    long idToUnload = nanoAppInfo.getNanoAppId();
                    ContextHubTransaction<Void> transaction =
                            ContextHubAPManager.getInstance().unloadNanoApp(idToUnload);

                    ContextHubTransaction.Response<Void> response = null;
                    try {
                        response = transaction.waitForResponse(/* timeout= */1, TimeUnit.SECONDS);
                    } catch (TimeoutException | InterruptedException e) {
                        mResultTextView.setText("Unload timed out for instance: " + idToUnload);
                        return;
                    }

                    if (response != null
                            && response.getResult() == ContextHubTransaction.RESULT_SUCCESS) {
                        mResultTextView.setText("Unload successful for instance: " + idToUnload);
                    } else {
                        mResultTextView.setText("Unload failed for instance: " + idToUnload);
                    }
                    populateNanoAppListView();
                });

                rowLayout.addView(infoTextView);
                rowLayout.addView(unloadButton);
                mNanoAppListLayout.addView(rowLayout);
            }
        }, 500);
    }

    /**
     * Called when the activity is first created.
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);
        mResultTextView = findViewById(R.id.resultTextView);
        Button initButton = findViewById(R.id.initButton);
        Button destroyButton = findViewById(R.id.destroyButton);
        mNanoappSpinner = findViewById(R.id.nanoappSpinner);
        Button loadButton = findViewById(R.id.loadButton);
        mNanoAppListLayout = findViewById(R.id.nanoAppListLayout);
        Button messageButton = findViewById(R.id.messageButton);
        mMessageTextView = findViewById(R.id.messageTextView);

        populateDynamicNanoappList();

        initButton.setOnClickListener(v -> {
            // Start the CHRE environment.
            ContextHubAPManager.getInstance().init(this);

            mEventLoopThread = new Thread(() -> {
                ContextHubAPManager.getInstance().runEventLoop(
                        ContextHubAPManager.EventLoopMode.PROVIDED);
            });
            mEventLoopThread.start();
            ContextHubAPManager.getInstance().setEventLoopThread(mEventLoopThread);

            mResultTextView.setText("CHRE AP: Started");
            populateNanoAppListView();
        });

        destroyButton.setOnClickListener(v -> {
            ContextHubAPManager.getInstance().destroy();
            mResultTextView.setText("CHRE AP: Destroyed");
            mNanoAppListLayout.removeAllViews();
        });

        loadButton.setOnClickListener(v -> {
            String selectedNanoapp = (String) mNanoappSpinner.getSelectedItem();
            if (selectedNanoapp == null) {
                Toast.makeText(this, "No nanoapp selected", Toast.LENGTH_SHORT).show();
                return;
            }
            boolean success = ContextHubAPManager.getInstance().loadNanoApp(selectedNanoapp);
            mResultTextView.setText(
                    (success ? "Successfully loaded: " : "Failed to load: ") + selectedNanoapp);
            populateNanoAppListView();
        });

        // Create a Button to send message to Message World nanoapp.
        var callback = new MessageCallback(mMessageTextView);
        var client = ContextHubAPManager.getInstance().createClient(callback);
        messageButton.setOnClickListener(v -> {
            var message = NanoAppMessage.createMessageToNanoApp(
                    0x0123456789000003L /*Message World Nanoapp ID*/, 100,
                    "Test message".getBytes(StandardCharsets.UTF_8));
            var result = client.sendMessageToNanoApp(message);
            mMessageTextView.setText(
                    "Sent message to Message World Nanoapp with res: "
                            + result);
        });
    }
}
