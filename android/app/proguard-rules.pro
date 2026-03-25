# ProGuard/R8 rules for Unleashed Recompiled (Android)

# Keep the SDL2 Java classes used via JNI from native code
-keep class org.libsdl.app.** { *; }

# Keep our MainActivity (called from SDL and contains JNI methods)
-keep class com.hedge_dev.UnleashedRecomp.MainActivity { *; }

# Keep all native method declarations
-keepclasseswithmembers class * {
    native <methods>;
}

# Keep classes referenced from AndroidManifest.xml
-keep public class * extends android.app.Activity
-keep public class * extends android.app.Application
-keep public class * extends android.app.Service

# Prevent stripping of JNI-called methods in SDL
-keepclassmembers class org.libsdl.app.SDLActivity {
    <methods>;
}
-keepclassmembers class org.libsdl.app.SDLAudioManager {
    <methods>;
}
-keepclassmembers class org.libsdl.app.SDLControllerManager {
    <methods>;
}
-keepclassmembers class org.libsdl.app.HIDDeviceManager {
    <methods>;
}
