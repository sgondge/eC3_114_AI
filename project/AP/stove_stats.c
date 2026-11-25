#include "stove_stats.h"
#include "payload.h"

// Tunable filters
#define AG_MIN 0.05f
#define AG_MAX 20.0f
#define VG_MIN 200.0f
#define VG_MAX 300.0f

static struct {
  float avgA, avgV, avgP;
  uint32_t n;
} s;

static inline bool in_range(float x, float lo, float hi) { return (bool)(x >= lo && x <= hi); }

void pg_avg_reset(void) { s.avgA = s.avgV = s.avgP = 0.0f; s.n = 0; }

// Feed one sample. Computes running averages and writes ONLY averages to globalPayload.
void pg_accept_sample(float ag, float vg)
{
  // Repair occasional "missing-decimal" Ag like 3449219 -> 3.449219
  if (ag > 1e5f && ag < 1e7f) ag = ag / 1e6f;

  if (!in_range(vg, VG_MIN, VG_MAX)) return;

 float ag_effective = in_range(ag, AG_MIN, AG_MAX) ? ag : 0.0f;

  s.n++;
  float r = 1.0f / (float)s.n;
  s.avgA += (ag_effective - s.avgA) * r;
  s.avgV += (vg - s.avgV) * r;
  s.avgP += ((ag_effective * vg) - s.avgP) * r;

  globalPayload.stat.Ag = s.avgA;         // A
  globalPayload.stat.Vg = s.avgV;         // V
  globalPayload.stat.Pg = s.avgP;         // W (average instantaneous power)
	
//	printf("Ag: %f, Vg: %u, Pg: %f\n",globalPayload.stat.Ag, globalPayload.stat.Vg, globalPayload.stat.Pg);
	
}
