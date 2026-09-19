# Possible release-sensitive rendering bugs

This note records additional code paths that may produce debug/release
differences. They are candidates for follow-up hardening, not confirmed causes
of the previously observed dark bump-mapping failure.

## 1. Float-to-`DWORD` conversion in D3DX format packing

`Perimeter/Source/Render/d3dx9/d3dx9.cpp:348` converts a floating-point color
component directly to `DWORD`:

```cpp
v = (DWORD)(src_component * ((1 << format->bits[c]) - 1) + 0.5f);
```

This assumes that `src_component` is finite and within the expected normalized
range. A negative, oversized, or NaN value makes the floating-point-to-integer
conversion undefined or otherwise invalid. The normal-map `Q8W8V8U8` path has a
separate conversion routine, so this is a generic D3DX format-conversion risk,
not a second confirmed cause of the bump-mapping issue.

## 2. Float-to-`uint32_t` conversion when packing normals

`Perimeter/Source/Render/src/RenderDevice.cpp:462-466` packs normal components
directly into unsigned integers:

```cpp
uint32_t x = static_cast<uint32_t>((n.x + 1) * 127.5f) & 0xFF;
```

The conversion is valid only when every component is finite and in the expected
`[-1, 1]` range. Invalid normalization could produce NaN or an out-of-range
value, making the conversion release-sensitive. The normal-generation path
normally constructs vectors with a positive Z component, so this has not been
shown to reproduce the original menu/building failure.

## 3. Byte-buffer pointer-punning and alignment assumptions

Several rendering paths load packed bytes by reinterpreting them as wider
objects instead of using `memcpy`:

- `Perimeter/Source/Render/src/RenderDevice.cpp:495-496`
- `Perimeter/Source/Render/d3dx9/d3dx9.cpp:314`
- `Perimeter/Source/Render/d3dx9/d3dx9.cpp:346`
- `Perimeter/Source/Render/D3D/D3DRender.h:22`

These accesses can violate C++ strict-aliasing and alignment rules. Optimized
builds and architectures with stricter alignment requirements may therefore
observe different results from debug builds. The accesses should be replaced
with `memcpy` or a defined bit-cast helper when those paths are next hardened.

## Follow-up

These candidates should be addressed independently of the committed fix for
the signed normal-map packing bug. Each change should be verified against the
affected renderer paths and reviewed for packed-pixel layout compatibility.
