# GforceBrake

## Overview

GforceBrakeModule is a motorcycle brake light controller built around the Seeed Studio XIAO nRF52840 Sense. The system uses the onboard accelerometer to detect vehicle deceleration and automatically activate a brake light output without requiring input from the motorcycle's existing brake switches.

The primary goal is to illuminate the brake light during:

* Engine braking
* Downshifting
* Rolling off the throttle
* Braking with either brake control
* Any other significant vehicle deceleration

The system is intended to provide additional visibility to following traffic while operating independently of the motorcycle's electrical braking controls.

---

# Hardware

## Controller

* Seeed Studio XIAO nRF52840 Sense

## Sensors

* Onboard IMU (accelerometer)
* External MPU6050 removed from design

## Outputs

### Brake Signal Output

| Function           | Pin |
| ------------------ | --- |
| Brake Light Signal | D4  |

This output drives a transistor or relay that controls the actual brake light circuit.

### External RGB Indicator

| Color | Pin |
| ----- | --- |
| Red   | D5  |
| Green | D6  |
| Blue  | D7  |

### Onboard RGB LED

The onboard NeoPixel mirrors the external RGB LED state to provide visual debugging without requiring the external LED to be connected.

---

# Design Philosophy

The module is mounted in a fixed orientation on the motorcycle frame.

Assumptions:

* X-axis points forward along vehicle travel
* Device remains rigidly attached to frame
* Significant negative X-axis acceleration corresponds to deceleration

Because orientation is fixed, the system does not need full inertial navigation or dynamic attitude estimation.

Instead it focuses on reliable longitudinal acceleration detection.

---

# State Machine

The system operates as a finite state machine.

## STARTUP

Purpose:

* Verify MCU operation
* Verify LED operation

Behavior:

* White LED self-test for 500 ms
* Initialize IMU
* Transition to WAITING_UPRIGHT

Indicator:

* Blue

---

## WAITING_UPRIGHT

Purpose:

Prevent calibration while the motorcycle is resting on the kickstand.

Behavior:

* Monitor tilt angle
* Wait until motorcycle is approximately upright

Requirements:

```text
Tilt < uprightTiltDeg
```

Default:

```cpp
const float uprightTiltDeg = 10.0;
```

Indicator:

* Blue

---

## CALIBRATING

Purpose:

Determine the baseline acceleration offset.

Behavior:

* Require stable upright position
* Require minimal movement
* Capture longitudinal acceleration offset

Requirements:

```cpp
abs(filteredAccelX) < stableAccelBand
```

for at least:

```cpp
1000 ms
```

Indicator:

* Green

Result:

```cpp
zeroOffsetX
```

stores the calibrated baseline.

---

## IDLE

Purpose:

Monitor for braking events.

Behavior:

* Brake output OFF
* LEDs OFF

Continuously evaluates filtered longitudinal acceleration.

---

## BRAKING

Purpose:

Activate brake light.

Behavior:

* D4 HIGH
* Red indicator LEDs ON

Indicator:

* Red

Brake output remains active until acceleration exceeds the release threshold.

---

## FAULT

Purpose:

Indicate unrecoverable startup or sensor failure.

Behavior:

* Brake output disabled
* Fault indicator shown

Indicator:

* Blue

---

# Acceleration Processing

## Raw Sensor Readings

The onboard IMU provides:

```cpp
ax
ay
az
```

in units of g.

---

## Gravity Normalization

The gravity vector is calculated as:

```cpp
mag = sqrt(ax² + ay² + az²)
```

Normalized components:

```cpp
gx = ax / mag
gy = ay / mag
gz = az / mag
```

---

## Tilt Calculation

Tilt is estimated using:

```cpp
acos(gz)
```

This is used only to determine whether the motorcycle is upright enough to begin calibration.

---

## Linear Acceleration

The goal is to remove gravitational influence from the forward axis.

Current implementation:

```cpp
linearX = ax - (gx * mag)
```

This approximates longitudinal acceleration independent of static tilt.

---

## Filtering

A low-pass filter reduces vibration and sensor noise.

```cpp
filteredAccelX =
    alpha * linearX +
    (1 - alpha) * filteredAccelX;
```

Default:

```cpp
const float alpha = 0.2;
```

Lower values:

* More smoothing
* More delay

Higher values:

* Faster response
* More noise

---

# Brake Detection Logic

Brake detection is based on longitudinal deceleration.

## Trigger Threshold

```cpp
const float brakeThreshold = -0.18;
```

Approximately:

```text
0.18 g deceleration
```

required before a braking event can begin.

---

## Hold Time

To prevent false activation from bumps:

```cpp
const unsigned long brakeHoldTime = 120;
```

Acceleration must remain beyond the threshold continuously.

---

## Release Threshold

Hysteresis prevents flicker.

```cpp
const float brakeReleaseThreshold = -0.08;
```

Brake turns OFF only after acceleration rises above this value.

This prevents rapid state transitions near the threshold.

---

# Calibration Strategy

The module intentionally refuses calibration while:

* Leaned on kickstand
* Being moved
* Experiencing significant vibration

Calibration begins only when:

1. Motorcycle is upright
2. Motion is minimal
3. Conditions remain stable for one second

This prevents large orientation errors from becoming the reference baseline.

---

# LED Behavior

## Startup

| LED   | Meaning   |
| ----- | --------- |
| White | Self-test |

---

## Waiting Upright

| LED  | Meaning                                 |
| ---- | --------------------------------------- |
| Blue | Waiting for rider to upright motorcycle |

---

## Calibrating

| LED   | Meaning                  |
| ----- | ------------------------ |
| Green | Capturing zero reference |

---

## Idle

| LED | Meaning    |
| --- | ---------- |
| Off | Monitoring |

---

## Braking

| LED | Meaning             |
| --- | ------------------- |
| Red | Brake output active |

---

## Fault

| LED  | Meaning                          |
| ---- | -------------------------------- |
| Blue | Sensor or initialization failure |

---

# Future Improvements

Potential enhancements include:

## True Linear Acceleration

Use sensor fusion and gravity estimation from:

* Accelerometer
* Gyroscope

instead of accelerometer-only gravity estimation.

---

## Lean Angle Compensation

Compensate for cornering forces and rider lean angle.

---

## Adaptive Thresholds

Adjust braking sensitivity based on:

* Vehicle speed
* Road vibration
* Historical acceleration patterns

---

## Brake Flash Pattern

Implement:

* Triple flash
* Then steady ON

before entering continuous brake mode.

---

## Sensor Health Monitoring

Add:

* IMU watchdog
* Automatic sensor reinitialization
* Fault logging

---

# Safety Notes

This project is intended as an auxiliary brake light controller.

It should not replace factory brake switches or legally required lighting systems.

The module should be tested extensively in controlled conditions before road use.

Always verify operation under:

* Engine braking
* Hard braking
* Cornering
* Road vibration
* Startup on kickstand
* Startup while upright

before deployment.

