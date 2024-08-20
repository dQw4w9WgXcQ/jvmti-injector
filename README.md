injects a DLL that uses jvmti (https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html) to modify existing classes & load new classes in a runelite process

more specifically, it uses https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html#RedefineClasses & https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html#GetClassLoader
