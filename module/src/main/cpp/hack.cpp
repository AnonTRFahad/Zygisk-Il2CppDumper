//
// Created by Perfare on 2020/7/4.
//

#include "hack.h"
#include "il2cpp_dump.h"
#include "log.h"
#include "xdl.h"
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/system_properties.h>
#include <dlfcn.h>
#include <jni.h>
#include <thread>
#include <sys/mman.h>
#include <linux/unistd.h>
#include <array>
#include <sys/stat.h>
#include "zygisk.hpp"

void hook_il2cpp_string_apis(zygisk::Api *api, void *il2cpp_handle);

// Helper: find libil2cpp.so path from /proc/self/maps
std::string find_libil2cpp_path() {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) return {};
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "libil2cpp.so")) {
            char *path = strchr(line, '/');
            if (path) {
                // Trim newline
                char *nl = strchr(path, '\n');
                if (nl) *nl = 0;
                fclose(fp);
                return std::string(path);
            }
        }
    }
    fclose(fp);
    return {};
}

// Hook impl

#include <cstdlib>
#include <cstring>

typedef const char* (*il2cpp_class_get_name_ftn)(Il2CppClass*);
typedef const char* (*il2cpp_method_get_name_ftn)(const MethodInfo*);
typedef const char* (*il2cpp_field_get_name_ftn)(FieldInfo*);
typedef const char* (*il2cpp_property_get_name_ftn)(PropertyInfo*);

static il2cpp_class_get_name_ftn orig_il2cpp_class_get_name = nullptr;
static il2cpp_method_get_name_ftn orig_il2cpp_method_get_name = nullptr;
static il2cpp_field_get_name_ftn orig_il2cpp_field_get_name = nullptr;
static il2cpp_property_get_name_ftn orig_il2cpp_property_get_name = nullptr;

const char* my_il2cpp_class_get_name(Il2CppClass *klass) {
    auto name = orig_il2cpp_class_get_name(klass);
    if (!name || strlen(name) < 3) return name;
    size_t len = strlen(name);
    char *dec = (char*)malloc(len + 1);
    for (size_t i = 0; i < len; ++i) dec[i] = name[i] ^ 0x42;
    dec[len] = 0;
    return dec;
}

const char* my_il2cpp_method_get_name(const MethodInfo *method) {
    auto name = orig_il2cpp_method_get_name(method);
    if (!name || strlen(name) < 3) return name;
    size_t len = strlen(name);
    char *dec = (char*)malloc(len + 1);
    for (size_t i = 0; i < len; ++i) dec[i] = name[i] ^ 0x42;
    dec[len] = 0;
    return dec;
}

const char* my_il2cpp_field_get_name(FieldInfo *field) {
    auto name = orig_il2cpp_field_get_name(field);
    if (!name || strlen(name) < 3) return name;
    size_t len = strlen(name);
    char *dec = (char*)malloc(len + 1);
    for (size_t i = 0; i < len; ++i) dec[i] = name[i] ^ 0x42;
    dec[len] = 0;
    return dec;
}

const char* my_il2cpp_property_get_name(PropertyInfo *prop) {
    auto name = orig_il2cpp_property_get_name(prop);
    if (!name || strlen(name) < 3) return name;
    size_t len = strlen(name);
    char *dec = (char*)malloc(len + 1);
    for (size_t i = 0; i < len; ++i) dec[i] = name[i] ^ 0x42;
    dec[len] = 0;
    return dec;
}

void hook_il2cpp_string_apis(zygisk::Api *api, void *il2cpp_handle) {
    if (!api || !il2cpp_handle) return;
    api->pltHookRegister("libil2cpp.so", "il2cpp_class_get_name", (void*)my_il2cpp_class_get_name, (void**)&orig_il2cpp_class_get_name);
    api->pltHookRegister("libil2cpp.so", "il2cpp_method_get_name", (void*)my_il2cpp_method_get_name, (void**)&orig_il2cpp_method_get_name);
    api->pltHookRegister("libil2cpp.so", "il2cpp_field_get_name", (void*)my_il2cpp_field_get_name, (void**)&orig_il2cpp_field_get_name);
    api->pltHookRegister("libil2cpp.so", "il2cpp_property_get_name", (void*)my_il2cpp_property_get_name, (void**)&orig_il2cpp_property_get_name);
    api->pltHookCommit();
}

