
# 🧠 LC-3 Virtual Machine in C

This is a fully functional implementation of the LC-3 (Little Computer 3) virtual machine, written in C. The LC-3 is a simple 16-bit educational computer architecture widely used for teaching computer organization and low-level programming.

---

## 🚀 Features

- ✅ Emulates all 15 LC-3 instructions:
  - Arithmetic: `ADD`, `AND`, `NOT`
  - Control Flow: `BR`, `JMP`, `JSR`
  - Memory Access: `LD`, `LDI`, `LDR`, `LEA`, `ST`, `STI`, `STR`
  - System Calls: `TRAP` routines like `GETC`, `OUT`, `PUTS`, `IN`, `PUTSP`, `HALT`
- 📦 Loads `.obj` files (LC-3 compiled binaries)
- 🧠 Simulates 2¹⁶ (65,536) memory locations
- 💻 Memory-mapped I/O:
  - Keyboard Status Register (`KBSR` at `0xFE00`)
  - Keyboard Data Register (`KBDR` at `0xFE02`)
- ⌨️ Handles real-time terminal input/output
---

## 🛠️ Building and Running

### 🧱 Prerequisites

- C compiler (e.g., `gcc`, `clang`)
- Windows OS (uses `Windows.h` for keyboard handling)

### 🔧 Build

```bash
gcc lc3.c -o lc3_vm
```
Download the assembled version of [2048](https://www.jmeiners.com/lc3-vm/supplies/2048.obj).
Run the VM with the .obj file as an argument:
```bash
 $ lc3-vm path/to/2048.obj
```

Play 2048!


<img width="432" height="293" alt="image" src="https://github.com/user-attachments/assets/6258e182-f1bc-4f1f-8273-eb65c54c2ce7" />

