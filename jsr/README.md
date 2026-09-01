# C++ Bindings Windows

[![Build Binary](https://github.com/Serial-IO/cpp-bindings-windows/actions/workflows/build_binary.yml/badge.svg)](https://github.com/Serial-IO/cpp-bindings-windows/actions/workflows/build_binary.yml)
[![JSR](https://jsr.io/badges/@serial/cpp-bindings-windows)](https://jsr.io/@serial/cpp-bindings-windows)

Binaries are provided as a [package on JSR](https://jsr.io/@serial/cpp-bindings-windows). They are serialized as a base64 string inside the JSON file.

This package targets server-side JavaScript runtimes that can write files and
load Windows dynamic libraries. Deno can consume it directly from JSR; Bun and
Node.js use JSR's npm compatibility layer. Browser and edge runtimes cannot use
the native library because they do not expose native FFI access.

The package contains portable binaries for `x86_64`. The x86-64 artifact
uses the generic x86-64 baseline.

## Binary compatibility

Common release baselines are shown below for orientation:

| Distribution | Release baseline |
| --- | --- |
| Windows | 10+ |

## FFI metadata

It also includes cpp-core FFI API metadata generated with
[ASTrein](https://github.com/Katze719/ASTrein) at `bin/x86_64/ffi.json`. It describes the exported C symbols, parameter and
return types, callbacks, structs, default values, and API documentation used by
runtime-specific FFI adapter generators.

This package is primarily intended as a dependency for
[`@serial/serial`](https://jsr.io/@serial/serial). However, it can also be used
independently.

## Usage

Select the export matching the host architecture. Each export contains the
base64-encoded shared library and its matching FFI metadata. The following
examples write the library to disk, load it, and release it again.

### Deno

Deno provides native JSR imports and the built-in `Deno.dlopen` FFI API. Save
this as `example.ts`:

```ts
import { x86_64 } from "jsr:@serial/cpp-bindings-windows/bin";

const binary = x86_64;
const path = `./${binary.filename}`;

Deno.writeFileSync(path, Uint8Array.fromBase64(binary.data));

const library = Deno.dlopen(path, {
  serialOpen: {
    parameters: ["pointer", "i32", "i32", "i32", "i32", "pointer"],
    result: "i64",
  },
});
library.close();
```

Run it with write and FFI permissions:

```sh
deno run --allow-write --allow-ffi example.ts
```

### Bun

Add the package through JSR's npm compatibility layer:

```sh
bunx jsr add @serial/cpp-bindings-windows
```

Then use Bun's built-in `bun:ffi` and `Bun.write` APIs:

```ts
import { dlopen } from "bun:ffi";
import { resolve } from "node:path";
import { x86_64 } from "@serial/cpp-bindings-windows/bin";

const binary = x86_64;

const path = resolve(binary.filename);
await Bun.write(path, Buffer.from(binary.data, "base64"));

const library = dlopen(path, {
  serialOpen: {
    args: ["ptr", "i32", "i32", "i32", "i32", "ptr"],
    returns: "i64",
  },
});
library.close();
```

```sh
bun run example.ts
```

> [!WARNING]
> Bun currently marks its built-in
> [`bun:ffi` API](https://bun.sh/docs/runtime/ffi) as experimental.

### Node.js

Node.js does not provide a general-purpose C FFI API. This example uses
[Koffi](https://koffi.dev/), together with JSR's npm compatibility layer:

```sh
npx jsr add @serial/cpp-bindings-windows
npm install koffi
```

Save this as `example.mjs`:

```js
import { writeFileSync } from "node:fs";
import { resolve } from "node:path";
import koffi from "koffi";
import { x86_64 } from "@serial/cpp-bindings-windows/bin";

const binary = x86_64;

const path = resolve(binary.filename);
writeFileSync(path, Buffer.from(binary.data, "base64"));

const library = koffi.load(path);
library.func("serialOpen", "int64_t", [
  "void *",
  "int",
  "int",
  "int",
  "int",
  "void *",
]);
library.unload();
```

```sh
node example.mjs
```

These examples verify that the native library can be loaded and that its
`serialOpen` symbol can be resolved. The matching `binary.ffi` value describes
the complete set of symbols and structs for generating or configuring
runtime-specific bindings.

Non-JavaScript consumers can download the same architecture-specific `.so` and
`.ffi.json` files directly from the
[GitHub releases](https://github.com/Serial-IO/cpp-bindings-windows/releases).

> [!NOTE]
> For a more in depth guide, check out the
> [Wiki](https://github.com/Serial-IO/cpp-bindings-windows/wiki) section on how to
> use the C++ bindings for Windows.
