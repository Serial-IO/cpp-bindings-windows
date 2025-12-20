# C++ Bindings Windows

This package ships the native Windows shared library payload as a **JSON/base64
blob**.

## Usage

Import the JSON and write the `.dll` to disk (consumer project example):

```ts
import blob from "@serial/cpp-bindings-windows/bin/x86_64" with {
    type: "json",
};

const bytes = new TextEncoder().encode(atob(blob.data));

const tempFilePath = Deno.makeTempFileSync();
Deno.writeFileSync(tempFilePath, bytes, { mode: 0o755 });

// Now you can open the binary using for example `Deno.dlopen`
```
