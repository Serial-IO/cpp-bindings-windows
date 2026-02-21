# C++ Bindings Windows

This package ships the native Windows shared library payload as a **JSON/base64
blob**.

## Usage

Import the binary export and write the `.dll` to disk (same API as `@serial/cpp-bindings-linux`):

```ts
import { x86_64 } from "@serial/cpp-bindings-windows/bin";

Deno.writeFileSync(`./${x86_64.filename}`, Uint8Array.fromBase64(x86_64.data));

// Now you can open the binary using for example `Deno.dlopen`
```
