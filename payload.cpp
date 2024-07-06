// g++ -DDEBUG_LOGGING -shared -o "C:\Users\user\Desktop\payload.dll" payload.cpp -I"C:\Program Files\Eclipse Adoptium\jdk-11.0.23.9-hotspot\include" -I"C:\Program Files\Eclipse Adoptium\jdk-11.0.23.9-hotspot\include\win32" -L"C:\Program Files\Eclipse Adoptium\jdk-11.0.23.9-hotspot\lib" -ljvm

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <memory>
#include <jni.h>
#include <jvmti.h>
#include <cstdlib>
#include <string>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

std::string get_user_home_path()
{
    const char *home_dir = std::getenv("USERPROFILE");
    if (!home_dir)
    {
        // Fallback to "HOME" environment variable if "USERPROFILE" is not set
        home_dir = std::getenv("HOME");
    }

    if (!home_dir)
    {
        throw std::runtime_error("Unable to determine user's home directory");
    }

    return std::string(home_dir);
}

std::filesystem::path class_file_path = std::filesystem::path(get_user_home_path()) / "Desktop" / "MyClass.class";
const std::string CLASS_FILE_PATH = class_file_path.string();

#ifdef DEBUG_LOGGING
class Logger
{
public:
    Logger(const std::string &filename) : logFile(filename, std::ios::app)
    {
        if (!logFile.is_open())
        {
            throw std::runtime_error("Unable to open log file: " + filename);
        }
    }

    template <typename T>
    void log(const T &message)
    {
        std::stringstream ss;
        ss << getCurrentTimestamp() << " - " << message << std::endl;
        std::cout << ss.str();
        logFile << ss.str();
        logFile.flush();
    }

    template <typename... Args>
    void logf(const char *format, Args... args)
    {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format, args...);
        log(std::string(buffer));
    }

private:
    std::ofstream logFile;

    std::string getCurrentTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%X");
        return ss.str();
    }
};

// Global logger instance
Logger logger((std::filesystem::path(get_user_home_path()) / "Desktop" / "log.txt").string());

#define LOG(message) logger.log(message)
#define LOGF(format, ...) logger.logf(format, __VA_ARGS__)
#else
#define LOG(message)
#define LOGF(format, ...)
#endif

// redefine_classes

void redefine_classes(jvmtiEnv *jvmtiEnv, JNIEnv *jniEnv)
{
    LOG("redifine_classes");
    // Load the new class bytes from a file
    std::unique_ptr<FILE, decltype(&fclose)> file(fopen(CLASS_FILE_PATH.c_str(), "rb"), &fclose);
    if (!file)
    {
        LOG("Failed to open the class file");
        return;
    }

    // Add the can_redefine_classes capability
    jvmtiCapabilities capabilities;
    memset(&capabilities, 0, sizeof(capabilities));
    capabilities.can_redefine_classes = 1;

    jint result = jvmtiEnv->AddCapabilities(&capabilities);
    if (result != JVMTI_ERROR_NONE)
    {
        LOGF("AddCapabilities failed: %d", result);
        return;
    }

    // Get the jclass object for the class
    jclass myClass = jniEnv->FindClass("org/example/MyClass");
    if (myClass == nullptr)
    {
        LOG("Failed to find the class MyClass");
        return;
    }

    fseek(file.get(), 0, SEEK_END);
    long fileSize = ftell(file.get());
    rewind(file.get());

    std::unique_ptr<unsigned char[]> classBytes(new unsigned char[fileSize]);
    fread(classBytes.get(), 1, fileSize, file.get());

    // Create the jvmtiClassDefinition struct
    jvmtiClassDefinition classDef;
    classDef.klass = myClass;
    classDef.class_byte_count = fileSize;
    classDef.class_bytes = classBytes.get();

    // Redefine the class
    jvmtiError error = jvmtiEnv->RedefineClasses(1, &classDef);
    if (error != JVMTI_ERROR_NONE)
    {
        LOGF("RedefineClasses failed: %d", error);
        return;
    }

    LOG("Successfully redefined the class");
}

// launch
const char *RUNELITE_CLASS = "Lnet/runelite/client/RuneLite;";

const char *ENTRY_CLASS = "org.example.Entry";
const char *ENTRY_METHOD_NAME = "entry";
const char *ENTRY_METHOD_SIGNATURE = "()V";

std::filesystem::path LAUNCH_JAR_PATH = std::filesystem::path(get_user_home_path()) / "Desktop" / "entry.jar";

const char *URL_CLASS = "java/net/URL";
const char *URL_CONSTRUCTOR_SIGNATURE = "(Ljava/lang/String;)V";
const char *URL_CLASSLOADER_CLASS = "java/net/URLClassLoader";
const char *URL_CLASSLOADER_CONSTRUCTOR_SIGNATURE = "([Ljava/net/URL;Ljava/lang/ClassLoader;)V";
const char *LOAD_CLASS_NAME = "loadClass";
const char *LOAD_CLASS_SIGNATURE = "(Ljava/lang/String;)Ljava/lang/Class;";

