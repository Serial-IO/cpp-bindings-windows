/**
 * Minimal Deno integration tests for cpp-bindings-windows:
 * - verify that the shared library can be loaded
 * - verify that it can be cleanly unloaded again
 */

import { assertExists } from "@std/assert";
import { type LoadedLibrary, loadSerialLib, type SerialLib } from "./ffi_bindings.ts";

let lib: SerialLib | null = null;
let loadedLib: LoadedLibrary | null = null;

Deno.test({
    name: "Load cpp-bindings-windows library",
    async fn() {
        loadedLib = await loadSerialLib();
        assertExists(loadedLib, "Failed to load cpp-bindings-windows library");
        lib = loadedLib.symbols;

        assertExists(lib.serialOpen);
        assertExists(lib.serialClose);
        assertExists(lib.serialRead);
        assertExists(lib.serialWrite);
    },
    sanitizeResources: false,
    sanitizeOps: false,
});

Deno.test({
    name: "Unload cpp-bindings-windows library",
    async fn() {
        await Promise.resolve();
        if (!loadedLib) return;

        loadedLib.close();
        loadedLib = null;
        lib = null;
    },
    sanitizeResources: false,
    sanitizeOps: false,
});


