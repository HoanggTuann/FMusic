#include <jni.h>
#include <android/log.h>
#include <cstdint>
#include <string>
#include <vector>
#include "Player.h"
#include "AudioDecoder.h"
#include "Equalizer.h"
#include "Lyrics.h"

#define TAG "FMusic"

namespace {

// Chuyển UTF-8 sang jstring qua UTF-16 để tránh lỗi CheckJNI (Modified UTF-8) với emoji 4-byte hoặc tag lỗi encoding.
jstring toJString(JNIEnv* env, const std::string& utf8) {
    if (utf8.empty()) return env->NewString(reinterpret_cast<const jchar*>(u""), 0);
    std::vector<jchar> out;
    out.reserve(utf8.size());
    const auto* s = reinterpret_cast<const uint8_t*>(utf8.data());
    const size_t n = utf8.size();
    size_t i = 0;
    while (i < n) {
        uint32_t cp = 0xFFFD;
        uint8_t c = s[i];
        if (c < 0x80) {
            cp = c;
            i += 1;
        } else if ((c >> 5) == 0x6 && i + 1 < n && (s[i + 1] & 0xC0) == 0x80) {
            cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
            i += 2;
        } else if ((c >> 4) == 0xE && i + 2 < n && (s[i + 1] & 0xC0) == 0x80 && (s[i + 2] & 0xC0) == 0x80) {
            cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
            i += 3;
        } else if ((c >> 3) == 0x1E && i + 3 < n && (s[i + 1] & 0xC0) == 0x80 &&
                   (s[i + 2] & 0xC0) == 0x80 && (s[i + 3] & 0xC0) == 0x80) {
            cp = ((c & 0x07) << 18) | ((s[i + 1] & 0x3F) << 12) | ((s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F);
            i += 4;
        } else {
            i += 1; // bỏ byte hỏng
        }
        if (cp <= 0xFFFF) {
            if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
            out.push_back(static_cast<jchar>(cp));
        } else if (cp <= 0x10FFFF) {
            cp -= 0x10000;
            out.push_back(static_cast<jchar>(0xD800 + (cp >> 10)));
            out.push_back(static_cast<jchar>(0xDC00 + (cp & 0x3FF)));
        } else {
            out.push_back(0xFFFD);
        }
    }
    return env->NewString(out.data(), static_cast<jsize>(out.size()));
}

jobject makeTrackInfoObj(JNIEnv* env, const TrackInfo& ti, const std::vector<uint8_t>* cover) {
    jclass cls = env->FindClass("com/fmusic/TrackInfoData");
    if (!cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return nullptr;
    }
    jmethodID ctor = env->GetMethodID(
        cls, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;DI[B)V"
    );
    if (!ctor) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return nullptr;
    }
    jstring title = toJString(env, ti.title);
    jstring artist = toJString(env, ti.artist);
    jstring album = toJString(env, ti.album);
    jstring lyrics = toJString(env, ti.lyrics);

    jbyteArray coverArr = nullptr;
    if (cover && !cover->empty()) {
        coverArr = env->NewByteArray(static_cast<jsize>(cover->size()));
        if (coverArr) {
            env->SetByteArrayRegion(coverArr, 0, static_cast<jsize>(cover->size()),
                                    reinterpret_cast<const jbyte*>(cover->data()));
        }
    }

    jobject obj = env->NewObject(
        cls, ctor, title, artist, album, lyrics,
        static_cast<jdouble>(ti.duration),
        static_cast<jint>(ti.sampleRate),
        coverArr
    );

    if (title) env->DeleteLocalRef(title);
    if (artist) env->DeleteLocalRef(artist);
    if (album) env->DeleteLocalRef(album);
    if (lyrics) env->DeleteLocalRef(lyrics);
    if (coverArr) env->DeleteLocalRef(coverArr);
    return obj;
}

} // namespace

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
    if (!h || !path) return JNI_FALSE;
    const char* p = env->GetStringUTFChars(path, nullptr);
    bool ok = reinterpret_cast<Player*>(h)->load(p ? p : "");
    if (p) env->ReleaseStringUTFChars(path, p);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobject JNICALL
