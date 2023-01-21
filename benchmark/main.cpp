#include <cubez/cubez.h>
#include <cubez/utils.h>

#include <omp.h>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <glm/glm.hpp>

typedef std::vector<glm::vec3> Vectors;

struct PositionComponent {
  glm::vec2 p;
};

struct DirectionComponent {
  glm::vec2 dir;
};

struct ComflabulationComponent {
  float thingy;
  int dingy;
  bool mingy;
  std::string stringy;
};

qbComponent position_component;
qbComponent direction_component;
qbComponent comflabulation_component;

void move(qbInstance* insts, qbFrame* f) {
  PositionComponent* p;
  DirectionComponent* d;
  qb_instance_mutable(insts[0], &p);
  qb_instance_const(insts[1], &d);

  float dt = *(float*)(f->state);
  p->p += d->dir * dt;
}

void comflab(qbInstance* insts, qbFrame*) {
  ComflabulationComponent* comflab;
  qb_instance_mutable(insts[0], &comflab);
  comflab->thingy *= 1.000001f;
  comflab->mingy = !comflab->mingy;
  comflab->dingy++;
}

double create_entities_benchmark(uint64_t count, uint64_t /** iterations */) {
  qbTimer timer;
  qb_timer_create(&timer, 0);

  qb_timer_start(timer);
  qbEntityAttr attr;
  qb_entityattr_create(&attr);
  for (uint64_t i = 0; i < count; ++i) {    
    qbEntity entity;
    qb_entity_create(&entity, attr);
  }
  qb_loop(0, 0);
  qb_entityattr_destroy(&attr);
  qb_timer_stop(timer);

  
  double result = qb_timer_elapsed(timer);
  qb_timer_destroy(&timer);

  return result;
}

int64_t count = 0;
int64_t* Count() {
  return &count;
}

double iterate_unpack_one_component_benchmark(uint64_t count, uint64_t iterations) {
  qbTimer timer;
  qb_timer_create(&timer, 0);
  {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    PositionComponent p;
    qb_entityattr_addcomponent(attr, position_component, &p);

    for (uint64_t i = 0; i < count; ++i) {
      qbEntity entity;
      p.p.x++;
      qb_entity_create(&entity, attr);
    }

    qb_entityattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addconst(attr, position_component);
    qb_systemattr_setfunction(attr,
      [](qbInstance* instance, qbFrame*) {        

        PositionComponent p;
        qb_instance_const(*instance, &p);
        *Count() += (uint64_t)p.p.x ^ 0x12983;
      });

    qbSystem system;
    qb_system_create(&system, attr);
    qb_systemattr_destroy(&attr);
  }
  qbQueryComponent_ all[] = { {position_component} };

  qbQuery_ query
  {
    [](qbEntity entity, qbVar, qbVar) {
      PositionComponent p;
      qb_instance_find(position_component, entity, &p);
      *Count() += (uint64_t)p.p.x ^ 0x12983;
      return QB_QUERY_RESULT_CONTINUE;
    },

    sizeof(all) / sizeof(all[0]),
    all
  };

  qb_loop(0, 0);
  qb_timer_start(timer);
  for (uint64_t i = 0; i < iterations; ++i) {
    qb_loop(0, 0);
    //qb_query(&query, qbNil);
  }
  qb_timer_stop(timer);
  std::cout << "Count = " << *Count() << std::endl;

  double elapsed = qb_timer_elapsed(timer);
  qb_timer_destroy(&timer);
  return elapsed;
}

double coroutine_overhead_benchmark(uint64_t count, uint64_t iterations) {
  qbTimer timer;
  qb_timer_create(&timer, 0);

  *Count() = 0;
  for (auto i = 0; i < count; ++i) {
    qb_coro_sync([](qbVar) {
      for (;;) {
        *Count() += 1;
        qb_coro_yield(qbInt(0));
      }
      return qbInt(0);
    }, qbInt(0));
  }

  qb_timer_start(timer);
  qb_loop(0, 0);
  for (uint64_t i = 0; i < iterations; ++i) {
    qb_loop(0, 0);
  }
  qb_timer_stop(timer);

  std::cout << "Count = " << *Count() << std::endl;

  double elapsed = qb_timer_elapsed(timer);
  qb_timer_destroy(&timer);
  return elapsed;
}

