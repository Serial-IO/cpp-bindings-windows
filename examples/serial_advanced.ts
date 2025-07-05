// @ts-nocheck
// deno run --allow-ffi --allow-read examples/serial_advanced.ts --lib ./build/libcpp_unix_bindings.so --port /dev/ttyUSB0

/*
   Advanced Deno example showcasing the extended C-API helpers:
   1. serialReadLine / serialWriteLine for convenient newline-terminated I/O
   2. serialPeek to inspect next byte without consuming
   3. Tx/Rx statistics counters
   4. serialDrain to wait until all bytes are sent

   Build the shared library first (e.g. via `cmake --build build`).
*/

interface CliOptions {
    lib: string;
    port?: string;
}

function parseArgs(): CliOptions {
    const opts: CliOptions = { lib: "./build/libcpp_unix_bindings.so" };

    for (let i = 0; i < Deno.args.length; ++i) {
        const arg = Deno.args[i];
        if (arg === "--lib" && i + 1 < Deno.args.length) {
            opts.lib = Deno.args[++i];
        } else if (arg === "--port" && i + 1 < Deno.args.length) {
            opts.port = Deno.args[++i];
        } else {
            console.warn(`Unknown argument '${arg}' ignored.`);
        }
    }
    return opts;
}

// -----------------------------------------------------------------------------
// Helper utilities for C interop
// -----------------------------------------------------------------------------
const encoder = new TextEncoder();
const decoder = new TextDecoder();

function cString(str: string): Uint8Array {
    const bytes = encoder.encode(str);
    const buf = new Uint8Array(bytes.length + 1);
    buf.set(bytes, 0);
    buf[bytes.length] = 0;
    return buf;
}

function pointer(view: Uint8Array): Deno.UnsafePointer {
    return Deno.UnsafePointer.of(view) as Deno.UnsafePointer;
}

// -----------------------------------------------------------------------------
// Load dynamic library & bind needed symbols
// -----------------------------------------------------------------------------
const { lib, port: cliPort } = parseArgs();

const dylib = Deno.dlopen(
    lib,
    {
        serialOpen: { parameters: ["pointer", "i32", "i32", "i32", "i32"], result: "pointer" },
        serialClose: { parameters: ["pointer"], result: "void" },
        serialWriteLine: { parameters: ["pointer", "pointer", "i32", "i32"], result: "i32" },
        serialReadLine: { parameters: ["pointer", "pointer", "i32", "i32"], result: "i32" },
        serialPeek: { parameters: ["pointer", "pointer", "i32"], result: "i32" },
        serialDrain: { parameters: ["pointer"], result: "i32" },
        serialGetTxBytes: { parameters: ["pointer"], result: "i64" },
        serialGetRxBytes: { parameters: ["pointer"], result: "i64" },
    } as const,
);

// -----------------------------------------------------------------------------
// Open port
// -----------------------------------------------------------------------------
const portPath = cliPort ?? "/dev/ttyUSB0";
console.log(`Using port: ${portPath}`);

const portBuf = cString(portPath);
const handle = dylib.symbols.serialOpen(pointer(portBuf), 115200, 8, 0, 0);
if (handle === null) {
    console.error("Failed to open port!");
    dylib.close();
    Deno.exit(1);
}

// Wait 2 s for Arduino reset (DTR toggle)
await new Promise((r) => setTimeout(r, 2000));

// -----------------------------------------------------------------------------
// 1. Send a few lines and read them back (echo sketch on MCU)
// -----------------------------------------------------------------------------
const lines = [
    "The quick brown fox jumps over the lazy dog",
    "Grüße aus Deno!",
    "1234567890",
];

for (const ln of lines) {
    const payloadBuf = encoder.encode(ln);
    const written = dylib.symbols.serialWriteLine(handle, pointer(payloadBuf), payloadBuf.length, 100);
    if (written !== payloadBuf.length + 1) {
        console.error(`WriteLine failed for '${ln}'`);
        continue;
    }

    // Peek first byte (should be our first char)
    const peekBuf = new Uint8Array(1);
    if (dylib.symbols.serialPeek(handle, pointer(peekBuf), 500) === 1) {
        console.log(`Peek: '${String.fromCharCode(peekBuf[0])}'`);
    }

    const readBuf = new Uint8Array(256);
    const n = dylib.symbols.serialReadLine(handle, pointer(readBuf), readBuf.length, 1000);
    const lineRx = decoder.decode(readBuf.subarray(0, n));
    console.log(`RX (${n} bytes): '${lineRx}'`);
}

// Ensure all bytes are transmitted
if (dylib.symbols.serialDrain(handle) === 1) {
    console.log("Transmit buffer drained.");
}

// -----------------------------------------------------------------------------
// Print statistics
// -----------------------------------------------------------------------------
const txBytes = Number(dylib.symbols.serialGetTxBytes(handle));
const rxBytes = Number(dylib.symbols.serialGetRxBytes(handle));
console.log(`\nStatistics -> TX: ${txBytes} bytes, RX: ${rxBytes} bytes`);

// -----------------------------------------------------------------------------
// Cleanup
// -----------------------------------------------------------------------------
dylib.symbols.serialClose(handle);
dylib.close();
console.log("Done."); 
