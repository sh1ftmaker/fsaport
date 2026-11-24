/* Big-endian values in file images.
 *
 * Every GameCube disc format is big-endian; the port runs little-endian. A struct
 * that is laid over file bytes declares its multi-byte fields as BE<T>: reading one
 * gives the host value, assigning stores it big-endian again, so the struct keeps
 * the file's layout and code that uses it reads the same as before.
 *
 * BE<T> is trivially copyable and trivially default-constructible, so it can live
 * in unions, arrays and structs that are copied with memcpy.
 *
 * Data the GPU consumes (vertex arrays, display lists, texture and palette images)
 * stays big-endian; Aurora decodes it as such. Only what the CPU reads is swapped.
 */
#ifndef FSA_PORT_BE_H
#define FSA_PORT_BE_H

#ifdef __cplusplus

#ifdef FSA_DIAG
// Port-only helpers without a decomp symbol, and the hottest functions there
// are: not counted by the diagnostics build (docs/DIAGNOSTICS.md).
#pragma clang attribute push(__attribute__((no_instrument_function)), apply_to = function)
#endif

template <unsigned N> struct be_uint_;
template <> struct be_uint_<1> { typedef unsigned char type; };
template <> struct be_uint_<2> { typedef unsigned short type; };
template <> struct be_uint_<4> { typedef unsigned int type; };
template <> struct be_uint_<8> { typedef unsigned long long type; };

template <typename T>
inline T be_swap_value(T v)
{
    typedef typename be_uint_<sizeof(T)>::type U;
    U u = __builtin_bit_cast(U, v);
    if constexpr (sizeof(T) == 2) {
        u = __builtin_bswap16(u);
    } else if constexpr (sizeof(T) == 4) {
        u = __builtin_bswap32(u);
    } else if constexpr (sizeof(T) == 8) {
        u = __builtin_bswap64(u);
    }
    return __builtin_bit_cast(T, u);
}

/* Reads the big-endian T stored at p. In the endian-trap build (FSA_ENDIAN_DIAG)
 * byte by byte, so that the trap's multi-byte load hook does not report the
 * helpers' own reads; otherwise one load and a swap. */
template <typename T>
inline T be_read_raw(const T* p)
{
#ifdef FSA_ENDIAN_DIAG
    typedef typename be_uint_<sizeof(T)>::type U;
    const volatile unsigned char* b = (const volatile unsigned char*)p;
    U u = 0;
    for (unsigned i = 0; i < sizeof(T); i++)
        u = (U)((u << 8) | b[i]);
    return __builtin_bit_cast(T, u);
#else
    return be_swap_value(*p);
#endif
}

template <typename T>
struct BE {
    T raw; /* the file's bytes, big-endian */

    BE() = default;
    BE(T v) : raw(be_swap_value(v)) {}
    BE& operator=(T v) { raw = be_swap_value(v); return *this; }
    operator T() const { return be_read_raw(&raw); }
    T get() const { return be_read_raw(&raw); }

    template <typename U> BE& operator+=(U v) { *this = (T)(get() + v); return *this; }
    template <typename U> BE& operator-=(U v) { *this = (T)(get() - v); return *this; }
    template <typename U> BE& operator|=(U v) { *this = (T)(get() | v); return *this; }
    template <typename U> BE& operator&=(U v) { *this = (T)(get() & v); return *this; }
    BE& operator++() { *this = (T)(get() + 1); return *this; }
    BE& operator--() { *this = (T)(get() - 1); return *this; }
    T operator++(int) { T o = get(); *this = (T)(o + 1); return o; }
    T operator--(int) { T o = get(); *this = (T)(o - 1); return o; }
};

/* A 32-bit offset in a file that the loader replaces with a pointer in place
 * (wasm32 pointers are 4 bytes, so the pointer fits where the offset was).
 * Before relocation it holds the big-endian offset; offset() reads it. After
 * relocation it holds a host pointer. */
template <typename T>
struct BEPtr {
    union {
        unsigned int rawOffset;
        T* ptr;
    };
    BEPtr() = default;
    unsigned int offset() const { return be_read_raw(&rawOffset); }
    void relocate(const void* base)
    {
        unsigned int o = offset();
        ptr = o ? (T*)((char*)base + o) : (T*)0;
    }
    operator T*() const { return ptr; }
    T* operator->() const { return ptr; }
    BEPtr& operator=(T* p) { ptr = p; return *this; }
};
static_assert(sizeof(void*) == 4, "BEPtr relocates in place: 32-bit pointers only");

/* A big-endian file offset declared as void* in the original struct (J3D blocks):
 * it reads as the host offset value in pointer form, which is what
 * JSUConvertOffsetToPtr(base, const void* offset) expects. */
struct BEOffset {
    unsigned int raw;
    operator void*() const { return (void*)(unsigned long)be_read_raw(&raw); }
    explicit operator unsigned int() const { return be_read_raw(&raw); }
    explicit operator unsigned long() const { return be_read_raw(&raw); }
};

/* Reads from unaligned byte pointers. */
inline unsigned short be16(const void* p)
{
    const unsigned char* b = (const unsigned char*)p;
    return (unsigned short)((b[0] << 8) | b[1]);
}
inline unsigned int be32(const void* p)
{
    const unsigned char* b = (const unsigned char*)p;
    return ((unsigned int)b[0] << 24) | ((unsigned int)b[1] << 16) | ((unsigned int)b[2] << 8) | b[3];
}
inline short be16s(const void* p) { return (short)be16(p); }
inline int be32s(const void* p) { return (int)be32(p); }
inline float bef32(const void* p) { return __builtin_bit_cast(float, be32(p)); }

/* Swap a host value in place (for structs swapped once at load). */
template <typename T>
inline void be_swap_inplace(T& v) { v = be_swap_value(v); }

#ifdef FSA_DIAG
#pragma clang attribute pop
#endif

#endif /* __cplusplus */

#endif
