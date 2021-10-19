#include <cubez/random.h>

uint64_t rol64(uint64_t x, int k) {
	return (x << k) | (x >> (64 - k));
}

thread_local uint64_t seed;

QB_API void qb_seed(uint64_t s) {
	if (s == 0) {
		seed = (uint64_t)(rand()) << 32ull | (uint64_t)(rand());
	} else {
		seed = s;
	}
}

QB_API uint64_t qb_rand() {
	static thread_local qbXorshift_ state = { .s = seed };
	return qb_xorshift(&state);
}

uint64_t qb_splitmix(qbSplitmix state) {
	uint64_t result = (state->s += 0x9E3779B97f4A7C15);
	result = (result ^ (result >> 30)) * 0xBF58476D1CE4E5B9;
	result = (result ^ (result >> 27)) * 0x94D049BB133111EB;
	return result ^ (result >> 31);
}

uint64_t qb_xorshift(qbXorshift state) {
	uint64_t x = state->s;
	x ^= x << 13;
	x ^= x >> 7;
	x ^= x << 17;
	return state->s = x;
}

uint64_t qb_xorshift128p(qbXorshift128p state) {
	uint64_t t = state->s[0];
	const uint64_t s = state->s[1];

	state->s[0] = s;
	t ^= t << 23;		// a
	t ^= t >> 18;		// b -- Again, the shifts and the multipliers are tunable
	t ^= s ^ (s >> 5);	// c
	state->s[1] = t;
	return t + s;
}

uint64_t qb_xoshiro256ss(qbXoshiro256ss state) {
	uint64_t* s = state->s;
	uint64_t const result = rol64(s[1] * 5, 7) * 9;
	uint64_t const t = s[1] << 17;

	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];

	s[2] ^= t;
	s[3] = rol64(s[3], 45);

	return result;
}