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

import android.app.Activity;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

public class ContextHubAPTester extends Activity {
    // Declare a TextView to display the result
    private TextView mResultTextView;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Create a vertical container layout
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);

        // Create a TextView to show the result
        mResultTextView = new TextView(this);
        mResultTextView.setText("Result will be shown here after clicking the button.");

        // Create a Button to trigger init CHRE AP
        Button initButton = new Button(this);
        initButton.setText("Init CHRE AP");
        initButton.setOnClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        int res = Native.init();
                        mResultTextView.setText("init returns: " + Integer.toString(res));
                    }
                });
        layout.addView(initButton);

        // Create a button to destroy CHRE AP
        Button destroyButton = new Button(this);
        destroyButton.setText("Destroy CHRE AP");
        destroyButton.setOnClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        Native.destroy();
                        mResultTextView.setText("destroyed");
                    }
                });
        layout.addView(destroyButton);

        // Setup the rest of the things.
        layout.addView(mResultTextView);
        setContentView(layout);
    }
}
