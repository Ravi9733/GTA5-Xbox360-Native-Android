plugins {
    id("com.android.application")
}

android {
    namespace = "com.gta5.nativeandroid"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.gta5.nativeandroid"
        minSdk = 29
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0-dev"
        ndk { abiFilters += listOf("arm64-v8a") }
        externalNativeBuild {
            cmake { cppFlags += listOf("-std=c++20") }
        }
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    buildTypes {
        release { isMinifyEnabled = false }
    }
}
