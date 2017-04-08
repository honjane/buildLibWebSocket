#include "include/libwebsockets.h"

#include <jni.h>
#include <android/log.h>

#define printf(...) __android_log_print(ANDROID_LOG_VERBOSE, "Jni_websocket", ##__VA_ARGS__)

/////////////////////////////////////////////////////////
// Code executed when loading the dynamic link library //
/////////////////////////////////////////////////////////

// The Java class the native functions shall be part of
#define JNIREG_CLASS "com/honjane/websocket/LwsService"

JavaVM *gJvm = NULL;
JNIEnv *gEnv = 0;

JNIEXPORT jboolean JNICALL jni_initLws(JNIEnv *env, jobject obj);

JNIEXPORT void JNICALL jni_exitLws(JNIEnv *env, jobject obj);

JNIEXPORT void JNICALL jni_serviceLws(JNIEnv *env, jobject obj, jint timeout);

JNIEXPORT void JNICALL jni_setConnectionParameters(JNIEnv *env, jobject obj, jstring serverAddress,
                                                   jint serverPort, jstring serverPath);

JNIEXPORT void JNICALL jni_setTimeout(JNIEnv *env, jobject obj, jint timeout);

JNIEXPORT void JNICALL jni_setPingInterval(JNIEnv *env, jobject obj, jint interval);

JNIEXPORT jboolean JNICALL jni_connectLws(JNIEnv *env, jobject obj);

JNIEXPORT jint JNICALL jni_sendMessageLws(JNIEnv *env, jobject obj, jstring jmessage,
                                          jboolean isws);

JNIEXPORT jint JNICALL jni_sendBytes(JNIEnv *env, jobject obj, jbyteArray);

JNIEXPORT jint JNICALL jni_sendChokedLws(JNIEnv *env, jobject obj);

JNIEXPORT void JNICALL jni_setCaCert(JNIEnv *env, jobject obj, jstring ca_path, jstring cert_path,
                                     jstring cert_key);

static JNINativeMethod gMethods[] = {
        {"initLws",                 "()Z",                                                       (void *) jni_initLws},
        {"exitLws",                 "()V",                                                       (void *) jni_exitLws},
        {"serviceLws",              "(I)V",                                                      (void *) jni_serviceLws},
        {"setConnectionParameters", "(Ljava/lang/String;ILjava/lang/String;)V",                  (void *) jni_setConnectionParameters},
        {"setTimeout",              "(I)V",                                                      (void *) jni_setTimeout},
        {"setPingInterval",         "(I)V",                                                      (void *) jni_setPingInterval},
        {"connectLws",              "()Z",                                                       (void *) jni_connectLws},
        {"sendMessageLws",          "(Ljava/lang/String;Z)I",                                    (void *) jni_sendMessageLws},
        {"sendChokedLws",           "()I",                                                       (void *) jni_sendChokedLws},
        {"setCaCert",               "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", (void *) jni_setCaCert},
};