void hack_start(const char *game_data_dir, zygisk::Api *api = nullptr) {
    bool load = false;
    FILE *dump_log = nullptr;
    auto log_path = std::string(game_data_dir) + "/zygisk_dumper.log";
    dump_log = fopen(log_path.c_str(), "a");
    if (dump_log) {
        time_t now = time(nullptr);
        char tstr[64];
        strftime(tstr, sizeof(tstr), "%Y-%m-%d %H:%M:%S", localtime(&now));
        fprintf(dump_log, "\n[%s] START DUMP SESSION\n", tstr);
        fclose(dump_log);
    }

    auto lib_path = find_libil2cpp_path();
    if (!lib_path.empty()) {
        struct stat st;
        if (stat(lib_path.c_str(), &st) == 0) {
            uint64_t mb = (st.st_size + 1024*1024 - 1) / (1024*1024);
            LOGI("Found libil2cpp.so: %s (%" PRIu64 " MB)", lib_path.c_str(), mb);
            if (dump_log = fopen(log_path.c_str(), "a")) {
                fprintf(dump_log, "Found libil2cpp.so: %s (%" PRIu64 " MB)\n", lib_path.c_str(), mb);
                fclose(dump_log);
            }
        } else {
            LOGI("Found libil2cpp.so: %s (size unknown)", lib_path.c_str());
            if (dump_log = fopen(log_path.c_str(), "a")) {
                fprintf(dump_log, "Found libil2cpp.so: %s (size unknown)\n", lib_path.c_str());
                fclose(dump_log);
            }
        }
    } else {
        LOGI("libil2cpp.so not found in /proc/self/maps yet");
        if (dump_log = fopen(log_path.c_str(), "a")) {
            fprintf(dump_log, "libil2cpp.so not found in /proc/self/maps yet\n");
            fclose(dump_log);
        }
    }

    for (int i = 0; i < 10; i++) {
        void *handle = xdl_open("libil2cpp.so", 0);
        if (handle) {
            load = true;
            LOGI("il2cpp_handle = %p", handle);
            il2cpp_api_init(handle);
            LOGI("il2cpp APIs initialized");
            hook_il2cpp_string_apis(api, handle);
            LOGI("il2cpp string hooks installed");
            il2cpp_dump(game_data_dir);
            LOGI("dump completed");
            break;
        } else {
            LOGI("Attempt %d: libil2cpp.so not loaded yet... waiting 1s", i+1);
            sleep(1);
        }
    }
    if (!load) {
        LOGI("libil2cpp.so not found in thread %d", gettid());
    }
}

std::string GetLibDir(JavaVM *vms) {
    JNIEnv *env = nullptr;
    vms->AttachCurrentThread(&env, nullptr);
    jclass activity_thread_clz = env->FindClass("android/app/ActivityThread");
    if (activity_thread_clz != nullptr) {
        jmethodID currentApplicationId = env->GetStaticMethodID(activity_thread_clz,
                                                                "currentApplication",
                                                                "()Landroid/app/Application;");
        if (currentApplicationId) {
            jobject application = env->CallStaticObjectMethod(activity_thread_clz,
                                                              currentApplicationId);
            jclass application_clazz = env->GetObjectClass(application);
            if (application_clazz) {
                jmethodID get_application_info = env->GetMethodID(application_clazz,
                                                                  "getApplicationInfo",
                                                                  "()Landroid/content/pm/ApplicationInfo;");
                if (get_application_info) {
                    jobject application_info = env->CallObjectMethod(application,
                                                                     get_application_info);
                    jfieldID native_library_dir_id = env->GetFieldID(
                            env->GetObjectClass(application_info), "nativeLibraryDir",
                            "Ljava/lang/String;");
                    if (native_library_dir_id) {
                        auto native_library_dir_jstring = (jstring) env->GetObjectField(
                                application_info, native_library_dir_id);
                        auto path = env->GetStringUTFChars(native_library_dir_jstring, nullptr);
                        LOGI("lib dir %s", path);
                        std::string lib_dir(path);
                        env->ReleaseStringUTFChars(native_library_dir_jstring, path);
                        return lib_dir;
                    } else {
                        LOGE("nativeLibraryDir not found");
                    }
                } else {
                    LOGE("getApplicationInfo not found");
                }
            } else {
                LOGE("application class not found");
            }
        } else {
            LOGE("currentApplication not found");
        }
    } else {
        LOGE("ActivityThread not found");
    }
    return {};
}

static std::string GetNativeBridgeLibrary() {
    auto value = std::array<char, PROP_VALUE_MAX>();
    __system_property_get("ro.dalvik.vm.native.bridge", value.data());
    return {value.data()};
}

