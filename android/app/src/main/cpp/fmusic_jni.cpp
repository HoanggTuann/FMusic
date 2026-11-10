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

JNIEXPORT jobject JNICALL
Java_com_fmusic_MusicPlayerCore_probePath(JNIEnv* env, jclass, jstring path) {
    if (!path) return nullptr;
    const char* p = env->GetStringUTFChars(path, nullptr);
    TrackInfo ti;
    bool ok = AudioDecoder::probe(p ? p : "", ti);
    if (p) env->ReleaseStringUTFChars(path, p);
    if (!ok) return nullptr;

    jclass cls = env->FindClass("com/fmusic/TrackInfoData");
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;DIL[B)V");
    if (!ctor) return nullptr;
    jstring title = env->NewStringUTF(ti.title.c_str());
    jstring artist = env->NewStringUTF(ti.artist.c_str());
    jstring album = env->NewStringUTF(ti.album.c_str());
    jobject obj = env->NewObject(cls, ctor, title, artist, album, (jdouble)ti.duration, (jint)ti.sampleRate, nullptr);
    if (title) env->DeleteLocalRef(title);
    if (artist) env->DeleteLocalRef(artist);
    if (album) env->DeleteLocalRef(album);
    return obj;
}

JNIEXPORT jobject JNICALL
Java_com_fmusic_MusicPlayerCore_parseLyrics(JNIEnv* env, jclass, jstring text) {
    Lyrics l;
    if (text) {
        const char* t = env->GetStringUTFChars(text, nullptr);
        if (t) {
            l.parse(t);
            env->ReleaseStringUTFChars(text, t);
        }
    }

    jclass listCls = env->FindClass("java/util/ArrayList");
    if (!listCls) return nullptr;
    jmethodID listCtor = env->GetMethodID(listCls, "<init>", "()V");
    jmethodID listAdd = env->GetMethodID(listCls, "add", "(Ljava/lang/Object;)Z");
    jobject list = env->NewObject(listCls, listCtor);

    jclass lineCls = env->FindClass("com/fmusic/LyricLineData");
    if (!lineCls) return nullptr;
    jmethodID lineCtor = env->GetMethodID(lineCls, "<init>", "(DLjava/lang/String;)V");

    for (const auto& item : l.lines()) {
        jstring s = env->NewStringUTF(item.text.c_str());
        jobject lineObj = env->NewObject(lineCls, lineCtor, (jdouble)item.time, s);
        env->CallBooleanMethod(list, listAdd, lineObj);
        if (s) env->DeleteLocalRef(s);
        if (lineObj) env->DeleteLocalRef(lineObj);
    }

    jclass resCls = env->FindClass("com/fmusic/LyricsData");
    if (!resCls) return nullptr;
    jmethodID resCtor = env->GetMethodID(resCls, "<init>", "(ZLjava/util/List;)V");
    return env->NewObject(resCls, resCtor, (jboolean)l.synced(), list);
}

} // extern "C"
