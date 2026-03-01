package com.hedge_dev.UnleashedRecomp;

import android.os.Bundle;
import android.os.Build;
import android.view.WindowManager;
import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Optimize window for games
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "main"
        };
    }

    public void setSustainedPerformanceMode(boolean enable) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            getWindow().setSustainedPerformanceMode(enable);
        }
    }
}
