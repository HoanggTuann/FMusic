#include <jni.h>
#include <android/log.h>
#include "Player.h"
#include "AudioDecoder.h"
#include "Lyrics.h"
#define TAG "FMusic"

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_fmusic_MusicPlayerCore_create(JNIEnv*, jobject) {
    return reinterpret_cast<jlong>(new Player());
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_destroy(JNIEnv*, jobject, jlong h) {
    delete reinterpret_cast<Player*>(h);
}

JNIEXPORT jboolean JNICALL
Java_com_fmusic_MusicPlayerCore_loadPath(JNIEnv* env, jobject, jlong h, jstring path) {
    const char* p = env->GetStringUTFChars(path, nullptr);
    bool ok = reinterpret_cast<Player*>(h)->load(p);
    env->ReleaseStringUTFChars(path, p);
    return ok;
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_play(JNIEnv*, jobject, jlong h) {
    reinterpret_cast<Player*>(h)->play();
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_pause(JNIEnv*, jobject, jlong h) {
    reinterpret_cast<Player*>(h)->pause();
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_seekToSeconds(JNIEnv*, jobject, jlong h, jdouble s) {
    reinterpret_cast<Player*>(h)->seek(s);
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_pullInterleaved(JNIEnv* env, jobject, jlong h, jfloatArray buf, jint frames) {
    jfloat* p = env->GetFloatArrayElements(buf, nullptr);
    reinterpret_cast<Player*>(h)->pull(p, frames);
    env->ReleaseFloatArrayElements(buf, p, 0);
}

JNIEXPORT jfloat JNICALL
Java_com_fmusic_MusicPlayerCore_copySpectrum(JNIEnv* env, jobject, jlong h, jfloatArray bands) {
    jfloat* p = env->GetFloatArrayElements(bands, nullptr);
    float bass = reinterpret_cast<Player*>(h)->spectrum(p);
    env->ReleaseFloatArrayElements(bands, p, 0);
    return bass;
}

JNIEXPORT jdouble JNICALL
Java_com_fmusic_MusicPlayerCore_getPosition(JNIEnv*, jobject, jlong h) {
    return reinterpret_cast<Player*>(h)->position();
}

JNIEXPORT jdouble JNICALL
Java_com_fmusic_MusicPlayerCore_getDuration(JNIEnv*, jobject, jlong h) {
    return reinterpret_cast<Player*>(h)->duration();
}

JNIEXPORT jboolean JNICALL
Java_com_fmusic_MusicPlayerCore_isPlaying(JNIEnv*, jobject, jlong h) {
    return reinterpret_cast<Player*>(h)->playing();
}

JNIEXPORT jboolean JNICALL
Java_com_fmusic_MusicPlayerCore_isEnded(JNIEnv*, jobject, jlong h) {
    return reinterpret_cast<Player*>(h)->ended();
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_setEqEnabled(JNIEnv*, jobject, jlong h, jboolean on) {
    reinterpret_cast<Player*>(h)->eq().setEnabled(on);
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_setEqGain(JNIEnv*, jobject, jlong h, jint band, jfloat db) {
    reinterpret_cast<Player*>(h)->eq().setGain(band, db);
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_applyEqPreset(JNIEnv*, jobject, jlong h, jint idx) {
    reinterpret_cast<Player*>(h)->eq().applyPreset(idx);
}

JNIEXPORT jfloat JNICALL
Java_com_fmusic_MusicPlayerCore_getEqGain(JNIEnv*, jobject, jlong h, jint band) {
    return reinterpret_cast<Player*>(h)->eq().gain(band);
}

} // extern "C"
