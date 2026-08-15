/*
 * java.c
 *
 * Fake JNI environment and bindings for DESTINIA PS Vita port.
 */

#include "java.h"
#include "audio.h"

#include <falso_jni/FalsoJNI_Impl.h>
#include <falso_jni/FalsoJNI.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "utils/logger.h"

enum MethodIDs {
    UNKNOWN = 0,
    GET_ASSET_RES_SIZE,
    GET_ASSET_RES,
    PLAY_SOUND,
    SET_SOUND_VOLUME,
    PLAY_VIB,
    GOTO_URL,
    LOG_EVENT,
    COMM_CLOSE,
    COMM_BUY_CASH
};

static int destinia_resolve_asset_path(const char *name, char *out, size_t out_size) {
    if (!name || !out) return 0;

    snprintf(out, out_size, "ux0:data/destinia/assets/%s", name);
    if (access(out, F_OK) == 0) return 1;

    snprintf(out, out_size, "ux0:data/destinia/%s", name);
    if (access(out, F_OK) == 0) return 1;

    snprintf(out, out_size, "ux0:data/destinia/sound/%s", name);
    if (access(out, F_OK) == 0) return 1;

    return 0;
}

static int get_string_from_jbytearray(jbyteArray arr, char *out, size_t out_size) {
    if (!arr || !out || out_size == 0) return 0;

    jsize len = jni->GetArrayLength(&jni, arr);
    if (len <= 0) {
        out[0] = '\0';
        return 0;
    }

    jbyte *elems = jni->GetByteArrayElements(&jni, arr, NULL);
    if (!elems) {
        out[0] = '\0';
        return 0;
    }

    size_t copy_len = (size_t)len < (out_size - 1) ? (size_t)len : (out_size - 1);
    memcpy(out, elems, copy_len);
    out[copy_len] = '\0';

    jni->ReleaseByteArrayElements(&jni, arr, elems, JNI_ABORT);
    return (int)copy_len;
}

static jint Java_getAssetResSize(jmethodID id, va_list args) {
    jbyteArray nameArr = va_arg(args, jbyteArray);
    char fname[128];
    if (!get_string_from_jbytearray(nameArr, fname, sizeof(fname))) {
        l_warn("[Java] getAssetResSize: empty or invalid name");
        return 0;
    }

    char path[256];
    if (!destinia_resolve_asset_path(fname, path, sizeof(path))) {
        l_warn("[Java] getAssetResSize: file not found: %s", fname);
        return 0;
    }

    struct stat st;
    if (stat(path, &st) == 0) {
        l_debug("[Java] getAssetResSize(%s) = %ld bytes", fname, (long)st.st_size);
        return (jint)st.st_size;
    }

    return 0;
}

static jbyteArray s_last_asset_res = NULL;

