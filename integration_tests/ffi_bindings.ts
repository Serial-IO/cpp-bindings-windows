/**
 * Minimal FFI bindings for the cpp-bindings-windows shared library
 * used by the Deno integration tests.
 */

export type LoadedLibrary = Deno.DynamicLibrary<typeof symbols>;
export type SerialLib = LoadedLibrary["symbols"];

const symbols = {
    serialOpen: {
        parameters: ["pointer", "i32", "i32", "i32", "i32", "pointer"] as const,
        result: "i64" as const,
    },
    serialClose: {
        parameters: ["i64", "pointer"] as const,
        result: "i32" as const,
    },
    serialRead: {
        parameters: ["i64", "pointer", "i32", "i32", "i32", "pointer"] as const,
        result: "i32" as const,
    },
    serialWrite: {
        parameters: ["i64", "pointer", "i32", "i32", "i32", "pointer"] as const,
        result: "i32" as const,
    },
};

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
        libraryPath,
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


