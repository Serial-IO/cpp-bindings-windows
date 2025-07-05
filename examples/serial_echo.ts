// @ts-nocheck
// deno run --allow-ffi --allow-read examples/serial_echo.ts

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
    // Encodes JavaScript string as null-terminated UTF-8 byte array.
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
// Load dynamic library
// -----------------------------------------------------------------------------
const { lib, port: cliPort } = parseArgs();

const dylib = Deno.dlopen(
    lib,
    {
        serialOpen: { parameters: ["pointer", "i32", "i32", "i32", "i32"], result: "pointer" },
        serialClose: { parameters: ["pointer"], result: "void" },
        serialWrite: { parameters: ["pointer", "pointer", "i32", "i32", "i32"], result: "i32" },
        serialReadUntil: {
            parameters: ["pointer", "pointer", "i32", "i32", "i32", "pointer"],
            result: "i32",
        },
        serialGetPortsInfo: { parameters: ["pointer", "i32", "pointer"], result: "i32" },
    } as const,
);

// -----------------------------------------------------------------------------
// 1. List available ports
// -----------------------------------------------------------------------------
const sepBuf = cString(";");
const portsBuf = new Uint8Array(4096);
dylib.symbols.serialGetPortsInfo(
    pointer(portsBuf),
    portsBuf.length,
    pointer(sepBuf),
);

const cPortsStr = decoder.decode(portsBuf.subarray(0, portsBuf.indexOf(0)));
const ports = cPortsStr ? cPortsStr.split(";") : [];
console.log("Available ports:");
for (const p of ports) {
    console.log(" •", p);
}
if (ports.length === 0) {
    console.error("No serial ports found (ttyUSB). Exiting.");
    dylib.close();
    Deno.exit(1);
}

// -----------------------------------------------------------------------------
// 2. Echo test on selected port
// -----------------------------------------------------------------------------
const portPath = cliPort ?? ports[0];
console.log(`\nUsing port: ${portPath}`);

const portBuf = cString(portPath);
const handle = dylib.symbols.serialOpen(pointer(portBuf), 115200, 8, 0, 0);
if (handle === null) {
    console.error("Failed to open port!");
    dylib.close();
    Deno.exit(1);
}

// Give MCU a moment to reboot (similar to C++ tests)
await new Promise((r) => setTimeout(r, 2000));

const msg = "HELLO\n";
const msgBuf = encoder.encode(msg);
const written = dylib.symbols.serialWrite(handle, pointer(msgBuf), msgBuf.length, 100, 1);
if (written !== msgBuf.length) {
    console.error(`Write failed (wrote ${written}/${msgBuf.length})`);
    dylib.symbols.serialClose(handle);
    dylib.close();
    Deno.exit(1);
}

const readBuf = new Uint8Array(64);
const untilBuf = new Uint8Array(["\n".charCodeAt(0)]);

const read = dylib.symbols.serialReadUntil(handle, pointer(readBuf), readBuf.length, 500, 1, pointer(untilBuf));
if (read <= 0) {
    console.error("Read failed or timed out.");
    dylib.symbols.serialClose(handle);
    dylib.close();
    Deno.exit(1);
}

const echo = decoder.decode(readBuf.subarray(0, read));
console.log(`Echo response (${read} bytes): '${echo}'`);

if (echo === msg) {
    console.log("Echo test: ✅ success");
} else {
    console.error("Echo test: ❌ mismatch");
}

dylib.symbols.serialClose(handle);
dylib.close(); 
