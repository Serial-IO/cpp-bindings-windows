/**
 * Module that provides exports for serialized binary files.
 *
 * @example
 * Import the corresponding binary and write the file to disk.
 *
 * ```ts
 * import { x86_64 } from "@serial/cpp-bindings-windows/bin";
 *
 * Deno.writeFileSync(`./${x86_64.filename}`, Uint8Array.fromBase64(x86_64.data));
 * ```
 * @module
 */

import x86_64 from "../../bin/x86_64.json" with { type: "json" };

export { x86_64 };
