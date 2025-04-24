# Flutter wrapper
-keep class io.flutter.app.** { *; }
-keep class io.flutter.plugin.** { *; }
-keep class io.flutter.util.** { *; }
-keep class io.flutter.view.** { *; }
-keep class io.flutter.** { *; }
-keep class io.flutter.plugins.** { *; }

# Keep your application classes that will be used by ObjectBox
-keep class com.example.test.** { *; }

# BLE related
-keep class com.polidea.rxandroidble.** { *; }
-keep class com.polidea.rxandroidble2.** { *; }

# Keep native methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep Parcelables
-keepclassmembers class * implements android.os.Parcelable {
    static ** CREATOR;
}

# Keep custom exceptions
-keep public class * extends java.lang.Exception

# Keep the support library
-keep class androidx.** { *; }
-keep interface androidx.** { *; }
-keep class * extends androidx.** { *; }
-keep class * implements androidx.** { *; }

# Keep the Android Support Library
-keep class android.support.** { *; }
-keep interface android.support.** { *; }
-keep class * extends android.support.** { *; }
-keep class * implements android.support.** { *; }

# Keep the AndroidX Library
-keep class androidx.** { *; }
-keep interface androidx.** { *; }
-keep class * extends androidx.** { *; }
-keep class * implements androidx.** { *; }

# Keep the Material Design Library
-keep class com.google.android.material.** { *; }
-keep interface com.google.android.material.** { *; }
-keep class * extends com.google.android.material.** { *; }
-keep class * implements com.google.android.material.** { *; } 