#include <iostream>
#include <vector>
#include <string>
#include "defs.h"
#include "defer.h"
#include "wasm.h"

const uint32_t MEMORY_PAGES = 102;

// Utils

void read_binary(const char* name, wasm_byte_vec_t& destination) {
    FILE* file = fopen(name, "rb");
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    wasm_byte_vec_new_uninitialized(&destination, file_size);
    fread(destination.data, file_size, 1, file);
    fclose(file);
}

std::string decode_name(const wasm_name_t* name) {
    return std::string(name->data, name->size);
}

void inspect_imports(wasm_module_t* module) {
    wasm_importtype_vec_t imports;
    DEFER(wasm_importtype_vec_delete(&imports));
    wasm_module_imports(module, &imports);
    for (uint16_t i = 0; i < imports.size; i++) {
      const wasm_externtype_t* type = wasm_importtype_type(imports.data[i]);
      const wasm_externkind_t kind = wasm_externtype_kind(type);
      const std::string key = decode_name(wasm_importtype_module(imports.data[i])) + "." + decode_name(wasm_importtype_name(imports.data[i]));
      std::cout << "Import " << i << ": " << key << std::endl;
    }

    wasm_exporttype_vec_t exports;
    DEFER(wasm_exporttype_vec_delete(&exports));
    wasm_module_exports(module, &exports);
    for (uint16_t i = 0; i < exports.size; i++) {
      const wasm_externtype_t* type = wasm_exporttype_type(exports.data[i]);
      const wasm_externkind_t kind = wasm_externtype_kind(type);
      const std::string key = decode_name(wasm_exporttype_name(exports.data[i]));
      std::cout << "Export " << i << ": " << key << std::endl;
    }
}

// Import functions

wasm_trap_t* console_log (void* env, const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    wasm_memory_t* memory = (wasm_memory_t*)env;
    uint32_t offset = args->data[0].of.i32;
    uint32_t length = args->data[1].of.i32;
    byte_t* data = wasm_memory_data(memory) + offset;
    std::string str = std::string(data, length);
    std::cout << str << std::endl;
    return NULL;
}

wasm_trap_t* milliseconds_since_start (const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    std::cout << "milliseconds_since_start" << std::endl;
    auto current = std::chrono::steady_clock::now();
    static auto start = current;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current - start);
    results->data[0] = WASM_I32_VAL((int32_t)elapsed.count());
    return NULL;
}

wasm_trap_t* draw_screen (const wasm_val_vec_t* args, wasm_val_vec_t* results) {
    // Unimplemented
    return NULL;
}

// Main program

int main() {
    // Runtime basics
    wasm_engine_t* engine = wasm_engine_new();
    wasm_store_t* store = wasm_store_new(engine);

    // Load, validate, compile module
    wasm_byte_vec_t wasm_bytes;
    read_binary("../doom.wasm", wasm_bytes);
    FAIL_IF(!wasm_module_validate(store, &wasm_bytes), "Invalid binary", ERR_GENERIC);
    wasm_module_t* module = wasm_module_new(store, &wasm_bytes);
    FAIL_IF(module == NULL, "Compilation failed", ERR_GENERIC);

    // Inspect imports
    // inspect_imports(module);

    // Define external memory
    const wasm_limits_t limits = { 108, wasm_limits_max_default };
    wasm_memory_t* memory = wasm_memory_new(store, wasm_memorytype_new(&limits));
    FAIL_IF(memory == NULL, "Failed to create memory", ERR_GENERIC);
    FAIL_IF(!wasm_memory_grow(memory, MEMORY_PAGES), "Failed to grow memory", ERR_GENERIC);

    // Define import functions
    wasm_functype_t* type_console_log = wasm_functype_new_2_0(wasm_valtype_new_i32(), wasm_valtype_new_i32());
    DEFER(wasm_functype_delete(type_console_log));
    wasm_func_t* func_console_log = wasm_func_new_with_env(store, type_console_log, console_log, memory, NULL);
    wasm_functype_t* type_milliseconds_since_start = wasm_functype_new_0_1(wasm_valtype_new_i32());
    DEFER(wasm_functype_delete(type_milliseconds_since_start));
    wasm_func_t* func_milliseconds_since_start = wasm_func_new(store, type_milliseconds_since_start, milliseconds_since_start);
    wasm_functype_t* type_draw_screen = wasm_functype_new_1_0(wasm_valtype_new_i32());
    DEFER(wasm_functype_delete(type_draw_screen));
    wasm_func_t* func_draw_screen = wasm_func_new(store, type_draw_screen, draw_screen);

    // Create import data; must be correctly ordered
    std::vector<wasm_extern_t*> extern_list;
    extern_list.push_back(wasm_func_as_extern(func_milliseconds_since_start));
    extern_list.push_back(wasm_func_as_extern(func_console_log));
    extern_list.push_back(wasm_func_as_extern(func_draw_screen));
    extern_list.push_back(wasm_func_as_extern(func_console_log));
    extern_list.push_back(wasm_func_as_extern(func_console_log));
    extern_list.push_back(wasm_memory_as_extern(memory));
    wasm_extern_vec_t imports = { extern_list.size(), extern_list.data() };

    // Instantiate with imports
    wasm_instance_t* instance = wasm_instance_new(store, module, &imports, NULL);
    FAIL_IF(instance == NULL, "Instantiation failed", ERR_GENERIC);

    // Extract function exports
    wasm_extern_vec_t exports;
    wasm_instance_exports(instance, &exports);
    const wasm_func_t* func_main = wasm_extern_as_func(exports.data[3]);
    const wasm_func_t* func_doom_loop_step = wasm_extern_as_func(exports.data[2]);

    // Initialize Doom
    wasm_val_t args_vals[2] = { WASM_I32_VAL(0), WASM_I32_VAL(0) };
    wasm_val_t results_vals[1] = { WASM_INIT_VAL };
    wasm_val_vec_t args = WASM_ARRAY_VEC(args_vals);
    wasm_val_vec_t results = WASM_ARRAY_VEC(results_vals);
    FAIL_IF(wasm_func_call(func_main, &args, &results), "Failed calling function", ERR_GENERIC);

    return 0;
}