template<class F>
void do_benchmark(const char* name, F f, uint64_t count, uint64_t iterations, uint64_t test_iterations) {
  std::cout << "Running benchmark: " << name << "\n";
  uint64_t elapsed = 0;
  for (uint64_t i = 0; i < test_iterations; ++i) {
    uint64_t before = elapsed;
    qbScene test_scene;
    qb_scene_create(&test_scene, "");
    qb_scene_activate(test_scene);

    elapsed += f(count, iterations) / iterations;

    qb_scene_destroy(&test_scene);
    if (before > elapsed) {
      std::cout << "overflow\n";
    }
  }
  std::cout << "Finished benchmark\n";
  std::cout << "Total elapsed: " << elapsed << "ns\n";
  std::cout << "Elapsed per iteration: " << elapsed / test_iterations << "ns\n";
  std::cout << "Total elapsed: " << (double)elapsed / 1e9 << "s\n";
  std::cout << "Elapsed per iteration: " << ((double)elapsed / 1e9) / test_iterations << "s\n";
  if (count) {
    std::cout << "Elapsed per iteration per obj: " << ((double)elapsed) / test_iterations / count << "ns\n";
  }
}

qbSchema pos_schema;
qbSchema vel_schema;
qbComponent pos_component;
qbComponent vel_component;

class Rand {
public:
  Rand(uint32_t seed) : x(seed) {}

  Rand() = default;
  Rand(Rand&& other) = default;
  Rand(const Rand& other) = default;
  Rand& operator=(const Rand& other) = default;
  Rand& operator=(Rand&& other) = default;

  uint32_t operator()() {
    uint32_t t;
    x ^= x << 16;
    x ^= x >> 5;
    x ^= x << 1;

    t = x;
    x = y;
    y = z;
    z = t ^ x ^ y;

    return z;
  }

private:
  uint32_t x = 0;
  uint32_t y = 362436069;
  uint32_t z = 521288629;
};

struct xorshift64_state {
  uint64_t a;
};

uint64_t xorshift64(struct xorshift64_state* state) {
  uint64_t x = state->a;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  return state->a = x;
}

class Hash {
public:
  Hash(uint64_t seed) {
    cw_a = 1 + (seed % p1());
    cw_b = 1 + (seed % p2());
  }

  Hash() = default;
  Hash(Hash&& other) = default;
  Hash(const Hash& other) = default;
  Hash& operator=(const Hash& other) = default;
  Hash& operator=(Hash&& other) = default;

  uint64_t hash(uint64_t val) const {
    return (val * cw_a + cw_b) % cw_p();
  }

  uint64_t hash(const std::string& s) const {
    uint64_t res = 0;
    uint64_t x = 1;

    for (auto c : s) {
      res = (res + c * x) % p1();
      x = (x * p2()) % p1();
    }

    return hash(res);
  }

  uint64_t hash(const char* s) const {
    return hash((uint64_t)s);
  }

  constexpr inline uint64_t cw_p() const { return 2147483629; }
  constexpr inline uint64_t p1() const { return 4294967291; }
  constexpr inline uint64_t p2() const { return 65521; }
  uint64_t cw_a = 0;
  uint64_t cw_b = 0;
};