jobject get_runelite_class_loader(JNIEnv *jni, jvmtiEnv *jvmti)
{
    jint class_count;
    jclass *classes;
    jvmtiError error = jvmti->GetLoadedClasses(&class_count, &classes);
    if (error != JVMTI_ERROR_NONE)
    {
        LOGF("GetLoadedClasses failed: %d", error);
    }

    for (jint i = 0; i < class_count; i++)
    {
        char *signature;
        error = jvmti->GetClassSignature(classes[i], &signature, nullptr);
        if (error != JVMTI_ERROR_NONE)
        {
            LOGF("GetClassSignature failed: %d", error);
            return nullptr;
        }

        if (std::string(signature) == RUNELITE_CLASS)
        {
            jobject class_loader;
            error = jvmti->GetClassLoader(classes[i], &class_loader);
            jvmti->Deallocate(reinterpret_cast<unsigned char *>(signature));
            if (error != JVMTI_ERROR_NONE)
            {
                LOGF("GetClassLoader failed: %d", error);
                return nullptr;
            }

            return class_loader;
        }

        jvmti->Deallocate(reinterpret_cast<unsigned char *>(signature));
    }

    return nullptr;
}

jobject loadJar(JNIEnv *jni, jobject parentClassLoader)
{
    std::string file_jar_path = "file:" + LAUNCH_JAR_PATH.string();

    jstring jJarPath = jni->NewStringUTF(file_jar_path.c_str());
    jclass urlClass = jni->FindClass(URL_CLASS);
    jmethodID urlConstructor = jni->GetMethodID(urlClass, "<init>", URL_CONSTRUCTOR_SIGNATURE);
    jobject url = jni->NewObject(urlClass, urlConstructor, jJarPath);

    jobjectArray urlArray = jni->NewObjectArray(1, urlClass, url);

    jclass urlClassLoaderClass = jni->FindClass(URL_CLASSLOADER_CLASS);
    jmethodID urlClassLoaderConstructor = jni->GetMethodID(urlClassLoaderClass, "<init>", URL_CLASSLOADER_CONSTRUCTOR_SIGNATURE);
    return jni->NewObject(urlClassLoaderClass, urlClassLoaderConstructor, urlArray, parentClassLoader);
}

void initJar(JNIEnv *jni, jobject classLoader)
{
    jstring mainClass = jni->NewStringUTF(ENTRY_CLASS);
    jclass urlClassLoaderClass = jni->GetObjectClass(classLoader);
    jmethodID loadClassMethod = jni->GetMethodID(urlClassLoaderClass, LOAD_CLASS_NAME, LOAD_CLASS_SIGNATURE);

    jobject classObj = jni->CallObjectMethod(classLoader, loadClassMethod, mainClass);
    if (classObj == nullptr)
    {
        LOG("Failed to load the entry class");
        return;
    }

    jclass oxideClass = static_cast<jclass>(classObj);
    jmethodID entryMethod = jni->GetStaticMethodID(oxideClass, ENTRY_METHOD_NAME, ENTRY_METHOD_SIGNATURE);
    jni->CallStaticVoidMethod(oxideClass, entryMethod);
}

void launch(jvmtiEnv *jvmti, JNIEnv *jni)
{
    LOG("launch");

    jobject rl_class_loader = get_runelite_class_loader(jni, jvmti);
    if (rl_class_loader == nullptr)
    {
        LOG("Failed to get the RuneLite class loader");
        return;
    }

    jobject urlClassLoader = loadJar(jni, rl_class_loader);
    initJar(jni, urlClassLoader);
}

void init()
{
    LOG("init");

    // Get the created Java VMs
    JavaVM *javaVMs[1];
    jsize numVMs;
    jint result = JNI_GetCreatedJavaVMs(javaVMs, 1, &numVMs);
    if (result != JNI_OK)
    {
        LOGF("JNI_GetCreatedJavaVMs failed: %d", result);
        return;
    }

    if (numVMs <= 0)
    {
        LOG("No Java VMs found");
        return;
    }

    JavaVM *jvm = javaVMs[0];
    JNIEnv *jniEnv;

    // Attach the current thread to the Java VM as a daemon thread
    result = jvm->AttachCurrentThreadAsDaemon((void **)&jniEnv, nullptr);
    if (result != JNI_OK)
    {
        LOGF("AttachCurrentThreadAsDaemon failed: %d", result);
        return;
    }

    LOG("Successfully attached to the Java VM as a daemon thread");

    // Get the jvmti env
    jvmtiEnv *jvmtiEnv;
    result = jvm->GetEnv((void **)&jvmtiEnv, JVMTI_VERSION_1_0);
    if (result != JNI_OK)
    {
        LOGF("GetEnv failed: %d", result);
        return;
    }

    // redefine_classes(jvmtiEnv, jniEnv);

    Sleep(1000);

    launch(jvmtiEnv, jniEnv);

    jvm->DetachCurrentThread();
}

#ifdef _WIN32
BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD ul_reason_for_call,
                      LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        LOG("hi from DLL_PROCESS_ATTACH");
        init();
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}
#endif
