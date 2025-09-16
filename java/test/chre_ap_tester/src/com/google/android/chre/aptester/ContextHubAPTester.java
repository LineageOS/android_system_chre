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

        // 1. Create a vertical container layout
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);

        // 2. Create a Button to trigger the native call on click
        Button clickButton = new Button(this);
        clickButton.setText("Click Me!");
        layout.addView(clickButton); // Add the button to the layout

        // 3. Create a TextView to show the result
        mResultTextView = new TextView(this);
        mResultTextView.setText("Result will be shown here after clicking the button.");
        layout.addView(mResultTextView); // Add the TextView to the layout

        // Set the entire container layout as the activity's content
        setContentView(layout);

        // 4. Set the click listener for the button
        clickButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // This code executes ONLY when the button is clicked
                int sum = Native.add(2, 3);
                // Update the TextView with the result
                mResultTextView.setText("2 + 3 = " + Integer.toString(sum));
            }
        });
    }
}
