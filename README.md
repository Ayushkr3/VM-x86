# VM-x86

x86 virtual machine using the Windows Hypervisor Platform (WinHvPlatform) for hardware-accelerated execution.

## Summary

VM-x86 is a low-level x86-32 virtual machine implementation aimed at analysis, debugging, and research. It is designed to boot and run 32-bit Windows images (including Windows XP) and exposes direct guest-physical memory access with page-granular interception implemented using nested page-table techniques (NPT / EPT-style). The project relies on the Windows Hypervisor Platform for hardware acceleration; a software-only fallback is limited or not provided.

Primary implementation languages: C (majority), C++ (components).

## Key capabilities

- Boot and run 32-bit Windows images, including Windows XP.
- Direct (raw) guest-physical memory access for inspection and live modification.
- EPT-style memory hooking using nested page table techniques (NPT) to intercept reads, writes and instruction fetches at page granularity.
- Low-level device emulation for legacy hardware: PCI host/ISA bridges, VGA, IDE, LAPIC/IOAPIC and port-mapped I/O.
- Facilities for breakpoints, execution tracing, per-page callbacks, and memory access logging.
- Hardware acceleration via WinHvPlatform / WinHvEmulation (required for accelerated paths on Windows hosts).

## Intended use cases

- Reverse engineering and dynamic analysis of 32-bit Windows binaries and drivers.
- Malware analysis in an isolated and observable environment.
- Low-level debugging of bootloaders, firmware and OS components.
- Research and teaching on x86 architecture, virtualization and device emulation.

## Technical highlights

- Real mode and protected mode support (16-bit and 32-bit CPU contexts).
- Flat guest-physical memory model for 32-bit execution.
- Nested page-table-inspired protection and interception for per-page callbacks and enforcement.
- Device subsystems implementing standard port-mapped I/O and MMIO interfaces.
- Hooking and tracing primitives suitable for Windows XP–era kernels and userland.

## Windows XP hooking

The memory interception and callback system is suitable for Windows XP–era analysis:
- Install per-page hooks to monitor kernel or user-space memory used by XP.
- Intercept reads/writes and instruction fetches to observe API calls, kernel transitions or driver behavior.
- Modify guest memory at runtime for instrumentation or live patching.

## Repository layout (high level)

- BIOS/        — CPU, PCI, APIC, VGA and other low-level firmware and device emulation sources  
- include/     — Third-party headers and interface headers (SDL3 headers included for builds)  
- build/       — Build artifacts (not checked into source control)  

## Build prerequisites

- Host: Windows 10/11 recommended for WinHvPlatform support. The repository depends on Windows Hypervisor Platform APIs for hardware-accelerated operation.
- Compiler: Visual Studio (MSVC) with C++17 support; CMake recommended for build generation.
- Dependencies:
  - Windows SDK providing WinHvPlatform and WinHvEmulation libraries
  - SDL3 (optional, for display/input)
  - Standard C/C++ toolchain


## Preview

https://github.com/user-attachments/assets/65713400-abd9-4596-9946-7cbb59d899d1


