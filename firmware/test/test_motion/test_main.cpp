// Motion and dowsing, played made-up hand movements: pio test -e native
#include <unity.h>
#include <math.h>
#include <stdlib.h>
#include "Motion.h"
#include "Dowse.h"

static const float D = Motion::DegToRad;
static const float Dt = 1.0f / 30;

void setUp() {}
void tearDown() {}

static void feed(Motion& m, float ax, float ay, float az, float gx, float gy, float gz, float secs,
                 int* flicks = nullptr, int* twists = nullptr, int* lifts = nullptr) {
  const float a[3] = {ax, ay, az}, g[3] = {gx, gy, gz};
  for (float t = 0; t < secs; t += Dt) {
    m.update(a, g, Dt);
    if (flicks) *flicks += m.flick;
    if (twists) *twists += m.twist;
    if (lifts) *lifts += m.lifted;
  }
}

static void test_still_on_a_desk() {
  Motion m;
  feed(m, 0, 0, 1, 0.002f, -0.001f, 0.001f, 3);
  TEST_ASSERT_TRUE(m.stillFor > 2.5f);
  TEST_ASSERT_TRUE(m.steadiness > 0.9f);
  TEST_ASSERT_TRUE(m.viewing());
  TEST_ASSERT_FALSE(m.faceDown());
}

static void test_walking_is_not_still() {
  Motion m;
  const float a0[3] = {0, 0.6f, 0.8f}, g0[3] = {0, 0, 0};
  m.update(a0, g0, Dt);
  float steadiest = 0;
  for (int i = 0; i < 150; i++) {   // footsteps: 2 a second, a quarter g up and down
    const float b = 0.25f * sinf(i * Dt * 6.2832f * 2);
    const float a[3] = {0.02f * sinf(i * 0.7f), 0.6f + 0.6f * b, 0.8f + 0.8f * b};
    const float g[3] = {8 * D * sinf(i * 0.9f), 6 * D * cosf(i * 0.5f), 5 * D * sinf(i * 1.3f)};
    m.update(a, g, Dt);
    if (i > 30 && m.steadiness > steadiest) steadiest = m.steadiness;
    TEST_ASSERT_TRUE(m.stillFor < 0.5f);
  }
  TEST_ASSERT_TRUE(steadiest < 0.4f);
}

static void test_face_down() {
  Motion m;
  feed(m, 0, 0, 1, 0, 0, 0, 0.5f);
  feed(m, 0, 0, -1, 0, 0, 0, 2.5f);
  TEST_ASSERT_TRUE(m.faceDown());
  TEST_ASSERT_TRUE(m.faceDownFor > 1.0f);
}

static void test_in_a_pocket_is_away() {
  Motion m;
  feed(m, 0, -1, 0, 0, 0, 0, 3);   // upright, screen facing the leg
  TEST_ASSERT_FALSE(m.viewing());
  TEST_ASSERT_TRUE(m.awayFor > 1.5f);
}

static void test_heading_follows_a_turn_however_its_held() {
  // Held tilted 45 degrees toward the face, turning right at 90 degrees a second.
  Motion m;
  const float s = sqrtf(0.5f);
  feed(m, 0, s, s, 0, 0, 0, 1);
  const float h0 = m.heading;
  const float r = 90 * D;
  feed(m, 0, s, s, -r * 0, -r * s, -r * s, 1.0f);
  const float turned = Motion::wrap(m.heading - h0);
  TEST_ASSERT_FLOAT_WITHIN(8 * D, 90 * D, turned);
}

static void test_a_flick_is_one_pulse() {
  Motion m;
  int flicks = 0, twists = 0;
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 1);
  feed(m, 0, 0.6f, 0.8f, -420 * D, 0, 0, 0.12f, &flicks, &twists);
  feed(m, 0, 0.6f, 0.8f, 300 * D, 0, 0, 0.1f, &flicks, &twists);
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 1, &flicks, &twists);
  TEST_ASSERT_EQUAL(1, flicks);
  TEST_ASSERT_EQUAL(0, twists);
}

