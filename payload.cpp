// g++ -shared -o payload.dll payload.cpp -I"C:\Program Files\Eclipse Adoptium\jdk-11.0.23.9-hotspot\include" -I"C:\Program Files\Eclipse Adoptium\jdk-11.0.23.9-hotspot\include\win32" -L"C:\Program Files\Eclipse Adoptium\jdk-11.0.23.9-hotspot\lib" -ljvm

// Uncomment to enable debug logging
#define DEBUG_LOGGING

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
        ss << "[" << getCurrentTimestamp() << "] " << message << std::endl;
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

void init()
{
    LOG("hi from init");

    // Get the created Java VMs
    JavaVM *javaVMs[1];
    jsize numVMs;
    jint result = JNI_GetCreatedJavaVMs(javaVMs, 1, &numVMs);
    if (result != JNI_OK)
    {
        LOGF("Failed to get created Java VMs: %d", result);
        return;
    }

    LOGF("Number of created Java VMs: %d", numVMs);

    if (numVMs <= 0)
    {
        LOG("No Java VMs found");
        return;
    }

    JavaVM *javaVm = javaVMs[0];
    JNIEnv *jniEnv;

    // Attach the current thread to the Java VM as a daemon thread
    result = javaVm->AttachCurrentThreadAsDaemon((void **)&jniEnv, nullptr);
    if (result != JNI_OK)
    {
        LOGF("Failed to attach to the Java VM as a daemon thread: %d", result);
        return;
    }

    LOG("Successfully attached to the Java VM as a daemon thread");

    // Get the jvmti env
    jvmtiEnv *jvmtiEnv;
    result = javaVm->GetEnv((void **)&jvmtiEnv, JVMTI_VERSION_1_0);
    if (result != JNI_OK)
    {
        LOGF("Failed to get the JVMTI environment: %d", result);
        return;
    }

    // Add the can_redefine_classes capability
    jvmtiCapabilities capabilities;
    memset(&capabilities, 0, sizeof(capabilities));
    capabilities.can_redefine_classes = 1;

    result = jvmtiEnv->AddCapabilities(&capabilities);
    if (result != JVMTI_ERROR_NONE)
    {
        LOGF("Failed to add the can_redefine_classes capability error code: %d", result);
        return;
    }

    // Get the jclass object for the class
    jclass myClass = jniEnv->FindClass("org/example/MyClass");
    if (myClass == nullptr)
    {
        LOG("Failed to find the class MyClass");
        return;
    }

    // Load the new class bytes from a file
    std::unique_ptr<FILE, decltype(&fclose)> file(fopen(CLASS_FILE_PATH.c_str(), "rb"), &fclose);
    if (!file)
    {
        LOG("Failed to open the class file");
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
        LOGF("Failed to redefine the class error code: %d", error);
        return;
    }

    LOG("Successfully redefined the class");

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
