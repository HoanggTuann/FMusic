plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "com.fmusic"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.fmusic"
        minSdk = 26
        targetSdk = 35
        versionCode = 11
        versionName = "1.1"
        externalNativeBuild {
            cmake { cppFlags("-std=c++17") }
        }
    }

    buildFeatures { compose = true }

    externalNativeBuild {
        cmake {
            // CMakeLists.txt lives at android/CMakeLists.txt (project root of android/)
            path("CMakeLists.txt")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    kotlinOptions { jvmTarget = "11" }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.material3)

    // ViewModel for Compose
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.7")

    // MediaSession / MediaCompat support
    implementation("androidx.media:media:1.7.0")

    // Coil for image loading + BlurTransformation for ambient blur backgrounds
    implementation("io.coil-kt:coil-compose:2.7.0")
}
