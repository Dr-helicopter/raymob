/*
 *  raymob License (MIT)
 *
 *  Copyright (c) 2023-2024 Le Juez Victor
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

package com.raylib.raymob;  // Don't change the package name (see gradle.properties)

import android.app.NativeActivity;
import android.os.Build;
import android.support.annotation.RequiresApi;
import android.util.Log;
import android.view.KeyEvent;


import android.app.Notification;
import android.app.NotificationManager;
import android.app.NotificationChannel;
import android.content.Context;

import android.os.Bundle;
import android.content.res.AssetManager;



public class NativeLoader extends NativeActivity {

    public DisplayManager displayManager;
    public SoftKeyboard softKeyboard;
    public boolean initCallback = false;

    public static NativeActivity instance;
    // Loading method of your native application
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        instance = this;
        Log.i("RAYMOB", "onCreate fired — NativeLoader instance set.");
        listAssets();
        displayManager = new DisplayManager(this);
        softKeyboard = new SoftKeyboard(this);
        System.loadLibrary("raymob");   // Load your game library (don't change raymob, see gradle.properties)
    }





    @RequiresApi(api = Build.VERSION_CODES.O)
    public static void showTestNotification() {
        Log.e("RAYMOB", "showTestNotification fired");

        if (instance == null) {
            Log.e("RAYMOB", "instance is null!");
            return;
        } else {
            Log.e("RAYMOB", "instance is not null!");
        }
        Context ctx = instance;
        NotificationManager manager = (NotificationManager) ctx.getSystemService(Context.NOTIFICATION_SERVICE);

        String channelId = "raymob_channel";
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(channelId, "RaymobChannel", NotificationManager.IMPORTANCE_DEFAULT);
            manager.createNotificationChannel(channel);
        }

        Notification.Builder builder = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                ? new Notification.Builder(ctx, channelId)
                : new Notification.Builder(ctx);

        builder.setContentTitle("Raymob Alert")
                .setContentText("JNI works. Your notification is alive.")
                .setSmallIcon(android.R.drawable.ic_dialog_info)
                .setAutoCancel(true);

        manager.notify(42, builder.build());
    }





    // Handling loss and regain of application focus
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (BuildConfig.FEATURE_DISPLAY_IMMERSIVE && hasFocus) {
            displayManager.setImmersiveMode(); // If the app has focus, re-enable immersive mode
        }
    }

    // Callback methods for managing the Android software keyboard
    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        softKeyboard.onKeyUpEvent(event);
        return super.onKeyDown(keyCode, event);
    }

    @Override
    protected void onStart() {
        super.onStart();
        if(initCallback) {
            onAppStart();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if(initCallback) {
            onAppResume();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if(initCallback) {
            onAppPause();
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        if(initCallback){
            onAppStop();
        }
    }

    private native void onAppStart();
    private native void onAppResume();
    private native void onAppPause();
    private native void onAppStop();

    public void listAssets() {
        AssetManager assetManager = getAssets();
        try {
            String[] files = assetManager.list(""); // "" means root of assets
            for (String file : files) {
                Log.d("AssetList", "Found in assets: " + file);
            }
        } catch (Exception e) {
            Log.e("AssetList", "Error listing assets", e);
        }
    }


}