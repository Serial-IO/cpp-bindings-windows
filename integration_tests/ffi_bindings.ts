/**
 * Minimal FFI bindings for the cpp-bindings-windows shared library
 * used by the Deno integration tests.
 */

export type LoadedLibrary = Deno.DynamicLibrary<typeof symbols>;
export type SerialLib = LoadedLibrary["symbols"];

const symbols = {
    meta: {
        parameters: ["pointer"],
        result: "void",
    },
    serialReadUntilSequence: {
        parameters: ["i64", "pointer", "i32", "pointer", "pointer", "i32", "pointer"],
        result: "i32",
    },
    serialWaitForDrain: {
        parameters: ["i64", "pointer"],
        result: "i32",
    },
    serialSetEventCallback: {
        parameters: ["pointer", "pointer"],
        result: "i32",
    },
    serialOpen: {
        parameters: ["pointer", "pointer", "pointer"],
        result: "i64",
    },
    serialClose: {
        parameters: ["i64", "pointer"],
        result: "i32",
    },
    serialRead: {
        parameters: ["i64", "pointer", "i32", "pointer", "pointer"],
        result: "i32",
    },
    serialWrite: {
        parameters: ["i64", "pointer", "i32", "pointer", "pointer"],
        result: "i32",
    },
} as const;

/**
 * Load the cpp-bindings-windows shared library
 * @param libraryPath Path to the .dll file (defaults to build directory)
 * @returns Object containing the symbols and a close method
 */
export async function loadSerialLib(
    libraryPath?: string,
): Promise<LoadedLibrary> {
    await Promise.resolve();

    const possiblePaths = [
        libraryPath ?? Deno.env.get("SERIAL_LIBRARY_PATH"),
        "../build/cpp_bindings_windows.dll",
        "../build/Release/cpp_bindings_windows.dll",
        "../build/cpp_bindings_windows/Release/cpp_bindings_windows.dll",
        "../build/**/Release/cpp_bindings_windows.dll",
        "./cpp_bindings_windows.dll",
    ].filter((p): p is string => p !== undefined);

    let lib: LoadedLibrary | null = null;
    let lastError: Error | null = null;

    for (const path of possiblePaths) {
        try {
            // Note: Deno does not support globs; keep entries explicit.
            if (path.includes("*")) continue;
            const loaded = Deno.dlopen(path, symbols) as LoadedLibrary;
            lib = loaded;
            break;
        } catch (error) {
            lastError = error as Error;
            continue;
        }
    }

    if (!lib) {
        throw new Error(
            `Failed to load cpp-bindings-windows library. Tried paths: ${
                possiblePaths.join(", ")
            }. Last error: ${lastError?.message}`,
        );
    }

    return lib;
}