uint32_t primes[] = {
  2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131,137,139,149,151,157,163,167,173,179,181,191,193,197,199,211,223,227,229,233,239,241,251,257,263,269,271,277,281,283,293,307,311,313,317,331,337,347,349,353,359,367,373,379,383,389,397,401,409,419,421,431,433,439,443,449,457,461,463,467,479,487,491,499,503,509,521,523,541,547,557,563,569,571,577,587,593,599,601,607,613,617,619,631,641,643,647,653,659,661,673,677,683,691,701,709,719,727,733,739,743,751,757,761,769,773,787,797,809,811,821,823,827,829,839,853,857,859,863,877,881,883,887,907,911,919,929,937,941,947,953,967,971,977,983,991,997,1009,1013,1019,1021,1031,1033,1039,1049,1051,1061,1063,1069,1087,1091,1093,1097,1103,1109,1117,1123,1129,1151,1153,1163,1171,1181,1187,1193,1201,1213,1217,1223,1229,1231,1237,1249,1259,1277,1279,1283,1289,1291,1297,1301,1303,1307,1319,1321,1327,1361,1367,1373,1381,1399,1409,1423,1427,1429,1433,1439,1447,1451,1453,1459,1471,1481,1483,1487,1489,1493,1499,1511,1523,1531,1543,1549,1553,1559,1567,1571,1579,1583,1597,1601,1607,1609,1613,1619,1621,1627,1637,1657,1663,1667,1669,1693,1697,1699,1709,1721,1723,1733,1741,1747,1753,1759,1777,1783,1787,1789,1801,1811,1823,1831,1847,1861,1867,1871,1873,1877,1879,1889,1901,1907,1913,1931,1933,1949,1951,1973,1979,1987,1993,1997,1999,2003,2011,2017,2027,2029,2039,2053,2063,2069,2081,2083,2087,2089,2099,2111,2113,2129,2131,2137,2141,2143,2153,2161,2179,2203,2207,2213,2221,2237,2239,2243,2251,2267,2269,2273,2281,2287,2293,2297,2309,2311,2333,2339,2341,2347,2351,2357,2371,2377,2381,2383,2389,2393,2399,2411,2417,2423,2437,2441,2447,2459,2467,2473,2477,2503,2521,2531,2539,2543,2549,2551,2557,2579,2591,2593,2609,2617,2621,2633,2647,2657,2659,2663,2671,2677,2683,2687,2689,2693,2699,2707,2711,2713,2719,2729,2731,2741,2749,2753,2767,2777,2789,2791,2797,2801,2803,2819,2833,2837,2843,2851,2857,2861,2879,2887,2897,2903,2909,2917,2927,2939,2953,2957,2963,2969,2971,2999,3001,3011,3019,3023,3037,3041,3049,3061,3067,3079,3083,3089,3109,3119,3121,3137,3163,3167,3169,3181,3187,3191,3203,3209,3217,3221,3229,3251,3253,3257,3259,3271,3299,3301,3307,3313,3319,3323,3329,3331,3343,3347,3359,3361,3371,3373,3389,3391,3407,3413,3433,3449,3457,3461,3463,3467,3469,3491,3499,3511,3517,3527,3529,3533,3539,3541,3547,3557,3559,3571,3581,3583,3593,3607,3613,3617,3623,3631,3637,3643,3659,3671,3673,3677,3691,3697,3701,3709,3719,3727,3733,3739,3761,3767,3769,3779,3793,3797,3803,3821,3823,3833,3847,3851,3853,3863,3877,3881,3889,3907,3911,3917,3919,3923,3929,3931,3943,3947,3967,3989,4001,4003,4007,4013,4019,4021,4027,4049,4051,4057,4073,4079,4091,4093,4099,4111,4127,4129,4133,4139,4153,4157,4159,4177,4201,4211,4217,4219,4229,4231,4241,4243,4253,4259,4261,4271,4273,4283,4289,4297,4327,4337,4339,4349,4357,4363,4373,4391,4397,4409,4421,4423,4441,4447,4451,4457,4463,4481,4483,4493,4507,4513,4517,4519,4523,4547,4549,4561,4567,4583,4591,4597,4603,4621,4637,4639,4643,4649,4651,4657,4663,4673,4679,4691,4703,4721,4723,4729,4733,4751,4759,4783,4787,4789,4793,4799,4801,4813,4817,4831,4861,4871,4877,4889,4903,4909,4919,4931,4933,4937,4943,4951,4957,4967,4969,4973,4987,4993,4999,5003,5009,5011,5021,5023,5039,5051,5059,5077,5081,5087,5099,5101,5107,5113,5119,5147,5153,5167,5171,5179,5189,5197,5209,5227,5231,5233,5237,5261,5273,5279,5281,5297,5303,5309,5323,5333,5347,5351,5381,5387,5393,5399,5407,5413,5417,5419,5431,5437,5441,5443,5449,5471,5477,5479,5483,5501,5503,5507,5519,5521,5527,5531,5557,5563,5569,5573,5581,5591,5623,5639,5641,5647,5651,5653,5657,5659,5669,5683,5689,5693,5701,5711,5717,5737,5741,5743,5749,5779,5783,5791,5801,5807,5813,5821,5827,5839,5843,5849,5851,5857,5861,5867,5869,5879,5881,5897,5903,5923,5927,5939,5953,5981,5987,6007,6011,6029,6037,6043,6047,6053,6067,6073,6079,6089,6091,6101,6113,6121,6131,6133,6143,6151,6163,6173,6197,6199,6203,6211,6217,6221,6229,6247,6257,6263,6269,6271,6277,6287,6299,6301,6311,6317,6323,6329,6337,6343,6353,6359,6361,6367,6373,6379,6389,6397,6421,6427,6449,6451,6469,6473,6481,6491,6521,6529,6547,6551,6553,6563,6569,6571,6577,6581,6599,6607,6619,6637,6653,6659,6661,6673,6679,6689,6691,6701,6703,6709,6719,6733,6737,6761,6763,6779,6781,6791,6793,6803,6823,6827,6829,6833,6841,6857,6863,6869,6871,6883,6899,6907,6911,6917,6947,6949,6959,6961,6967,6971,6977,6983,6991,6997,7001,7013,7019,7027,7039,7043,7057,7069,7079,7103,7109,7121,7127,7129,7151,7159,7177,7187,7193,7207,7211,7213,7219,7229,7237,7243,7247,7253,7283,7297,7307,7309,7321,7331,7333,7349,7351,7369,7393,7411,7417,7433,7451,7457,7459,7477,7481,7487,7489,7499,7507,7517,7523,7529,7537,7541,7547,7549,7559,7561,7573,7577,7583,7589,7591,7603,7607,7621,7639,7643,7649,7669,7673,7681,7687,7691,7699,7703,7717,7723,7727,7741,7753,7757,7759,7789,7793,7817,7823,7829,7841,7853,7867,7873,7877,7879,7883,7901,7907,7919
};