static jobject Java_getAssetRes(jmethodID id, va_list args) {
    if (s_last_asset_res) {
        jda_free((JavaDynArray *)s_last_asset_res);
        s_last_asset_res = NULL;
    }

    jbyteArray nameArr = va_arg(args, jbyteArray);
    char fname[128];
    if (!get_string_from_jbytearray(nameArr, fname, sizeof(fname))) {
        l_warn("[Java] getAssetRes: empty or invalid name");
        return NULL;
    }

    char path[256];
    if (!destinia_resolve_asset_path(fname, path, sizeof(path))) {
        l_warn("[Java] getAssetRes: file not found: %s", fname);
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        l_warn("[Java] getAssetRes: failed to open: %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return NULL;
    }

    jbyteArray ret = jni->NewByteArray(&jni, (jsize)size);
    if (!ret) {
        l_error("[Java] getAssetRes: NewByteArray failed for %s (%ld bytes)", fname, size);
        fclose(f);
        return NULL;
    }

    jbyte *buf = jni->GetByteArrayElements(&jni, ret, NULL);
    if (buf) {
        fread(buf, 1, size, f);
        jni->ReleaseByteArrayElements(&jni, ret, buf, 0);
    }
    fclose(f);

    l_debug("[Java] getAssetRes(%s): loaded %ld bytes", fname, size);
    s_last_asset_res = ret;
    return (jobject)ret;
}

static void Java_playSound(jmethodID id, va_list args) {
    jint soundIdx = va_arg(args, jint);
    l_debug("[Java] playSound(%d)", soundIdx);
    audio_play_sound((int)soundIdx);
}

static void Java_setSoundVolume(jmethodID id, va_list args) {
    jint soundType = va_arg(args, jint);
    jint cur = va_arg(args, jint);
    jint max = va_arg(args, jint);
    l_debug("[Java] setSoundVolume(type=%d, cur=%d, max=%d)", soundType, cur, max);
    audio_set_volume((int)soundType, (int)cur, (int)max);
}

static void Java_playVib(jmethodID id, va_list args) {
    jint timems = va_arg(args, jint);
    l_debug("[Java] playVib(%d ms)", timems);
}

static void Java_gotoURL(jmethodID id, va_list args) {
    jbyteArray urlArr = va_arg(args, jbyteArray);
    char url[256];
    get_string_from_jbytearray(urlArr, url, sizeof(url));
    l_info("[Java] gotoURL(%s)", url);
}

static void Java_logEvent(jmethodID id, va_list args) {
    jbyteArray eventArr = va_arg(args, jbyteArray);
    char ev[128];
    get_string_from_jbytearray(eventArr, ev, sizeof(ev));
    l_debug("[Java] logEvent(%s)", ev);
}

static void Java_close(jmethodID id, va_list args) {
    l_debug("[Java] Comm close called");
}

static void Java_buyCash(jmethodID id, va_list args) {
    jint amount = va_arg(args, jint);
    l_info("[Java] Comm buyCash(%d)", amount);
}

/*
 * JNI Method Registration Table
 */

NameToMethodID nameToMethodId[] = {
    { GET_ASSET_RES_SIZE, "getAssetResSize" },
    { GET_ASSET_RES,      "getAssetRes" },
    { PLAY_SOUND,         "playSound" },
    { SET_SOUND_VOLUME,   "setSoundVolume" },
    { PLAY_VIB,           "playVib" },
    { GOTO_URL,           "gotoURL" },
    { LOG_EVENT,          "logEvent" },
    { COMM_CLOSE,         "close" },
    { COMM_BUY_CASH,      "buyCash" },
};

MethodsBoolean methodsBoolean[] = {};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};

MethodsInt methodsInt[] = {
    { GET_ASSET_RES_SIZE, Java_getAssetResSize },
};

MethodsLong methodsLong[] = {};

MethodsObject methodsObject[] = {
    { GET_ASSET_RES, Java_getAssetRes },
};

MethodsShort methodsShort[] = {};

MethodsVoid methodsVoid[] = {
    { PLAY_SOUND,       Java_playSound },
    { SET_SOUND_VOLUME, Java_setSoundVolume },
    { PLAY_VIB,         Java_playVib },
    { GOTO_URL,         Java_gotoURL },
    { LOG_EVENT,        Java_logEvent },
    { COMM_CLOSE,       Java_close },
    { COMM_BUY_CASH,    Java_buyCash },
};

/*
 * JNI Fields
 */

char WINDOW_SERVICE[] = "window";
const int SDK_INT = 19; // Android 4.4 / KitKat

NameToFieldID nameToFieldId[] = {
    { 0, "WINDOW_SERVICE", FIELD_TYPE_OBJECT },
    { 1, "SDK_INT", FIELD_TYPE_INT },
};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {
    { 1, SDK_INT },
};
FieldsObject fieldsObject[] = {
    { 0, WINDOW_SERVICE },
};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES

void destinia_java_init(void) {
    jni_init();
    l_info("[Java] FalsoJNI environment initialized.");
}
