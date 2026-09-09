/**
 * Module that provides serialized binaries and FFI metadata.
 *
 * The matching C API metadata, is available as
 * `ffi` property in every runtime.
 *
 * @example
 * Usage with Deno
 *
 * Deno provides native JSR imports and the built-in `Deno.dlopen` FFI API:
 *
 * ```ts
 * import { x86_64 } from "@serial/cpp-bindings-windows/bin";
 * 
 * const binary = x86_64;
 * const path = `./${binary.filename}`;
 * 
 * Deno.writeFileSync(path, Uint8Array.fromBase64(binary.data));
 * 
 * const library = Deno.dlopen(path, {
 *   serialOpen: {
 *     parameters: ["pointer", "i32", "i32", "i32", "i32", "pointer"],
 *     result: "i64",
 *   },
 * });
 * library.close();
 * ```
 *
 * @example
 * Usage with Bun
 *
 * Use Bun's built-in `bun:ffi` and `Bun.write` APIs:
 *
 * ```ts
 * import { dlopen } from "bun:ffi";
 * import { resolve } from "node:path";
 * import { x86_64 } from "@serial/cpp-bindings-windows/bin";
 * 
 * const binary = x86_64;
 * 
 * const path = resolve(binary.filename);
 * await Bun.write(path, Buffer.from(binary.data, "base64"));
 * 
 * const library = dlopen(path, {
 *   serialOpen: {
 *     args: ["ptr", "i32", "i32", "i32", "i32", "ptr"],
 *     returns: "i64",
 *   },
 * });
 * library.close();
 * ```
 *
 * @example
 * Usage with Node.js
 *
 * Node.js does not provide a general-purpose C FFI API. This example uses
 * [Koffi](https://koffi.dev/), together with JSR's npm compatibility layer:
 *
 * ```js
 * import { writeFileSync } from "node:fs";
 * import { resolve } from "node:path";
 * import koffi from "koffi";
 * import { x86_64 } from "@serial/cpp-bindings-windows/bin";
 * 
 * const binary = x86_64;
 * 
 * const path = resolve(binary.filename);
 * writeFileSync(path, Buffer.from(binary.data, "base64"));
 * 
 * const library = koffi.load(path);
 * library.func("serialOpen", "int64_t", [
 *   "void *",
 *   "int",
 *   "int",
 *   "int",
 *   "int",
 *   "void *",
 * ]);
 * library.unload();
 * ```
 *
 * @module
 */

import x86_64Library from "../../bin/x86_64.json" with { type: "json" };
import x86_64ffi from "../../bin/x86_64.ffi.json" with { type: "json" };

/**
 * The serialized `x86_64-windows` shared library and its FFI metadata.
 *
 * The library targets the generic x86-64 baseline. Decode `data` from base64
 * and write it to `filename` before loading it using the filesystem
 * and native FFI APIs provided by your runtime.
 */
const x86_64 = {
  ...x86_64Library,
  /**
   * ASTrein-generated metadata describing the library's exported C API.
   *
   * It contains symbols, parameter and return types, callbacks, struct
   * definitions, default values, and API documentation for generating
   * runtime-specific FFI adapters.
   */
  ffi: x86_64ffi,
};

export { x86_64 };
