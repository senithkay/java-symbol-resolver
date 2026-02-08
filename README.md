# JVMTI-based Java Symbol Resolver

This project provides a JVMTI-based symbol resolver that maps JIT-compiled code addresses back to Java method names.

## Overview

The system consists of a **JVMTI Agent (`agent.c`)**: Loads into the JVM and builds a symbol table of JIT-compiled methods, mapping code address ranges to method names.

## How Symbol Resolution Works

### JVMTI Agent Architecture

The JVMTI (Java Virtual Machine Tool Interface) agent is a shared library (`libagent.so`) that attaches to the JVM at startup. It:

1. **Captures JIT Compilation Events**: Listens for `CompiledMethodLoad` events when the JVM compiles Java bytecode to native machine code.
2. **Builds Symbol Table**: Stores each compiled method's code address range (`[start, end)`) and associated metadata (class name, method name, signature).
3. **Provides Lookup Function**: The `lookup_symbol(const void *addr)` function searches the table to find which method contains the given address.

### Symbol Table Structure

```c
typedef struct {
    const void *code_start;    // Start of JIT-compiled code
    const void *code_end;      // End of JIT-compiled code
    char *method_name;         // e.g., "heavyCompute"
    char *class_sig;           // e.g., "Lspike_detector/TestJavaCpuSpike;"
    char *method_sig;          // e.g., "(I)J"
    jmethodID method;          // JVM internal ID
} Symbol;
```

### Resolution Process

1. **Agent Loads**: At JVM startup (`Agent_OnLoad`), the agent registers event callbacks and enables JIT compilation notifications.
2. **VM Initialization**: After the VM is fully initialized (`VMInit` callback), the agent calls `GenerateEvents()` to retroactively capture all already-compiled methods.
3. **Symbol Collection**: For each `CompiledMethodLoad` event, the agent records the method's code range and metadata.
4. **Lookup**: When an address is provided (via stdin), `lookup_symbol()` performs a linear search through the table to find the matching range.

## How to Run

### Prerequisites

- Linux kernel ≥ 4.15
- JDK (Java Development Kit)
- GCC

### Setup

#### 1. Set JAVA_HOME (if not set)

```bash
# Find Java installation
find /usr -name "java" -type f 2>/dev/null | head -5

# Edit sethome.sh with the correct path, e.g.:
# export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-arm64

# Source the script
source sethome.sh
```

#### 2. Compile Everything

```bash
# Compile JVMTI agent
gcc -shared -fPIC agent.c -o libagent.so \
  -I$JAVA_HOME/include \
  -I$JAVA_HOME/include/linux \
  -lpthread

# Compile Java test programs
cd spike_detector
javac TestJavaCpuSpike.java
cd ..
```

### Running the System

#### Manual Step-by-Step

**Terminal 1: Start Java App with Agent**

```bash
# Run Java test program with JVMTI agent
java -agentpath:./libagent.so  <JAVA_APPLICATION>

# The agent will print:
# [agent] Loaded — waiting for VM init to generate retroactive events
# [agent] VM initialized — requesting retroactive CompiledMethodLoad events
# [agent] #0  Lspike_detector/TestJavaCpuSpike;::heavyCompute  [0xffff... – 0xffff...]  (128 bytes)
# [agent] #1  Lspike_detector/SpikeMath;::trigBurn  [0xffff... – 0xffff...]  (256 bytes)
# ...
# Enter address (hex):
```

**Terminal 1: Resolve Symbols**

Paste hex addresses into Terminal 1's prompt:

```
Enter address (hex): 0000ffffabcd1234
Symbol for 0x0000ffffabcd1234: heavyCompute  (symbol_count=12)
```

### Troubleshooting

- **Agent doesn't load**: Check `JAVA_HOME` and library paths.
- **No symbols captured**: Ensure the Java app is running CPU-intensive code to trigger JIT compilation.
- **"UNKNOWN" for addresses**: Verify the address is within a JIT-compiled method range (check agent output).

### Files Overview

| File | Purpose |
|------|---------|
| `agent.c` | JVMTI agent source — builds symbol table |
| `sethome.sh` | JAVA_HOME setup script |