class SmallMap {
public:
  SmallMap(const std::unordered_map<const char*, qbVar>& map) {
    assert(map.size() <= 64 && "Only map sizes <= 64 are supported.");

    map_size_ = find_closest_prime_size(map.size());
    data_ = std::vector<qbVar>(map_size_, qbNil);
    keys_ = std::vector<const char*>(map_size_, nullptr);

    xorshift64_state rand_state{ .a = 123456789 };
    std::vector<int> counts(map_size_, 0);
    for (uint64_t tries = 0; tries < 1000000; ++tries) {
      seed_ = xorshift64(&rand_state);
      Hash h(seed_);
      
      for (auto& [k, v] : map) {
        ++counts[h.hash(k) % map_size_];
      }

      bool try_again = false;
      for (auto count : counts) {
        if (count > 1) try_again = true;
      }

      if (try_again) {
        for (auto& count : counts) {
          count = 0;
        }
        continue;
      }
      else break;
    }

    for (auto count : counts) {
      assert(count <= 1);
    }

    hash_ = Hash(seed_);
    for (auto& [k, v] : map) {
      keys_[hash_.hash(k) % map_size_] = k;
      data_[hash_.hash(k) % map_size_] = v;
    }
  }
  SmallMap(const std::vector<std::pair<const char*, qbVar>>& map) {
    assert(map.size() <= 64 && "Only map sizes <= 64 are supported.");

    map_size_ = find_closest_prime_size(map.size());
    data_ = std::vector<qbVar>(map_size_, qbNil);
    keys_ = std::vector<const char*>(map_size_, nullptr);

    xorshift64_state rand_state{ .a = 123456789 };
    std::vector<int> counts(map_size_, 0);
    uint64_t tries = 0;
    for (tries = 0; ; ++tries) {
      seed_ = xorshift64(&rand_state);
      Hash h(seed_);
      
      for (auto& [k, v] : map) {
        ++counts[h.hash(k) % map_size_];
      }

      if (tries % 1000000 == 0) {
        std::cout << tries << "\n";
      }

      bool try_again = false;
      for (auto count : counts) {
        if (count > 1) {
          try_again = true;
          break;
        }
      }

      if (try_again) {
        for (auto& count : counts) {
          count = 0;
        }
        continue;
      } else {
        break;
      }
    }
    std::cout << tries << ", " << seed_ << std::endl;

    hash_ = Hash(seed_);
    for (auto& [k, v] : map) {
      keys_[hash_.hash(k) % map_size_] = k;
      data_[hash_.hash(k) % map_size_] = v;
    }
  }

