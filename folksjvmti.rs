#![feature(try_find)]

mod jvmti;

use std::ptr::null_mut;

use jni::objects::{JClass, JObject};
use jni::sys::JNI_GetCreatedJavaVMs;
use jni::{JNIEnv, JavaVM};

use crate::jvmti::errors::{InjectError, InjectResult};
use crate::jvmti::jvmti_sys::JVMTI_VERSION;
use crate::jvmti::jvmti_wrapper::JvmtiEnv;
use crate::jvmti::sync::JvmtiSupplier;

const OXIDE_CLASS: &str = "dev.narcos.oxide.Oxide";
const OXIDE_ENTRY_METHOD_NAME: &str = "entry";
const OXIDE_ENTRY_METHOD_SIGNATURE: &str = "()V";

const RUNELITE_CLASS: &str = "Lnet/runelite/client/RuneLite;";

#[no_mangle]
pub unsafe extern "system" fn DllMain(_: usize, reason: u32, _: usize) -> i32 {
    match reason {
        1 => match init_oxide() {
            Ok(_) => 1,
            Err(_) => 0,
        },
        _ => 1,
    }
}

unsafe fn init_oxide() -> InjectResult<()> {
    let mut jvm_ptr = Vec::with_capacity(1).as_mut_ptr();
    let count = null_mut();

    JNI_GetCreatedJavaVMs(&mut jvm_ptr, 1, count);

    let jvm = JavaVM::from_raw(jvm_ptr).unwrap();
    let mut jni = jvm.attach_current_thread_as_daemon().unwrap();
    let jvmti = jvm.get_jvmti_env(JVMTI_VERSION);

    let rl_class_loader = get_runelite_class_loader(&jvmti)?;
    let url_class_loader = load_jar(&mut jni, rl_class_loader)?;
    init_jar(&mut jni, url_class_loader)
}

unsafe fn get_runelite_class_loader<'a>(jvmti: &JvmtiEnv) -> InjectResult<JObject<'a>> {
    let classes = jvmti.get_loaded_classes()?;

    let runelite_class = classes
        .into_iter()
        .try_find(|it| {
            jvmti
                .get_class_signature(*it)
                .map(|it| it.0 == RUNELITE_CLASS)
        })?
        .ok_or(InjectError::Generic("Could not find RuneLite class"))?;

    Ok(JObject::from_raw(jvmti.get_class_loader(runelite_class)?))
}

const URL_CLASS: &str = "java/net/URL";
const URL_CONSTRUCTOR_SIGNATURE: &str = "(Ljava/lang/String;)V";

const URL_CLASSLOADER_CLASS: &str = "java/net/URLClassLoader";
const URL_CLASSLOADER_CONSTRUCTOR_SIGNATURE: &str = "([Ljava/net/URL;Ljava/lang/ClassLoader;)V";

fn load_jar<'a>(
    jni: &mut JNIEnv<'a>,
    parent_class_loader: JObject<'a>,
) -> InjectResult<JObject<'a>> {
    // let path = dirs::home_dir().map_or(
    //     Err(InjectError::Generic("Could not find Oxide jar")),
    //     |it| Ok(it.join(OXIDE_PATH)),
    // )?;

    let oxide_path = dirs::home_dir().unwrap().join(".oxide").join("Oxide.jar");

    // let jar_path = jni.new_string(format!("file:{}", OXIDE_PATH))?;
    let jar_path = jni.new_string(format!("file:{}", oxide_path.as_path().to_str().unwrap()))?;

    let url = jni.new_object(URL_CLASS, URL_CONSTRUCTOR_SIGNATURE, &[(&jar_path).into()])?;
    let url_array = jni.new_object_array(1, URL_CLASS, url)?;

    Ok(jni.new_object(
        URL_CLASSLOADER_CLASS,
        URL_CLASSLOADER_CONSTRUCTOR_SIGNATURE,
        &[(&url_array).into(), (&parent_class_loader).into()],
    )?)
}

const LOAD_CLASS_NAME: &str = "loadClass";
const LOAD_CLASS_SIGNATURE: &str = "(Ljava/lang/String;)Ljava/lang/Class;";

fn init_jar<'a>(jni: &mut JNIEnv<'a>, class_loader: JObject<'a>) -> InjectResult<()> {
    let main_class = jni.new_string(OXIDE_CLASS)?;

    let class_obj = jni
        .call_method(
            class_loader,
            LOAD_CLASS_NAME,
            LOAD_CLASS_SIGNATURE,
            &[(&main_class).into()],
        )?
        .l()?;

    let class = JClass::from(class_obj);

    let _ = jni.call_static_method(
        class,
        OXIDE_ENTRY_METHOD_NAME,
        OXIDE_ENTRY_METHOD_SIGNATURE,
        &[],
    )?;

    Ok(())
}
