#include "FreeRTOS.h"
#include "debug.h"
#include "log.h"
#include "param.h"
#include "stdint.h"
#include "task.h"

////////////////////////////////////////////////////////////////
// Utils
////////////////////////////////////////////////////////////////

static uint32_t popcount(uint32_t x) {
  uint32_t ret = 0;
  // naive method, but quick when we call it with small values
  while (x) {
    ret += x & 1;
    x >>= 1;
  }
  return ret;
}

////////////////////////////////////////////////////////////////
// Meat
////////////////////////////////////////////////////////////////

typedef enum { crossingBeamMethod = 0, sweepAngleMethod = 1 } Method;

static paramVarId_t paramIdLighthouseMethod;
static logVarId_t logIdLighthouseBaseStationActiveMap;

// The lighthouses (V1 and V2) only refresh at 60Hz, so it's overkill to
// refresh any faster than that. This job is computationally negligible, so
// no need to throttle. Ideally we'd hook into the lighthouse driver and just
// sync to that (or just integrate this logic there)
static const uint32_t interval = M2T(16);

static void useMethod(const Method method) {
  paramSetInt(paramIdLighthouseMethod, method);
}

//
// Switch between the lighthouse position estimator crossing beam and sweep
// angle methods according to the number of lighthouses in scope.
//
// Reading the lighthouse driver code reveals that this parameter is only
// switching how values are processed once received from the lighthouse deck
// (and not preparing for the next pulse) and that the processing is not
// stateful so switching hysteresis is unnecessary.
// TODO: ^ I believe this comment to be correct, but I could give the
// lighthouse driver a more thorough read.
//
// Not being coupled in the lighthouse driver does mean that there is one
// frame of latency between a lighthouse becoming inactive and method selection;
// the consequence is that this does the opposite of the desired behaviour when
// one lighthouse is toggling its activity every frame, obviously a
// pathological case that isn't going to happen in reality...
//
void appMain() {
  //
  // Initialization
  //
  paramIdLighthouseMethod = paramGetVarId("lighthouse", "method");
  logIdLighthouseBaseStationActiveMap = logGetVarId("lighthouse", "bsActive");
  if (!paramIdLighthouseMethod.id) {
    DEBUG_PRINT("Unable to get Lighthouse Method parameter!\n");
    ASSERT_FAILED();
  };
  if (!logIdLighthouseBaseStationActiveMap) {
    DEBUG_PRINT("Unable to get Lighthouse active basestation log item!\n");
    ASSERT_FAILED();
  };

  // oldMethod and newMethod are only here to allow debug printing on change
  Method oldMethod = sweepAngleMethod;
  useMethod(oldMethod);
  while (1) {
    const uint32_t numLighthouses =
        popcount(logGetUint(logIdLighthouseBaseStationActiveMap));
    // The important line :)
    const Method newMethod =
        numLighthouses > 1 ? crossingBeamMethod : sweepAngleMethod;
    useMethod(newMethod);

    if (newMethod != oldMethod) {
      DEBUG_PRINT("APP: Switching to %s\n", newMethod == crossingBeamMethod
                                                ? "crossing beam method"
                                                : "sweep angle method");
    }
    oldMethod = newMethod;

    // Rest a while
    vTaskDelay(interval);
  }
}