static void test_a_twist_and_back_is_a_ward() {
  Motion m;
  int flicks = 0, twists = 0;
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 1);
  feed(m, 0, 0.6f, 0.8f, 0, 0, 320 * D, 0.15f, &flicks, &twists);
  feed(m, 0, 0.6f, 0.8f, 0, 0, -320 * D, 0.15f, &flicks, &twists);
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 1, &flicks, &twists);
  TEST_ASSERT_EQUAL(1, twists);
  TEST_ASSERT_EQUAL(0, flicks);
}

static void test_one_way_twist_is_nothing() {
  Motion m;
  int twists = 0;
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 1);
  feed(m, 0, 0.6f, 0.8f, 0, 0, 320 * D, 0.2f, nullptr, &twists);
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 1.5f, nullptr, &twists);
  TEST_ASSERT_EQUAL(0, twists);
}

static void test_dowsing_turn_fires_no_gestures() {
  Motion m;
  int flicks = 0, twists = 0;
  const float s = sqrtf(0.5f), r = 70 * D;
  feed(m, 0, s, s, 0, 0, 0, 1);
  feed(m, 0, s, s, 0, -r * s, -r * s, 6, &flicks, &twists);
  TEST_ASSERT_EQUAL(0, flicks + twists);
}

static void test_arm_swinging_at_your_side_casts_nothing() {
  // Hanging upright in a hand, swinging 250 degrees a second back and forth.
  Motion m;
  int flicks = 0, twists = 0;
  feed(m, 0, 1, 0, 0, 0, 0, 1);
  for (int i = 0; i < 300; i++) {
    const float sw = 250 * D * sinf(i * Dt * 6.2832f);
    const float a[3] = {0.1f * sinf(i * 0.2f), 1, 0.05f}, g[3] = {sw * 0.3f, 0, sw};
    m.update(a, g, Dt);
    flicks += m.flick;
    twists += m.twist;
  }
  TEST_ASSERT_EQUAL(0, flicks + twists);
}

static void test_picking_it_up_wakes_it() {
  Motion m;
  int lifts = 0;
  feed(m, 0, 0, 1, 0, 0, 0, 3, nullptr, nullptr, &lifts);
  TEST_ASSERT_EQUAL(0, lifts);
  feed(m, 0, 0.3f, 0.95f, 100 * D, 0, 0, 0.3f, nullptr, nullptr, &lifts);
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 1, nullptr, nullptr, &lifts);
  TEST_ASSERT_EQUAL(1, lifts);
}

// ---- Dowsing ----

static float sweep(Dowse& d, float bearing, float swing, int noise, float from, float to, unsigned seed) {
  srand(seed);
  d.reset();
  for (float h = from; h < to; h += 0.12f) {
    const int r = (int)lroundf(-66 + swing * cosf(h - bearing)) + (rand() % (2 * noise + 1)) - noise;
    d.add(Motion::wrap(h), r);
  }
  return d.bearing();
}

static void test_dowsing_finds_the_well() {
  int worst = 0;
  for (int k = 0; k < 24; k++) {
    Dowse d;
    const float truth = Motion::wrap(k * 0.27f);
    const float got = sweep(d, truth, 4.0f, 3, -3.1416f, 3.1416f, 100 + k);
    TEST_ASSERT_TRUE(d.ready());
    const int err = (int)fabsf(Motion::wrap(got - truth) / Motion::DegToRad);
    if (err > worst) worst = err;
    TEST_ASSERT_TRUE(d.contrast() > 3.0f);
  }
  TEST_ASSERT_TRUE_MESSAGE(worst <= 35, "dowsing missed by more than 35 degrees");
}

static void test_half_a_turn_is_not_enough() {
  Dowse d;
  sweep(d, 0, 4, 2, -1.5f, 1.5f, 3);
  TEST_ASSERT_FALSE(d.ready());
}

static void test_no_pull_shows_as_weak() {
  Dowse d;
  sweep(d, 0, 0, 1, -3.1416f, 3.1416f, 9);
  TEST_ASSERT_TRUE(d.ready());
  TEST_ASSERT_TRUE(d.contrast() < 2.5f);
}

