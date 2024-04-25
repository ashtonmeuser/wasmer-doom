use std::{time::{Duration, Instant}, str};
use std::thread;
use wasmer::{imports, AsStoreRef, Function, FunctionEnv, FunctionEnvMut, Instance, Memory, MemoryType, Module, Store, Value, WasmSlice};

const MEMORY_PAGES: u32 = 102;

struct DoomEnv {
    start: Instant,
    memory: Memory,
}

fn call(instance: &Instance, store: &mut Store, name: &str, params: &[Value]) -> anyhow::Result<Box<[Value]>> {
    let main = instance.exports.get_function(name)?;
    let result = main.call(store, params)?;
    Ok(result)
}

fn console_log(env: FunctionEnvMut<DoomEnv>, offset: i32, length: i32) {
    let store = env.as_store_ref();
    let view = env.data().memory.view(&store);
    let slice = WasmSlice::new(&view, offset as u64, length as u64).unwrap();
    let vec = slice.read_to_vec().unwrap();
    let string = str::from_utf8(&vec).unwrap();
    println!("{}", string);
}


fn milliseconds_since_start(env: FunctionEnvMut<DoomEnv>) -> i32 {
    let current_time = Instant::now();
    let elapsed = current_time.duration_since(env.data().start);
    elapsed.as_millis() as i32
}

fn draw_screen(_offset: i32) { } // Unimplemented

fn main() -> anyhow::Result<()> {    
    let wasm_bytes = include_bytes!("../../doom.wasm");
    let mut store = Store::default();
    let memory = Memory::new(&mut store, MemoryType::new(MEMORY_PAGES, None, false))?;
    let env = FunctionEnv::new(&mut store, DoomEnv {
        start: Instant::now(),
        memory: memory.clone(),
    });
    let module = Module::new(&store, &wasm_bytes)?;
    let imports = imports! {
        "env" => {
            "memory" => memory,
        },
        "js" => {
            "js_console_log" => Function::new_typed_with_env(&mut store, &env, console_log),
            "js_stdout" => Function::new_typed_with_env(&mut store, &env, console_log),
            "js_stderr" => Function::new_typed_with_env(&mut store, &env, console_log),
            "js_milliseconds_since_start" => Function::new_typed_with_env(&mut store, &env, milliseconds_since_start),
            "js_draw_screen" => Function::new_typed(&mut store, draw_screen),
        },
    };
    let instance = Instance::new(&mut store, &module, &imports)?;

    // Initialize Doom
    call(&instance, &mut store, "main", &[Value::I32(0), Value::I32(0)])?;

    loop {
        // Invoke Doom step 60 times per second
        call(&instance, &mut store, "doom_loop_step", &[])?;
        thread::sleep(Duration::from_micros(16667));
    }
}