  qbVar& operator[](const char* k) {
    return data_[hash_.hash(k) % map_size_];
  }

  const qbVar& operator[](const char* k) const {
    return data_[hash_.hash(k) % map_size_];
  }

  const qbVar& at(const char* k) const {
    return data_[hash_.hash(k) % map_size_];
  }

  bool contains(const char* k) const {
    return keys_[hash_.hash(k) % map_size_] == k;
  }

private:
  size_t find_closest_prime_size(size_t val) {
    static const size_t num_primes = sizeof(primes) / sizeof(primes[0]);

    uint32_t double_val = (uint32_t)(val * 4);
    for (size_t i = 0; i < num_primes; ++i) {
      if (double_val < primes[i]) {
        return (size_t)primes[i];
      }
    }

    return (size_t)double_val;
  }

  size_t map_size_;
  std::vector<qbVar> data_;
  std::vector<const char*> keys_;
  uint64_t seed_;
  Hash hash_;
};

double small_map_creation_benchmark(uint64_t count, uint64_t iterations) {
  qbTimer timer;
  qb_timer_create(&timer, 0);

  std::vector<const char*> keys;
  int num_keys = count;

  uint64_t accum = 0;
  double elapsed = 0.0;
  for (int iteration = 0; iteration < iterations; ++iteration) {
    keys = {};
    for (int i = 0; i < num_keys; ++i) {
      keys.push_back((const char*)((uint64_t)rand() << 48 | (uint64_t)rand() << 32 | (uint64_t)rand() << 16 | rand()));
    }

    std::vector<std::pair<const char*, qbVar>> map;
    for (auto k : keys) {
      map.push_back({ k, qbInt(rand()) });
    }

    qb_timer_start(timer);
    SmallMap small_map(map);
    qb_timer_stop(timer);
    elapsed += qb_timer_elapsed(timer);

    for (auto k : keys) {
      accum += small_map[k].i;
    }
  }
  std::cout << accum << std::endl;

  qb_timer_destroy(&timer);
  return elapsed;
}

double small_map_lookup_benchmark(uint64_t count, uint64_t iterations) {
  qbTimer timer;
  qb_timer_create(&timer, 0);

  std::vector<const char*> keys;
  int num_keys = count;

  uint64_t accum = 0;
  keys = {};
  for (int i = 0; i < num_keys; ++i) {
    keys.push_back((const char*)rand());
  }

  std::vector<std::pair<const char*, qbVar>> map;
  for (auto k : keys) {
    map.push_back({ k, qbInt(rand()) });
  }

  SmallMap small_map(map);
    
  xorshift64_state state{ .a = (uint64_t)(rand()) << 32 | (uint64_t)rand() };
  double elapsed = 0.0;
  for (int iteration = 0; iteration < iterations; ++iteration) {
    const char* key = keys[xorshift64(&state) % keys.size()];
    qb_timer_start(timer);
    accum += small_map[key].i;
    qb_timer_stop(timer);
    elapsed += qb_timer_elapsed(timer);
  }

  std::cout << accum << std::endl;

  qb_timer_destroy(&timer);
  return elapsed;
}

int main() {
  std::cout << "Number of processors: " << omp_get_max_threads() << std::endl;
  omp_set_num_threads(omp_get_max_threads());
  //omp_set_num_threads(1);

  qbUniverse uni;
  qbUniverseAttr_ attr = {};
  attr.enabled = (qbFeature)(QB_FEATURE_GAME_LOOP | QB_FEATURE_LOGGER);
  
  qbScriptAttr_ script_attr = {};
  attr.script_args = &script_attr;

  qb_init(&uni, &attr);
  qb_start();

  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, PositionComponent);

    qb_component_create(&position_component, "Pos", attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, DirectionComponent);
    qb_component_create(&direction_component, "Direction", attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, ComflabulationComponent);
    qb_component_create(&comflabulation_component, "Comfabulation", attr);
    qb_componentattr_destroy(&attr);
  }
  uint64_t count = 1000000;
  uint64_t iterations = 500;
  uint64_t test_iterations = 1;
