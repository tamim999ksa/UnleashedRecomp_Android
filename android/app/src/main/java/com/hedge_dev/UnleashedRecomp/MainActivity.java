package com.hedge_dev.UnleashedRecomp;

import android.os.Bundle;
import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        System.loadLibrary("main");
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "main"
        };
    }
}
