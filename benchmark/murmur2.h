//-----------------------------------------------------------------------------
// MurmurHash2 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#ifndef _MURMURHASH2_H_
#define _MURMURHASH2_H_

//-----------------------------------------------------------------------------
// Platform-specific functions and macros

// Microsoft Visual Studio

#if defined(_MSC_VER) && (_MSC_VER < 1600)

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;

// Other compilers

#else	// defined(_MSC_VER)

#include <stdint.h>

#endif // !defined(_MSC_VER)

//-----------------------------------------------------------------------------

uint32_t MurmurHash2(const void* key, int len, uint32_t seed);

//-----------------------------------------------------------------------------
// MurmurHash2, 64-bit versions, by Austin Appleby

// The same caveats as 32-bit MurmurHash2 apply here - beware of alignment 
// and endian-ness issues if used across multiple platforms.

// 64-bit hash for 64-bit platforms
uint64_t MurmurHash64A(const void* key, int len, uint64_t seed);

// 64-bit hash for 32-bit platforms
uint64_t MurmurHash64B(const void* key, int len, uint64_t seed);

//-----------------------------------------------------------------------------
// MurmurHash2A, by Austin Appleby

// This is a variant of MurmurHash2 modified to use the Merkle-Damgard 
// construction. Bulk speed should be identical to Murmur2, small-key speed 
// will be 10%-20% slower due to the added overhead at the end of the hash.

// This variant fixes a minor issue where null keys were more likely to
// collide with each other than expected, and also makes the function
// more amenable to incremental implementations.
uint32_t MurmurHash2A(const void* key, int len, uint32_t seed);

//-----------------------------------------------------------------------------
// MurmurHashNeutral2, by Austin Appleby

// Same as MurmurHash2, but endian- and alignment-neutral.
// Half the speed though, alas.
uint32_t MurmurHashNeutral2(const void* key, int len, uint32_t seed);


//-----------------------------------------------------------------------------
// MurmurHashAligned2, by Austin Appleby

// Same algorithm as MurmurHash2, but only does aligned reads - should be safer
// on certain platforms. 

// Performance will be lower than MurmurHash2
uint32_t MurmurHashAligned2(const void* key, int len, uint32_t seed);

//-----------------------------------------------------------------------------

#endif // _MURMURHASH2_H_