Java_com_fmusic_MusicPlayerCore_getTrackInfo(JNIEnv* env, jobject, jlong h) {
    if (!h) return nullptr;
    auto* player = reinterpret_cast<Player*>(h);
    return makeTrackInfoObj(env, player->info(), &player->coverArt());
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_play(JNIEnv*, jobject, jlong h) {
    if (h) reinterpret_cast<Player*>(h)->play();
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_pause(JNIEnv*, jobject, jlong h) {
    if (h) reinterpret_cast<Player*>(h)->pause();
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_seekToSeconds(JNIEnv*, jobject, jlong h, jdouble s) {
    if (h) reinterpret_cast<Player*>(h)->seek(s);
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_setSampleRate(JNIEnv*, jobject, jlong h, jint rate) {
    if (h) reinterpret_cast<Player*>(h)->setSampleRate(static_cast<int>(rate));
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_pullInterleaved(JNIEnv* env, jobject, jlong h, jfloatArray buf, jint frames) {
    if (!h || !buf || frames <= 0) return;
    jfloat* p = env->GetFloatArrayElements(buf, nullptr);
    if (p) {
        reinterpret_cast<Player*>(h)->pull(p, static_cast<size_t>(frames));
        env->ReleaseFloatArrayElements(buf, p, 0);
    }
}

JNIEXPORT jfloat JNICALL
Java_com_fmusic_MusicPlayerCore_copySpectrum(JNIEnv* env, jobject, jlong h, jfloatArray bands) {
    if (!h || !bands) return 0.f;
    jfloat* p = env->GetFloatArrayElements(bands, nullptr);
    float bass = 0.f;
    if (p) {
        bass = reinterpret_cast<Player*>(h)->spectrum(p);
        env->ReleaseFloatArrayElements(bands, p, 0);
    }
    return bass;
}

JNIEXPORT jdouble JNICALL
Java_com_fmusic_MusicPlayerCore_getPosition(JNIEnv*, jobject, jlong h) {
    return h ? reinterpret_cast<Player*>(h)->position() : 0.0;
}

JNIEXPORT jdouble JNICALL
Java_com_fmusic_MusicPlayerCore_getDuration(JNIEnv*, jobject, jlong h) {
    return h ? reinterpret_cast<Player*>(h)->duration() : 0.0;
}

JNIEXPORT jboolean JNICALL
Java_com_fmusic_MusicPlayerCore_isPlaying(JNIEnv*, jobject, jlong h) {
    return (h && reinterpret_cast<Player*>(h)->playing()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_fmusic_MusicPlayerCore_isEnded(JNIEnv*, jobject, jlong h) {
    return (h && reinterpret_cast<Player*>(h)->ended()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_setEqEnabled(JNIEnv*, jobject, jlong h, jboolean on) {
    if (h) reinterpret_cast<Player*>(h)->eq().setEnabled(on == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_setEqGain(JNIEnv*, jobject, jlong h, jint band, jfloat db) {
    if (h) reinterpret_cast<Player*>(h)->eq().setGain(band, db);
}

JNIEXPORT void JNICALL
Java_com_fmusic_MusicPlayerCore_applyEqPreset(JNIEnv*, jobject, jlong h, jint idx) {
    if (h) reinterpret_cast<Player*>(h)->eq().applyPreset(idx);
}

JNIEXPORT jfloat JNICALL
Java_com_fmusic_MusicPlayerCore_getEqGain(JNIEnv*, jobject, jlong h, jint band) {
    return h ? reinterpret_cast<Player*>(h)->eq().gain(band) : 0.f;
}

JNIEXPORT jobjectArray JNICALL
Java_com_fmusic_MusicPlayerCore_getEqPresetNames(JNIEnv* env, jclass) {
    jclass strCls = env->FindClass("java/lang/String");
    if (!strCls) return nullptr;
    jobjectArray arr = env->NewObjectArray(Equalizer::kPresetCount, strCls, nullptr);
    for (int i = 0; i < Equalizer::kPresetCount; ++i) {
        const char* name = Equalizer::presetName(i);
        jstring s = toJString(env, name ? name : "");
        env->SetObjectArrayElement(arr, i, s);
        if (s) env->DeleteLocalRef(s);
    }
    return arr;
}

JNIEXPORT jobject JNICALL
Java_com_fmusic_MusicPlayerCore_probePath(JNIEnv* env, jclass, jstring path) {
    if (!path) return nullptr;
    const char* p = env->GetStringUTFChars(path, nullptr);
    TrackInfo ti;
    bool ok = AudioDecoder::probe(p ? p : "", ti);
    if (p) env->ReleaseStringUTFChars(path, p);
    if (!ok) return nullptr;
    return makeTrackInfoObj(env, ti, nullptr);
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
        jstring s = toJString(env, item.text);
        jobject lineObj = env->NewObject(lineCls, lineCtor, static_cast<jdouble>(item.time), s);
        env->CallBooleanMethod(list, listAdd, lineObj);
        if (s) env->DeleteLocalRef(s);
        if (lineObj) env->DeleteLocalRef(lineObj);
    }

    jclass resCls = env->FindClass("com/fmusic/LyricsData");
    if (!resCls) return nullptr;
    jmethodID resCtor = env->GetMethodID(resCls, "<init>", "(ZLjava/util/List;)V");
    return env->NewObject(resCls, resCtor, static_cast<jboolean>(l.synced()), list);
}

} // extern "C"