static int registerNativeMethods(JNIEnv *env, const char *className, JNINativeMethod *gMethods,
                                 int numMethods) {
    jclass cls;
    cls = env->FindClass(className);
    if (cls == NULL) {
        return JNI_FALSE;
    }
    if (env->RegisterNatives(cls, gMethods, numMethods) < 0) {
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

static int registerNatives(JNIEnv *env) {
    if (!registerNativeMethods(env, JNIREG_CLASS, gMethods,
                               sizeof(gMethods) / sizeof(gMethods[0]))) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    jint result = -1;

    gJvm = vm;
    if (vm->GetEnv((void **) &gEnv, JNI_VERSION_1_6) != JNI_OK) goto bail;
    if (vm->AttachCurrentThread(&gEnv, NULL) < 0) goto bail;
    if (registerNatives(gEnv) != JNI_TRUE) goto bail;

    result = JNI_VERSION_1_6;

    bail:
    return result;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
    gJvm = NULL;
}

////////////////////////////////////////////////////
// JNI functions to export:                       //
////////////////////////////////////////////////////

static jclass gLwsServiceCls;
static jobject gLwsServiceObj;
static jmethodID receiveMessageId;

#define BUFFER_SIZE 4096

static struct lws_context *context = NULL;
static struct lws_context_creation_info info;
static struct lws *wsi = NULL;

enum websocket_protocols {
    PROTOCOL_DUMB_INCREMENT = 0,
    PROTOCOL_COUNT
};

struct per_session_data { ;// no data
};

static int callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
                    size_t len);

static struct lws_protocols protocols[] = {
        {
                "dumb-increment-protocol",
                callback,
                     sizeof(struct per_session_data),
                BUFFER_SIZE,
        },
        {NULL, NULL, 0, 0} // end of list
};

static const struct lws_extension exts[] = {
        {
                "deflate-frame",
                lws_extension_callback_pm_deflate,
                "deflate_frame"
        },
        {NULL, NULL, NULL}
};

static int timeout_secs = 0;
static int ping_pong_interval = 0;

static int port = 0;
static int use_ssl = 0;
static int use_ssl_client = 0;
static char address[8192];
static const char *path;

static char ca_cert[1024];
static char client_cert[1024];
static char client_cert_key[1024];

static int deny_deflate = 0;
static int deny_mux = 0;
static int is_set_params = 0;

// Logging function for libwebsockets
static void emit_log(int level, const char *msg) {
    printf("%s", msg);
}

JNIEXPORT void JNICALL jni_setTimeout(
        JNIEnv *env,
        jobject obj,
        jint timeout
) {
    timeout_secs = timeout;
    printf("jni_setTimeout: %d s", timeout_secs);
}

JNIEXPORT void JNICALL jni_setPingInterval(
        JNIEnv *env,
        jobject obj,
        jint interval
) {
    ping_pong_interval = interval;
    printf("jni_setPingInterval: %d s", ping_pong_interval);
}


JNIEXPORT void JNICALL jni_setCaCert(
        JNIEnv *env,
        jobject obj,
        jstring ca_path, jstring cert_path, jstring cert_key
) {

    const char *opt_ca = env->GetStringUTFChars(ca_path, 0);
    const char *opt_cert = env->GetStringUTFChars(cert_path, 0);
    const char *opt_cert_key = env->GetStringUTFChars(cert_key, 0);

    printf("jni_setCaCert: opt_ca = %s，opt_cert = %s，opt_cert_key = %s", opt_ca, opt_cert,
           opt_cert_key);

    strncpy(ca_cert, opt_ca, sizeof(ca_cert) - 1);
    ca_cert[sizeof(ca_cert) - 1] = '\0';

    strncpy(client_cert, opt_cert, sizeof(client_cert) - 1);
    client_cert[sizeof(client_cert) - 1] = '\0';

    strncpy(client_cert_key, opt_cert_key, sizeof(client_cert_key) - 1);
    client_cert_key[sizeof(client_cert_key) - 1] = '\0';

    use_ssl = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;

    env->ReleaseStringUTFChars(ca_path, opt_ca);
    env->ReleaseStringUTFChars(cert_path, opt_cert);
    env->ReleaseStringUTFChars(cert_key, opt_cert_key);
}

JNIEXPORT jboolean JNICALL jni_initLws(JNIEnv *env, jobject obj) {
    if (context) {
        printf("jni_initLws complete, context already exist");
        return JNI_FALSE;
    }
    if(!is_set_params){
        return JNI_FALSE;
    }

    // Attach the java virtual machine to this thread
    gJvm->AttachCurrentThread(&gEnv, NULL);

    // Set java global references to the class and object
    jclass cls = env->GetObjectClass(obj);
    gLwsServiceCls = (jclass) env->NewGlobalRef(cls);
    gLwsServiceObj = env->NewGlobalRef(obj);

    // Get the receiveMessage method from the LwsService class (inherited from class ThreadService)
    receiveMessageId = gEnv->GetMethodID(gLwsServiceCls, "receiveMessage",
                                         "(ILjava/lang/Object;)V");

    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
#ifndef LWS_NO_EXTENSIONS
    info.extensions = exts;
#endif
    info.gid = -1;
    info.uid = -1;
    info.timeout_secs = timeout_secs;
    info.ws_ping_pong_interval = ping_pong_interval;

    if (use_ssl) {
        /*
         * If the server wants us to present a valid SSL client certificate
         * then we can set it up here.
         */

        if (client_cert[0])
            info.ssl_cert_filepath = client_cert;
        if (client_cert_key[0])
            info.ssl_private_key_filepath = client_cert_key;

        /*
         * A CA cert and CRL can be used to validate the cert send by the server
         */
        if (ca_cert[0])
            info.ssl_ca_filepath = ca_cert;
    }

    lws_set_log_level(LLL_NOTICE | LLL_INFO | LLL_ERR | LLL_WARN | LLL_CLIENT, emit_log);
    // 初始化长链接
    context = lws_create_context(&info);
    if (context == NULL) {
        printf("jni_initLws failed");
        return JNI_FALSE;
    }

    printf("jni_initLws complete");
    return JNI_TRUE;
}

// 接收服务端消息，回调给java
// (must call jni_initLws() first)
static inline void receiveMessage(int id, jobject obj) {
    gEnv->CallVoidMethod(gLwsServiceObj, receiveMessageId, id, obj);
}

JNIEXPORT void JNICALL jni_exitLws(JNIEnv *env, jobject obj) {
    if (context) {
        lws_context_destroy(context);
        context = NULL;
        env->DeleteGlobalRef(gLwsServiceObj);
        env->DeleteGlobalRef(gLwsServiceCls);
        printf("jni_exitLws, complete");
    } else {
        printf("jni_exitLws, context already null");
    }
}

/**
 * 连接服务器及接收消息回调
 */
static int callback(
        struct lws *wsi,
        enum lws_callback_reasons reason,
        void *user,
        void *in,
        size_t len
) {
    if (reason == LWS_CALLBACK_GET_THREAD_ID) {
        return 0;
    }

    int reasonId = (int) reason;
    jstring inStr = NULL;
    if (in > 0 && len > 0 && reason == LWS_CALLBACK_CLIENT_RECEIVE) {
        ((char *) in)[len] = 0;
        inStr = gEnv->NewStringUTF((const char *) in);
    }
    printf("callback: code=%d", reasonId);
    receiveMessage(reasonId, inStr);

    if (reason == LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED) {
        if ((strcmp((const char *) in, "deflate-stream") == 0) && deny_deflate) {
            printf("callback: denied deflate-stream extension");
            return 1;
        }
        if ((strcmp((const char *) in, "deflate-frame") == 0) && deny_deflate) {
            printf("callback: denied deflate-frame extension");
            return 1;
        }
        if ((strcmp((const char *) in, "x-google-mux") == 0) && deny_mux) {
            printf("callback: denied x-google-mux extension");
            return 1;
        }
    }

    return 0;
}

//挂起长连服务
JNIEXPORT void JNICALL jni_serviceLws(JNIEnv *env, jobject obj, jint timeout) {
    if (context) {
        lws_service(context, timeout);
    }
}

JNIEXPORT void JNICALL jni_setConnectionParameters(
        JNIEnv *env,
        jobject obj,
        jstring jaddress,
        jint jport,
        jstring jpath
) {
    if(!jaddress || !jpath){
         printf("jni_setConnectionParameters , please input right host or path ");
         is_set_params = JNI_FALSE;
         return;
    }
    is_set_params = JNI_TRUE;
    address[0] = 0;
    port = jport;
    if(jpath){
        path = env->GetStringUTFChars(jpath, 0);
    }
    use_ssl = 0;
    use_ssl_client = 0;
    snprintf(address, sizeof(address), "%s", env->GetStringUTFChars(jaddress, 0));
}

JNIEXPORT jboolean JNICALL jni_connectLws(JNIEnv *env, jobject obj) {
    if(!is_set_params){
         printf("jni_connectLws , please input right host or path ");
       return JNI_FALSE;
    }

    struct lws_client_connect_info info_ws;
    memset(&info_ws, 0, sizeof(info_ws));

    info_ws.port = port;
    info_ws.address = address;
    info_ws.path = path;
    info_ws.context = context;
    info_ws.ssl_connection = use_ssl;
    info_ws.host = address;
    info_ws.origin = address;
    info_ws.ietf_version_or_minus_one = -1;
    info_ws.client_exts = exts;
    info_ws.protocol = protocols[PROTOCOL_DUMB_INCREMENT].name;

    printf("jni_connectLws connect");
    // connect 建立连接
    wsi = lws_client_connect_via_info(&info_ws);
    if (wsi == NULL) {
        // Error
        printf("jni_connectLws failed");
        return JNI_FALSE;
    }
    printf("jni_connectLws start");
    return JNI_TRUE;
}


//判断ws连接是否阻塞 ,ws连接阻塞，则返回1，否则返回0

JNIEXPORT jint JNICALL jni_sendChokedLws(JNIEnv *env, jobject obj) {
    if (wsi == NULL) {
        return 1;
    }
    jint jres = lws_send_pipe_choked(wsi);
    return jres;
}

//发送数据到服务端
//wsi 连接对象
//buf 需要发送数据的起始地址
//len 需要发送数据的长度
//protocol  TEXT
//isws true :websocket；false： http
JNIEXPORT jint JNICALL jni_sendMessageLws(JNIEnv *env, jobject obj, jstring jmessage,
                                          jboolean isws) {
    if (wsi == NULL || jmessage == NULL) {
        return JNI_FALSE;
    }

    const char *buf = env->GetStringUTFChars(jmessage, JNI_FALSE);
    if (buf == NULL) {
        env->ReleaseStringUTFChars(jmessage, buf);
        return JNI_FALSE;
    }
    size_t len = env->GetStringUTFLength(jmessage);
    printf("jni_sendMessageLws，len = %d, msg =%s", len, buf);

    int mallocLen = LWS_SEND_BUFFER_PRE_PADDING + len + 1;
    u_char *dest = (u_char *) malloc(mallocLen);
    memset(dest, 0, mallocLen);
    memcpy(dest + LWS_SEND_BUFFER_PRE_PADDING, buf, mallocLen - LWS_SEND_BUFFER_PRE_PADDING);

    jint result;
    if (isws) {
        result = lws_write(wsi, (unsigned char *) (dest + LWS_SEND_BUFFER_PRE_PADDING), len,
                           LWS_WRITE_TEXT);
        printf("lws -> send msg %s", result > 0 ? "success" : "fail");
        goto bail;
    } else {
        result = lws_write_http(wsi, dest + LWS_SEND_BUFFER_PRE_PADDING, len);
        printf("http -> send msg %s", result > 0 ? "success" : "fail");
        goto bail;
    }

    bail :
    env->ReleaseStringUTFChars(jmessage, buf);
    free(dest);
    return result;
}

