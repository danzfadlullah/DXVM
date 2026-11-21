/*
 * JNI bridge for the Vulkan capability suite.
 *
 * Deliberately thin: the C side reports what the driver said (a return code and a
 * detail string), and Kotlin decides how that becomes a DXML line and what the
 * device identity *means* (system driver vs PanVK). Keeping the verdict in one
 * language means the format cannot drift between the host script and the app.
 */
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Provided by vktests.c (built into this library with VKTESTS_NO_MAIN). */
int vktests_run(const char *name, char *detail, int detail_len);
int vktests_identity(char *out, int out_len);
int vktests_count(void);
const char *vktests_name(int i);

#define DETAIL_MAX 512
#define IDENTITY_MAX 1024

static jstring to_jstring(JNIEnv *env, const char *s) {
  return (*env)->NewStringUTF(env, s ? s : "");
}

/* "rc|detail" — the caller splits on the first '|'. */
JNIEXPORT jstring JNICALL
Java_dev_zenithblue_panvklauncher_VkTests_runNative(JNIEnv *env, jobject thiz, jstring name) {
  char out[DETAIL_MAX + 16];
  char detail[DETAIL_MAX];
  const char *cname;
  int rc;

  (void)thiz;
  memset(detail, 0, sizeof(detail));
  cname = name ? (*env)->GetStringUTFChars(env, name, NULL) : NULL;
  if (cname == NULL) {
    snprintf(out, sizeof(out), "2|no test name");
    return to_jstring(env, out);
  }

  rc = vktests_run(cname, detail, (int)sizeof(detail));
  (*env)->ReleaseStringUTFChars(env, name, cname);

  snprintf(out, sizeof(out), "%d|%s", rc, detail);
  return to_jstring(env, out);
}

JNIEXPORT jstring JNICALL
Java_dev_zenithblue_panvklauncher_VkTests_identityNative(JNIEnv *env, jobject thiz) {
  char out[IDENTITY_MAX];
  (void)thiz;
  memset(out, 0, sizeof(out));
  vktests_identity(out, (int)sizeof(out));
  return to_jstring(env, out);
}

JNIEXPORT jobjectArray JNICALL
Java_dev_zenithblue_panvklauncher_VkTests_namesNative(JNIEnv *env, jobject thiz) {
  int n = vktests_count();
  jclass str = (*env)->FindClass(env, "java/lang/String");
  jobjectArray arr;
  int i;

  (void)thiz;
  if (n < 0) n = 0;
  arr = (*env)->NewObjectArray(env, n, str, NULL);
  if (arr == NULL) return NULL;
  for (i = 0; i < n; i++) {
    const char *name = vktests_name(i);
    if (name) {
      jstring s = to_jstring(env, name);
      (*env)->SetObjectArrayElement(env, arr, i, s);
      (*env)->DeleteLocalRef(env, s);
    }
  }
  return arr;
}