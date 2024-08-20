injects a DLL that uses jvmti (https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html) to modify existing classes & load new classes in a process with a JVM.  

more specifically, it uses https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html#RedefineClasses & https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html#GetClassLoader

was used to inject runelite (https://runelite.net/)