struct NativeBridgeCallbacks {
    uint32_t version;
    void *initialize;

    void *(*loadLibrary)(const char *libpath, int flag);

    void *(*getTrampoline)(void *handle, const char *name, const char *shorty, uint32_t len);

    void *isSupported;
    void *getAppEnv;
    void *isCompatibleWith;
    void *getSignalHandler;
    void *unloadLibrary;
    void *getError;
    void *isPathSupported;
    void *initAnonymousNamespace;
    void *createNamespace;
    void *linkNamespaces;

    void *(*loadLibraryExt)(const char *libpath, int flag, void *ns);
};

bool NativeBridgeLoad(const char *game_data_dir, int api_level, void *data, size_t length) {
    //TODO 等待houdini初始化
    sleep(5);

    auto libart = dlopen("libart.so", RTLD_NOW);
    auto JNI_GetCreatedJavaVMs = (jint (*)(JavaVM **, jsize, jsize *)) dlsym(libart,
                                                                             "JNI_GetCreatedJavaVMs");
    LOGI("JNI_GetCreatedJavaVMs %p", JNI_GetCreatedJavaVMs);
    JavaVM *vms_buf[1];
    JavaVM *vms;
    jsize num_vms;
    jint status = JNI_GetCreatedJavaVMs(vms_buf, 1, &num_vms);
    if (status == JNI_OK && num_vms > 0) {
        vms = vms_buf[0];
    } else {
        LOGE("GetCreatedJavaVMs error");
        return false;
    }

    auto lib_dir = GetLibDir(vms);
    if (lib_dir.empty()) {
        LOGE("GetLibDir error");
        return false;
    }
    if (lib_dir.find("/lib/x86") != std::string::npos) {
        LOGI("no need NativeBridge");
        munmap(data, length);
        return false;
    }

    auto nb = dlopen("libhoudini.so", RTLD_NOW);
    if (!nb) {
        auto native_bridge = GetNativeBridgeLibrary();
        LOGI("native bridge: %s", native_bridge.data());
        nb = dlopen(native_bridge.data(), RTLD_NOW);
    }
    if (nb) {
        LOGI("nb %p", nb);
        auto callbacks = (NativeBridgeCallbacks *) dlsym(nb, "NativeBridgeItf");
        if (callbacks) {
            LOGI("NativeBridgeLoadLibrary %p", callbacks->loadLibrary);
            LOGI("NativeBridgeLoadLibraryExt %p", callbacks->loadLibraryExt);
            LOGI("NativeBridgeGetTrampoline %p", callbacks->getTrampoline);

            int fd = syscall(__NR_memfd_create, "anon", MFD_CLOEXEC);
            ftruncate(fd, (off_t) length);
            void *mem = mmap(nullptr, length, PROT_WRITE, MAP_SHARED, fd, 0);
            memcpy(mem, data, length);
            munmap(mem, length);
            munmap(data, length);
            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "/proc/self/fd/%d", fd);
            LOGI("arm path %s", path);

            void *arm_handle;
            if (api_level >= 26) {
                arm_handle = callbacks->loadLibraryExt(path, RTLD_NOW, (void *) 3);
            } else {
                arm_handle = callbacks->loadLibrary(path, RTLD_NOW);
            }
            if (arm_handle) {
                LOGI("arm handle %p", arm_handle);
                auto init = (void (*)(JavaVM *, void *)) callbacks->getTrampoline(arm_handle,
                                                                                  "JNI_OnLoad",
                                                                                  nullptr, 0);
                LOGI("JNI_OnLoad %p", init);
                init(vms, (void *) game_data_dir);
                return true;
            }
            close(fd);
        }
    }
    return false;
}

void hack_prepare(zygisk::Api *api, const char *game_data_dir, void *data, size_t length) {
    LOGI("hack thread: %d", gettid());
    int api_level = android_get_device_api_level();
    LOGI("api level: %d", api_level);

#if defined(__i386__) || defined(__x86_64__)
    if (!NativeBridgeLoad(game_data_dir, api_level, data, length)) {
#endif
        hack_start(game_data_dir, api);
#if defined(__i386__) || defined(__x86_64__)
    }
#endif
}

#if defined(__arm__) || defined(__aarch64__)

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    auto game_data_dir = (const char *) reserved;
    std::thread hack_thread(hack_start, game_data_dir);
    hack_thread.detach();
    return JNI_VERSION_1_6;
}

#endif