static void test_slices_cover_the_circle() {
  TEST_ASSERT_EQUAL(0, Dowse::binOf(-3.14159f));
  TEST_ASSERT_EQUAL(Dowse::Bins - 1, Dowse::binOf(3.14159f));
  TEST_ASSERT_EQUAL(Dowse::Bins / 2, Dowse::binOf(0.01f));
  for (int b = 0; b < Dowse::Bins; b++) TEST_ASSERT_EQUAL(b, Dowse::binOf(Dowse::binCenter(b)));
}


// Footsteps, two a second, the Deck bobbing a quarter g: in a hand (`up`) or a pocket.
static void walk(Motion& m, float secs, float upY = 0.6f, float upZ = 0.8f, float bounce = 0.25f, float hz = 2) {
  for (float t = 0; t < secs; t += Dt) {
    const float b = bounce * sinf(m.t * 6.2832f * hz);
    const float a[3] = {0.02f * sinf(m.t * 21), upY * (1 + b), upZ * (1 + b)};
    const float g[3] = {8 * D * sinf(m.t * 27), 6 * D * cosf(m.t * 15), 5 * D * sinf(m.t * 39)};
    m.update(a, g, Dt);
  }
}

static void test_footsteps_are_walking() {
  Motion m;
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 1);
  TEST_ASSERT_FALSE(m.walking());
  walk(m, 1.2f);
  TEST_ASSERT_FALSE(m.walking());   // a couple of steps aren't a walk yet
  walk(m, 20);
  TEST_ASSERT_TRUE(m.walking());
  TEST_ASSERT_TRUE(m.walkFor > 15);
  walk(m, 20, -1, 0);   // in a pocket, upright
  TEST_ASSERT_TRUE(m.walking());
  TEST_ASSERT_FLOAT_WITHIN(4, 38, m.walkedRecently());
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 4);   // stopped and held still
  TEST_ASSERT_FALSE(m.walking());
  TEST_ASSERT_EQUAL_FLOAT(0, m.walkFor);
  TEST_ASSERT_TRUE(m.stillFor > 3);
  TEST_ASSERT_FLOAT_WITHIN(4, 38, m.walkTotal);
}

static void test_a_shaky_hand_or_a_bump_isnt_walking() {
  Motion m;
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 1);
  walk(m, 10, 0.6f, 0.8f, 0.04f, 8);   // a tremor
  TEST_ASSERT_FALSE(m.walking());
  walk(m, 0.3f);                       // one jolt
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 2);
  TEST_ASSERT_FALSE(m.walking());
  TEST_ASSERT_EQUAL_FLOAT(0, m.walkTotal);
}

static void test_a_walk_fades_from_the_last_five_minutes() {
  Motion m;
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 1);
  walk(m, 60);
  TEST_ASSERT_TRUE(m.walkedRecently() > 50);
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 200);
  TEST_ASSERT_TRUE(m.walkedRecently() > 50);   // still within five minutes
  feed(m, 0, 0.6f, 0.8f, 0, 0, 0, 130);
  TEST_ASSERT_EQUAL_FLOAT(0, m.walkedRecently());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_still_on_a_desk);
  RUN_TEST(test_walking_is_not_still);
  RUN_TEST(test_face_down);
  RUN_TEST(test_in_a_pocket_is_away);
  RUN_TEST(test_heading_follows_a_turn_however_its_held);
  RUN_TEST(test_a_flick_is_one_pulse);
  RUN_TEST(test_a_twist_and_back_is_a_ward);
  RUN_TEST(test_one_way_twist_is_nothing);
  RUN_TEST(test_dowsing_turn_fires_no_gestures);
  RUN_TEST(test_arm_swinging_at_your_side_casts_nothing);
  RUN_TEST(test_picking_it_up_wakes_it);
  RUN_TEST(test_dowsing_finds_the_well);
  RUN_TEST(test_half_a_turn_is_not_enough);
  RUN_TEST(test_no_pull_shows_as_weak);
  RUN_TEST(test_slices_cover_the_circle);
  RUN_TEST(test_footsteps_are_walking);
  RUN_TEST(test_a_shaky_hand_or_a_bump_isnt_walking);
  RUN_TEST(test_a_walk_fades_from_the_last_five_minutes);
  return UNITY_END();
}
