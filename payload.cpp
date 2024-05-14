// g++ -shared -o payload.dll payload.cpp -I"C:\Program Files\Eclipse Adoptium\jdk-11.0.23.9-hotspot\include" -I"C:\Program Files\Eclipse Adoptium\jdk-11.0.23.9-hotspot\include\win32" -L"C:\Program Files\Eclipse Adoptium\jdk-11.0.23.9-hotspot\lib" -ljvm

#include <iostream>
#include <jni.h>
#include <jvmti.h>
#include <memory>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

constexpr const char *LOG_FILE_PATH = "C:\\Users\\user\\Desktop\\log.txt";
constexpr const char *CLASS_FILE_PATH = "C:\\Users\\user\\Desktop\\MyClass.class";

int log_count = 0;
void log(const std::string &msg)
{
    FILE *file = fopen(LOG_FILE_PATH, "a");
    printf("%i - %s\n", log_count, msg.c_str());
    fprintf(file, "%i - %s\n", log_count, msg.c_str());
    log_count++;
    fclose(file);
}

void init()
{
    log("hi from init");

    // Get the created Java VMs
    JavaVM *javaVMs[1];
    jsize numVMs;
    jint result = JNI_GetCreatedJavaVMs(javaVMs, 1, &numVMs);
    if (result != JNI_OK)
    {
        char msg[100];
        snprintf(msg, sizeof(msg), "Failed to get created Java VMs: %d", result);
        log(msg);
        return;
    }

    char msg[100];
    snprintf(msg, sizeof(msg), "Number of created Java VMs: %d", numVMs);
    log(msg);

    if (numVMs <= 0)
    {
        log("No Java VMs found");
        return;
    }

    JavaVM *javaVm = javaVMs[0];
    JNIEnv *jniEnv;

    // Attach the current thread to the Java VM as a daemon thread
    result = javaVm->AttachCurrentThreadAsDaemon((void **)&jniEnv, nullptr);
    if (result != JNI_OK)
    {
        char msg[100];
        snprintf(msg, sizeof(msg), "Failed to attach to the Java VM as a daemon thread: %d", result);
        log(msg);
        return;
    }

    log("Successfully attached to the Java VM as a daemon thread");

    // Get the jvmti env
    jvmtiEnv *jvmtiEnv;
    result = javaVm->GetEnv((void **)&jvmtiEnv, JVMTI_VERSION_1_0);
    if (result != JNI_OK)
    {
        char msg[100];
        snprintf(msg, sizeof(msg), "Failed to get the JVMTI environment: %d", result);
        log(msg);
        log("Failed to get the JVMTI environment");
        return;
    }

    // Add the can_redefine_classes capability
    jvmtiCapabilities capabilities;
    memset(&capabilities, 0, sizeof(capabilities));
    capabilities.can_redefine_classes = 1;

    result = jvmtiEnv->AddCapabilities(&capabilities);
    if (result != JVMTI_ERROR_NONE)
    {
        char msg[100];
        snprintf(msg, sizeof(msg), "Failed to add the can_redefine_classes capability error code: %d", result);
        log(msg);
        return;
    }

    // Get the jclass object for the class
    jclass myClass = jniEnv->FindClass("org/example/MyClass");
    if (myClass == nullptr)
    {
        log("Failed to find the class MyClass");
        return;
    }

    // Load the new class bytes from a file
    std::unique_ptr<FILE, decltype(&fclose)> file(fopen(CLASS_FILE_PATH, "rb"), &fclose);
    if (!file)
    {
        log("Failed to open the class file");
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
        char msg[100];
        snprintf(msg, sizeof(msg), "Failed to redefine the class error code: %d", error);
        log(msg);
        return;
    }

    log("Successfully redefined the class");

    // Detach the current thread from the Java VM
    javaVm->DetachCurrentThread();
}

#ifdef _WIN32
BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD ul_reason_for_call,
                      LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        log("hi from DLL_PROCESS_ATTACH");
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
