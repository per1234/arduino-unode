/*******************************************************************************
 * Copyright (c) 2018 Ioannis Charalampidis
 *
 * This is a private, preview release of the uNode hardware abstraction library.
 * The holder of a copy of this software and associated documentation files
 * (the "Software") is allowed to use the Software without any obligation to
 * create private and/or commercial projects. The Software can be obtained
 * through the official channels of the author, including but not limited to
 * Github and the official TLab.gr website. It is FORBIDDEN however to modify,
 * reverse-engineer, publish, distribute, sublicense, and/or sell copies of the
 * Software itself.
 *
 * The license for this file might change in a future release. The author is not
 * obliged to announce this change through any channel but it should be included
 * in the release notes.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *******************************************************************************/
#include <Arduino.h>
#include <SPI.h>
#include "Power.hpp"

#include "../Pinout.hpp"
#include "../Config.hpp"
#include "GPIO.hpp"
#include "LoRa.hpp"

extern "C" {
  #include "user_interface.h"
}

#define FPM_SLEEP_MAX_TIME 0xFFFFFFF

#define DEBUG_CONTEXT "uNode"
#include "../util/Debug.hpp"

/**
 * Initialize the singleton
 */
PowerClass Power;

/**
 * Configure the power system
 */
void PowerClass::begin() {

  // Disable power on VBus
  pinMode(UPIN_VBUS_EN, OUTPUT);
  digitalWrite(UPIN_VBUS_EN, LOW);

  // Make sure no current leaks to the RFM
  pinMode(UPIN_RFM_DIO1, OUTPUT);
  digitalWrite(UPIN_RFM_DIO1, LOW);
  pinMode(UPIN_RFM_DIO0, OUTPUT);
  digitalWrite(UPIN_RFM_DIO0, LOW);
  pinMode(UPIN_RFM_EN, OUTPUT);
  digitalWrite(UPIN_RFM_EN, LOW);

  // Make sure CS_RF is low
  pinMode(UPIN_CS_RF, OUTPUT);
  digitalWrite(UPIN_CS_RF, LOW);

  // Do not turn on WiFi
  wifi_station_disconnect();
  wifi_set_opmode(NULL_MODE);
  wifi_set_sleep_type(MODEM_SLEEP_T);
  wifi_fpm_open();
  wifi_fpm_do_sleep(FPM_SLEEP_MAX_TIME);

  // Initial state
  state.vbus = 0;
  state.gpio = 0;
  state.lora = 0;
  state.wifi = 0;
}

/**
 * Apply the state of the peripherals
 */
void PowerClass::apply() {
  if (state.gpio || state.lora) {
    logDebug("Enabling VBus");
    digitalWrite(UPIN_VBUS_EN, HIGH);
    state.vbus = 1;
  }

  else {
    logDebug("Disabling VBus");
    digitalWrite(UPIN_VBUS_EN, LOW);
    state.vbus = 0;
  }
}

/**
 * Enable/Disable LoRA
 */
void PowerClass::setLoRaRadio(uint8_t enabled) {
  if (enabled && !state.lora) {
    logDebug("Enabling LoRa");
    state.lora = 1;

    // If that's the first time the module powers up, we should
    // wait for 100 ms for it to initialize
    if (!state.vbus) {
      apply();
      delay(1000);
    }

    // Apply the initialization sequence on the chip
    LoRa.begin();
  }

  else if (!enabled && state.lora) {
    // Disable the subsystem
    logDebug("Disabling LoRa");
    LoRa.end();

    // Make sure no current flows through the RFM chip
    pinMode(UPIN_RFM_DIO1, OUTPUT);
    digitalWrite(UPIN_RFM_DIO1, LOW);
    pinMode(UPIN_RFM_DIO0, OUTPUT);
    digitalWrite(UPIN_RFM_DIO0, LOW);
    pinMode(UPIN_RFM_EN, OUTPUT);
    digitalWrite(UPIN_RFM_EN, LOW);

    // Disable module and VBus
    state.lora = 0;
    apply();
  }
}
uint8_t PowerClass::getLoRaRadio() {
  return state.lora;
}

/**
 * Enable/Disable GPIO
 */
void PowerClass::setGPIO(uint8_t enabled) {
  if (enabled && !state.gpio) {
    logDebug("Enabling GPIO Expansion");
    state.gpio = 1;

    // If that's the first time the module powers up, we should
    // wait for 100 ms for it to initialize
    if (!state.vbus) {
      apply();
      delay(100);
    }

    // Initialize GPIO chip
    GPIO.begin();
  }

  else if (!enabled && state.gpio) {
    logDebug("Disabling GPIO Expansion");
    state.gpio = 0;
    apply();
  }
}
uint8_t PowerClass::getGPIO() {
  return state.wifi;
}

/**
 * Enable/Disable WiFi Radio
 */
void PowerClass::setWiFiRadio(uint8_t newState) {
  if (state.wifi != newState) {

    // Turn off
    if (newState == 0) {
      logDebug("Disabling WiFi");
      wifi_station_disconnect();
      wifi_set_opmode(NULL_MODE);
      wifi_set_sleep_type(MODEM_SLEEP_T);
      wifi_fpm_open();
      wifi_fpm_do_sleep(FPM_SLEEP_MAX_TIME);
    }

    // Turn to station
    else if (newState == 1) {
      logDebug("Enabling WiFi");
      wifi_fpm_do_wakeup();
      wifi_fpm_close();
      wifi_set_opmode(STATION_MODE);
      wifi_station_disconnect();
    }

    // Mark the new state
    state.wifi = newState;
  }
}
uint8_t PowerClass::getWiFiRadio() {
  return state.wifi;
}

/**
 * Disable all peripherals
 */
void PowerClass::off() {
  // Stop SPI
  SPI.end();

  // Turn off all peripherals
  setLoRaRadio(0);
  setGPIO(0);
  setWiFiRadio(0);

  // Make sure we are leaking no power
  digitalWrite(UPIN_RFM_DIO1, LOW);
  digitalWrite(UPIN_RFM_DIO0, LOW);
  digitalWrite(UPIN_RFM_EN, LOW);
  digitalWrite(UPIN_VBUS_EN, LOW);
}