#if 0
  pos_component = qb_component_find("Position", &pos_schema);
  vel_component = qb_component_find("Velocity", &vel_schema);

  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addconst(attr, pos_component);
    qb_systemattr_addconst(attr, vel_component);
    qb_systemattr_setfunction(attr, [](qbInstance* insts, qbFrame*) {
      std::pair<double, double>* p;
      qb_instance_const(insts[0], &p);

      std::pair<double, double>* v;
      qb_instance_const(insts[1], &v);
      
      p->first += v->first;
      p->second += v->second;
    });
    qbSystem s;
    qb_system_create(&s, attr);

    qb_systemattr_destroy(&attr);
  }




  
  qbTimer timer;
  qb_timer_create(&timer, 0);
  
  qb_loop(0, 0);
  double elapsed = 0.0;
  for (uint64_t i = 0; i < iterations; ++i) {
    qb_timer_start(timer);
    qb_loop(0, 0);
    qb_timer_stop(timer);
    double e = qb_timer_elapsed(timer);
    elapsed += e;
    std::cout << i << "] " << e / 1e9 << "s, " << elapsed / i / count << ", " << e / count << std::endl;
  }
  
  std::cout << "Total elapsed: " << (double)elapsed / 1e9 << "s\n";
  std::cout << "Elapsed per iteration: " << ((double)elapsed / 1e9) / iterations << "s\n";
  std::cout << "Elapsed per object: " << ((double)elapsed / 1e9) / iterations / count << "s\n";
  std::cout << "Elapsed per object: " << ((double)elapsed) / iterations / count << "ns\n";
  qb_timer_destroy(&timer);
#endif  

  
  //do_benchmark("Create entities benchmark",
  //             create_entities_benchmark, count, iterations, 1);
  /*do_benchmark("Unpack one component benchmark",
    iterate_unpack_one_component_benchmark, count, iterations, test_iterations);*/
  /*do_benchmark("coroutine_overhead_benchmark",
               coroutine_overhead_benchmark, 1, 1000000, 1);*/

  for (int num_keys = 64; num_keys <= 64; ++num_keys) {
    std::cout << "num_keys = " << num_keys << std::endl;
    do_benchmark("small_map_creation_benchmark",
                 small_map_creation_benchmark, num_keys, 10, 1);
    std::cout << std::endl;
  }

  int num_keys = 64;
  std::cout << "num_keys = " << num_keys << std::endl;
  do_benchmark("small_map_lookup_benchmark", small_map_lookup_benchmark, num_keys, 1000, 100);
  

  qb_stop();
  while (1);
}

/*
// Hypothesis: accessing the _fields table is slowing down due to hashmap lookup and GC.
Total elapsed: 11.992s
Elapsed per iteration: 1.1992s
Elapsed per object: 1.1992e-05s
Elapsed per object: 11992ns

// 

*/

/*
Before
#1
Total elapsed: 462616369ns
Elapsed per iteration: 462616369ns
Total elapsed: 0.462616s
Elapsed per iteration: 0.462616s

#2
Total elapsed: 480803490ns
Elapsed per iteration: 480803490ns
Total elapsed: 0.480803s
Elapsed per iteration: 0.480803s

#3
Total elapsed: 456288546ns
Elapsed per iteration: 456288546ns
Total elapsed: 0.456289s
Elapsed per iteration: 0.456289s

#4
Total elapsed: 461743162ns
Elapsed per iteration: 461743162ns
Total elapsed: 0.461743s
Elapsed per iteration: 0.461743s

#5
Total elapsed: 457279816ns
Elapsed per iteration: 457279816ns
Total elapsed: 0.45728s
Elapsed per iteration: 0.45728s


After
#1
Total elapsed: 40476674ns
Elapsed per iteration: 40476674ns
Total elapsed: 0.0404767s
Elapsed per iteration: 0.0404767s

#2
Total elapsed: 43829517ns
Elapsed per iteration: 43829517ns
Total elapsed: 0.0438295s
Elapsed per iteration: 0.0438295s

#3


#4


#5


After2
#1
Total elapsed: 66839651ns
Elapsed per iteration: 66839651ns
Total elapsed: 0.0668397s
Elapsed per iteration: 0.0668397s
*/