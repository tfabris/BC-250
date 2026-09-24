// ============================================================================
// Steam Puck Controller Wake
// ============================================================================
// Code for an ESP32 chip running in a special custom-designed system with the
// following hardware:
// 
// - BC-250 board, modified for use as a gaming computer.
// - 2026 Steam Controller and Charging puck.
// - ESP32 S2 Mini, model ESP32-S2FN4R2
// - TS3USB221E USB Multiplexer Board
// - Special electronics to connect to the BC-250 power controls.
// 
// See accompanying README.md for full details of the required hardware.
//
// This program does the following: 
// - Commands the USB Multiplexer to switch back and forth between "Monitor
//   Mode" and "Runtime Mode" as needed.
// - Monitor mode: Multiplexer "S" pin is pulled low. Multiplexer switches to
//   output #1 which is connected to the ESP32 board. The ESP32 board scans the
//   USB port for a 2026 Steam Controller puck and waits to see if there is
//   any data incoming from that controller.
// - Runtime mode: Multiplexer "S" pin is pulled High. Multiplexer switches to
//   output #2 which is connected to the USB port of the BC-250, and the
//   controller sends its data directly to the BC-250 via the multiplexer. The
//   ESP32 cannot see any data from the controller.
// - Watches the BC-250's power-on status to decide what the correct state of
//   the BC-250's power should be, based on controller data and the power-on
//   status.
// - In the correct situations, it sends a pulse to the BC-250's power button
//   pin to power on the machine when it sees data from the controller.
//   Also switches into Runtime Mode when the BC-250 is turned on.
// - In the correct situations, switches back into Monitor Mode, when the
//   BC-250's power is off.
//
// At the time of this writing, this code is made to speak only to a 2026-era
// Steam Controller Puck device. It could theoretically be modified to speak to
// other devices, but they would have to be USB devices, or wireless devices
// with a USB puck.
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include "usb/usb_host.h"
#include "esp_err.h"

// ==========================================
// GLOBAL VARIABLES
// ==========================================
// This section of code obtains secret strings from the build configuration,
// passed into this code with command line parameters during the build. To make
// your own secret strings, make a file called "secrets.ini" which looks like
// the below. Place the secrets.ini file in the same folder as platformio.ini.
//
//     [secrets]
//     wifi_ssid = "Your WiFi Network Name"
//     wifi_pass = "Your WiFi Network Password"
//     ota_pass = "Your Desired Firmware Update Password"
//
const char* WIFI_SSID = SECRET_WIFI_SSID;
const char* WIFI_PASSWORD = SECRET_WIFI_PASSWORD;
const char* OTA_PASSWORD = SECRET_OTA_PASSWORD;
const char* WIFI_HOSTNAME = "SteamPuckControllerWake"; 

#define LOG_VERBOSE 0
#define ONBOARD_LED 15
#define ONBOARD_LED_BRIGHTNESS 5
#define SELECT_PIN 14
#define POWER_SENSE_PIN 3
#define POWER_ON_PULSE_PIN 9
#define STEAM_CONTROLLER_VID 0x28DE
#define STEAM_CONTROLLER_PID 0x1304
#define MAIN_LOOP_DELAY_MS 10
#define MAX_LOG_LINES 150 // Number of most-recent log lines visible in the HTML log.
#define MAX_LOG_LINE_LENGTH 190 // Max length a single timestamped log line can be.

// The 2026 Steam Controller USB puck has five "interfaces" (low level
// communication endpoints) that it exposes to the USB system that speaks to
// it. They are numbered 2 through 6 (in my testing I could never talk to
// interface 1). Though we are supposed to be scanning all interfaces
// 2,3,4,5,6, and this code tries to do just that, a USB hardware limitation on
// the ESP32-S2 Mini prevents the code from scanning more than 3 interfaces. In
// my testing, it has been enough to scan interfaces 2,3 and 4 - The controller
// wakes the machine in that case. But in the future, if we find situations
// where there are multiple controllers connected at the same time, we may have
// to work around this limitation so that we can scan all six interfaces.
const uint8_t START_INTERFACE = 2;
const uint8_t END_INTERFACE = 6;
const uint8_t NUM_INTFS = 7; // Array boundary spanning 0 to 6 (zero indexed, so the number is 7)

AsyncWebServer server(80);
usb_host_client_handle_t client_hdl = NULL;
usb_device_handle_t dev_hdl = NULL;
uint8_t current_interface = 2; 
bool interface_is_claimed_map[NUM_INTFS] = { false };
uint8_t interface_ep_address_map[NUM_INTFS] = { 0 };
usb_transfer_t* interface_transfer_map[NUM_INTFS] = { NULL };
int selectPinLastLoggedState;
int sensePinLastLoggedState;
int selectPinReadState;
int sensePinReadState;
bool shouldReboot = false;
TaskHandle_t usbLibHandle = NULL;
TaskHandle_t usbClientHandle = NULL;
int logHead = 0;
int logCount = 0;
bool ledIsOn = false; 
unsigned long timeToWait = 0;
unsigned long lastLEDHeartbeat = 0;
unsigned long lastControllerDataReceived = 0;
unsigned long lastControllerDataReceivedCount = 0;
unsigned long lastControllerDataLogTime = 0;
unsigned long lastDiagTime = 0;
const unsigned long diagInterval = 2 * 60 * 1000;

// ============================================================================
// LOGGING SYSTEM
// ============================================================================
// Logging system, prints logs to an internal web page hosted on this ESP-32.
//
// The wake circuit uses the ESP32's built-in USB port for talking to the 2026
// Steam Controller puck through a USB multiplexer. So the ESP32's USB port is
// not available for monitoring the log output, and Serial features are not
// turned on. Instead, use this HTML logging system to print the logs to a
// small web page hosted on the ESP32 itself.
//
// This is a circular log, which displays only the most recent log lines.
// ============================================================================

// Need to allocate logBufferC at a fixed size, because when I used dynamic
// string lengths in the original circular logging code, it caused a RAM leak,
// which made this wake circuit stop working overnight.
char logBufferC[MAX_LOG_LINES][MAX_LOG_LINE_LENGTH]; 

// ============================================================================
// Main circular logging function.
// ============================================================================
void logMessage(const String &text)
{
  // Note that when printing the timestamp at the start of the line, it will
  // print a number that resets every 49.7 days because the millis() function
  // rolls over its index value that often.
  String timestamped = "[" + String(millis() / 1000.0, 2) + "s] " + text;
  snprintf(logBufferC[logHead], MAX_LOG_LINE_LENGTH, "%s", timestamped.c_str());
  logHead = (logHead + 1) % MAX_LOG_LINES;
  if (logCount < MAX_LOG_LINES)
  {
    logCount++;
  }
}

// ============================================================================
// Returns a large string containing the most recent logs. Used for building
// the HTML page.
// ============================================================================
String get_logs_html()
{
  // Trying to help prevent heap fragmentation by reserving the RAM initially.  
  String contents = "";
  contents.reserve(logCount * (MAX_LOG_LINE_LENGTH + 5)); 
  int current = (logHead - logCount + MAX_LOG_LINES) % MAX_LOG_LINES;
  for (int i = 0; i < logCount; i++)
  {
    contents += String(logBufferC[current]) + "\n";
    current = (current + 1) % MAX_LOG_LINES;
  }
  return contents;
}

// ============================================================================
// Logs certain things, only if the LOG_VERBOSE flag has been set to 1.
// ============================================================================
void logVerbose(const String &text)
{
  if (LOG_VERBOSE == 1)
  {
    logMessage(text);
  }   
}

// ============================================================================
// Logs the status of the Select and Power Sense GPIO pins and flags if either
// one has changed recently (three asterisks).
// ============================================================================
void logPinStatus(bool force = false)
{
  selectPinReadState = digitalRead(SELECT_PIN);
  sensePinReadState = digitalRead(POWER_SENSE_PIN);
  if (force == true || selectPinReadState != selectPinLastLoggedState || sensePinReadState != sensePinLastLoggedState )
  {
    logMessage("Pin status, Power Sense: " + String(sensePinReadState  == HIGH ? "High/On     " : "Low/Off     ") + String(sensePinReadState != sensePinLastLoggedState ? " ***" : ""));
    logMessage("Pin status,  USB Select: " + String(selectPinReadState == HIGH ? "High/Runtime" : "Low/Monitor ") + String(selectPinReadState != selectPinLastLoggedState ? " ***" : ""));
  }
  selectPinLastLoggedState = selectPinReadState;
  sensePinLastLoggedState = sensePinReadState;    
}

// ============================================================================
// Logs a specific type of error found in the USB code, digs out the user-
// friendly string of the name of the error rather than just its number.
// ============================================================================
void logEspError(esp_err_t errorNumber, uint8_t interfaceNumber, String functionName)
{
  char error_string[64]; 
  snprintf(error_string, sizeof(error_string), "%s (0x%X)", esp_err_to_name(errorNumber), errorNumber);
  logMessage( String(interfaceNumber == 0 ? "In " : "Interface " + String(interfaceNumber) + ", in ") + functionName + ", error " + error_string );
}

// ============================================================================
// Intermittently logs a diagnostic line, mostly for making sure I don't have
// any more memory leaks.
// ============================================================================
void logDiagnostics()
{
  // Only print diagnostic data every N milliseconds.
  unsigned long currentMillis = millis();
  if (lastDiagTime > 0 && currentMillis - lastDiagTime < diagInterval) { return; }
  lastDiagTime = currentMillis;

  // freeHeap:     Free RAM. Smaller numbers are worse.
  // minFreeHeap:  The lowest that Free RAM has been since boot. Smaller numbers are worse.
  // maxAllocHeap: Largest contiguous block available to allocate. Smaller
  //               numbers are worse, indicating Fragmented memory.
  // nnnStack:     Minimum amount of remaining stack space from a specific task, in
  //               units of "Words" (4 bytes). Smaller numbers are worse. If one of these
  //               numbers starts to approach zero you run the risk of a stack overflow
  //               error.
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t minFreeHeap = ESP.getMinFreeHeap(); 
  uint32_t maxAllocHeap = ESP.getMaxAllocHeap(); 
  UBaseType_t loopStack = uxTaskGetStackHighWaterMark(NULL); // NULL measures the current-executing task, in this case, loop()
  UBaseType_t usbLibStack = uxTaskGetStackHighWaterMark(usbLibHandle); // Task handle of usb_host_lib_task
  UBaseType_t usbClientStack = uxTaskGetStackHighWaterMark(usbClientHandle); // Task handle of usb_client_task

  // Calculate Uptime.
  int64_t total_us = esp_timer_get_time();
  int64_t total_secs = total_us / 1000000;
  int days  = total_secs / 86400;
  int hours = (total_secs % 86400) / 3600;
  int mins  = (total_secs % 3600) / 60;
  int secs  = total_secs % 60;

  // Log the ESP32 chip temperature, to ensure it's not overheating in the case.
  // Note that this is the temp of the ESP32 chip, not the temp of the BC-250.
  float chipTempCelsius = temperatureRead();

  // Log the GPIO pin status of the select and sense pins.
  selectPinReadState = digitalRead(SELECT_PIN);
  sensePinReadState = digitalRead(POWER_SENSE_PIN);

  // Print the diagnostic data.  
  char diagBuffer[200];
  snprintf(
    diagBuffer, sizeof(diagBuffer),
      "Uptime: %lud %luh %lum %lus | POW:%u SEL:%u | RAM: %u | Min RAM: %u | Frag: %u | Loop Stack: %u | USB Lib Stack: %u | USB Client Stack: %u | Temp: %.1f &deg;C",
      days, hours % 24, mins % 60, secs,
      sensePinReadState, selectPinReadState,
      freeHeap, minFreeHeap, maxAllocHeap, 
      loopStack, usbLibStack, usbClientStack,
      chipTempCelsius
      );
  logMessage(diagBuffer);
}

// ============================================================================
// USB DEVICE DRIVER
// ============================================================================
// Callback functions which handle the incoming information on the USB port.
// These are intended to communicate with the 2026 Steam Controller Puck only,
// and only get as far as finding out if there is data (any data) coming from
// that puck, and behave accordingly.

// ============================================================================
// Callback function which triggers when data is received from the USB device.
// This function performs the main important job of this code, which is to
// pulse the power-on pin when data is received, then switch from monitor mode
// to runtime mode.
// ============================================================================
static void hid_transfer_cb(usb_transfer_t *transfer)
{
  if (transfer->status == USB_TRANSFER_STATUS_COMPLETED)
  {
    uint8_t intf = (uint8_t)(uintptr_t)transfer->context;
    current_interface = intf;

    // Log that some data from the controller was received, but only log this
    // information every few seconds, to prevent filling the log with a billion
    // data messages. This uses a separate timestamp variable of its own.
    if (lastControllerDataLogTime == 0 || millis() - lastControllerDataLogTime >= 5000)
    {    
      lastControllerDataLogTime = millis();
      char status_msg[128];
      snprintf(status_msg, sizeof(status_msg), "============== CONTROLLER DATA RECEIVED - Interface %u ==============", current_interface);
      logMessage(status_msg);
    }

    // ========================================================================
    // BOOT FROM CONTROLLER
    // ========================================================================
    // If the machine's power is not on, and there was data found, it's time to
    // turn on the machine (by pulsing our power pin, which is connected to the
    // BC-250's power on button) and flip the USB multiplexer into runtime mode
    // so that the controller talks to the BC-250 instead of this ESP32 chip. 
    selectPinReadState = digitalRead(SELECT_PIN);
    sensePinReadState = digitalRead(POWER_SENSE_PIN);
    if (sensePinReadState == LOW && selectPinReadState == LOW && timeToWait < 1)
    {
      // Bugfix for GitHub Issue #1: Ensure that the data is a continuous stream
      // by checking the last time that data was received, and now many packets
      // we got recently. Only do stuff if a few recent data packets were
      // received.
      lastControllerDataReceivedCount ++;
      if (millis() - lastControllerDataReceived >= 100)
      { 
        // If the previous data was received a while ago, then consider this to
        // be the first data packet. Don't actually do anything at first.
        lastControllerDataReceived= millis();
        lastControllerDataReceivedCount = 0;
        logMessage("GitHub Issue #1 Bugfix: Early data packet received, taking no action yet.");
      }
      else
      {
        // Bugfix for GitHub Issue #1: If previous data was received recently,
        // then consider this to be part of a longer stream of data rather than
        // a one-time puck connection. Still, wait for more than just a single
        // data packet, or else the bug still reproduces. We have to wait for a
        // few of these packets to come through before counting this as the
        // controller actually being "on".
        lastControllerDataReceived= millis();
        if (lastControllerDataReceivedCount < 5)
        {
          logMessage("GitHub Issue #1 Bugfix: Early data packet received, waiting for more data.");
        }
        else
        {
          logMessage("Enough data has been received: Pulsing power on line.");
          digitalWrite(POWER_ON_PULSE_PIN, HIGH);
          delay(500);
          digitalWrite(POWER_ON_PULSE_PIN, LOW);

          // When first booting the machine, there must be a long pause after
          // the initial boot before switching the multiplexer into runtime
          // mode. If we don't wait, and switch to runtime mode before the
          // operating system is loaded up enough to talk to the USB puck,
          // then the puck just shuts itself down and thus the controller
          // shuts down again. This wait time keeps the multiplexer in
          // monitor mode while we wait for the OS to boot.
          logMessage("Waiting to switch USB to Runtime mode.");
          timeToWait = 15000;
        }
      }
    }

    // ========================================================================
    // POWER ON
    // ========================================================================
    // If the machine's power is on, and there was data found, but the
    // machine is still in monitoring mode, then flip the multiplexer into
    // runtime mode without touching the power-on button. By separating
    // this into its own section separate from the actual triggering of the
    // power button, this covers switching into runtime mode both in the
    // controller wake situation, and also the situation where the machine
    // was turned on independently from the controller.
    selectPinReadState = digitalRead(SELECT_PIN);
    sensePinReadState = digitalRead(POWER_SENSE_PIN);
    if (sensePinReadState == HIGH && selectPinReadState == LOW && timeToWait < 1)
    {
      logMessage("Power is on, switching USB to Runtime mode.");
      digitalWrite(SELECT_PIN, HIGH);
      delay(100);
    }

    // Resubmit the stream: Put this specific transfer handle back into the pool
    // to listen for the next packet.
    usb_host_transfer_submit(transfer);

    // At the current time, I don't need to see the bytes of data. Any data
    // at all from the controller counts as waking it up. Maybe in the
    // future this could be useful, so hanging onto this code just in case.
    //
    // String dataStr = "Data found on Interface " + String(current_interface) + "! [Bytes: ";
    // for (int i = 0; i < transfer->actual_num_bytes; i++)
    // {
    //     dataStr += String(transfer->data_buffer[i]) + (i < transfer->actual_num_bytes - 1 ? ", " : "");
    // }
    // dataStr += "]";
    // logMessage(dataStr);
  }
  else if (transfer->status == USB_TRANSFER_STATUS_CANCELED)
  {
    logVerbose("USB Transfer Status: USB_TRANSFER_STATUS_CANCELED");
  }
}

// ============================================================================
// Callback function to handle when the USB library detects that something new
// has happened on the USB port, such as a device being added or removed.
// ============================================================================
static void usb_client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
  // The main type of USB event that we need to handle is the
  // discovery of a new USB device on the port.
  if (event_msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV)
  {
    // Open the device and retrieve its information.
    logVerbose("Device Connection Registered. Initializing interfaces.");
    esp_err_t deviceOpenReturn = usb_host_device_open(client_hdl, event_msg->new_dev.address, &dev_hdl);
    if (deviceOpenReturn != ESP_OK) { logEspError(deviceOpenReturn, 0, "usb_host_device_open"); } else
    {
      const usb_device_desc_t *device_desc;
      esp_err_t getDescriptorReturn = usb_host_get_device_descriptor(dev_hdl, &device_desc);
      if (getDescriptorReturn != ESP_OK) { logEspError(getDescriptorReturn, 0, "usb_host_get_device_descriptor"); } else
      {
        // Check to make sure it's the device we're looking for. This code is
        // written specifically to work with one specific game controller.
        // Right now it is hard-coded to accept only a single brand and model
        // of controller. Theoretically it could be expanded in the future to
        // accept other controllers by modifying this.
        if (device_desc->idVendor == STEAM_CONTROLLER_VID && device_desc->idProduct == STEAM_CONTROLLER_PID)
        {
          logMessage("Steam Controller Puck Detected.");
          const usb_config_desc_t *config_desc;
          esp_err_t getActiveConfigDescriptorReturn = usb_host_get_active_config_descriptor(dev_hdl, &config_desc);
          if (getActiveConfigDescriptorReturn != ESP_OK) { logEspError(getActiveConfigDescriptorReturn, 0, "usb_host_get_active_config_descriptor"); } else
          {
            // The single USB device has a set of "interfaces", which are
            // individual communication channels available for sending and
            // receiving data. Scan through each possible interface to attach
            // to one. On my device, there are a limited number of
            // specifically-numbered interfaces that I have to cycle through.
            for (uint8_t intf_idx = START_INTERFACE; intf_idx <= END_INTERFACE; intf_idx++)
            {
              esp_err_t claimReturn = usb_host_interface_claim(client_hdl, dev_hdl, intf_idx, 0);
              if (claimReturn != ESP_OK) { logEspError(claimReturn, intf_idx, "usb_host_interface_claim"); } else
              {
                interface_is_claimed_map[intf_idx] = true;
                int offset = 0;
                const usb_intf_desc_t *intf = usb_parse_interface_descriptor(config_desc, intf_idx, 0, &offset);
                if (intf)
                {
                  const usb_standard_desc_t *next_desc = (const usb_standard_desc_t *)intf;
                  bool found_ep_for_this_intf = false;
                  while (next_desc != NULL && !found_ep_for_this_intf)
                  {
                    next_desc = usb_parse_next_descriptor(next_desc, config_desc->wTotalLength, &offset);

                    // Shortcut the loop.
                    if (next_desc == NULL || next_desc->bDescriptorType == 0x04) break;

                    // Check if it's the endpoint descriptor type (0x05).
                    if (next_desc->bDescriptorType == 0x05)
                    {
                      const usb_ep_desc_t *ep = (const usb_ep_desc_t *)next_desc;

                      // We must communicate with specific endpoint addresses
                      // inside each interface. 0x80 is testing if it's an "IN"
                      // endpoint. 0x03 is testing if it's an "Interrupt"
                      // transfer type.
                      if ((ep->bEndpointAddress & 0x80) && ((ep->bmAttributes & 0x03) == 0x03))
                      {
                        // Update our map of connected interfaces.
                        interface_ep_address_map[intf_idx] = ep->bEndpointAddress;
                        char ep_msg[128];
                        snprintf(ep_msg, sizeof(ep_msg), "Interface %u mapped to EP 0x%02X", intf_idx, ep->bEndpointAddress);
                        logMessage(ep_msg);
                        found_ep_for_this_intf = true; // Exits inner block walk safely.
                      }
                    }
                  }
                }
              }
            }
          }
        }
        else
        {
          // If the device that was connected to the USB port is not the VID and
          // PID of the specific game controller we wanted, then close it and
          // ignore the event.
          usb_host_device_close(client_hdl, dev_hdl);
          dev_hdl = NULL;
        }
      }
    }
  }

  // The other type of USB event that we need to handle is the removal of a USB
  // device from the port. TO DO: Ensure that this code is OK as-is. It doesn't
  // check to ensure that the device that was removed is the specific game
  // controller that we connected to in the first place, before looping through
  // and disconnecting from its endpoints. Should it? The way the hardware is
  // designed, in normal operation it should only ever plug into the one
  // controller that we want, so this might not be a big deal unless we expand
  // the design to accept other controllers.
  else if (event_msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE)
  {
    logMessage("Steam Controller Puck disconnected.");
    if (dev_hdl)
    {
      for (uint8_t i = START_INTERFACE; i <= END_INTERFACE; i++)
      {
        if (interface_transfer_map[i])
        {
          usb_host_transfer_free(interface_transfer_map[i]);
          interface_transfer_map[i] = NULL;
        }
        if (interface_is_claimed_map[i])
        {
          usb_host_interface_release(client_hdl, dev_hdl, i);
          interface_is_claimed_map[i] = false;
        }
      }
      usb_host_device_close(client_hdl, dev_hdl);
      dev_hdl = NULL;
    }
  }
}

// ============================================================================
// Generically handle USB events in the USB device driver. 
// ============================================================================
static void usb_host_lib_task(void *arg)
{
  while (1)
  {
    uint32_t event_flags;
    usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
  }
}

// ============================================================================
// Generically handle USB client events in the USB device driver. 
// ============================================================================
static void usb_client_task(void *arg)
{
  while (1)
  {
    usb_host_client_handle_events(client_hdl, portMAX_DELAY);
  }
}

// ============================================================================
// Initialize the USB device driver.
// ============================================================================
void init_usb_host_subsystem()
{
  logVerbose("Initializing ESP32-S2 USB Host Engine...");
  usb_host_config_t host_config =
  {
    .skip_phy_setup = false, .intr_flags = ESP_INTR_FLAG_LEVEL1
  };
  
  esp_err_t hostInstallReturn = usb_host_install(&host_config);
  if (hostInstallReturn != ESP_OK) { logEspError(hostInstallReturn, 0, "usb_host_install"); } else
  {
    usb_host_client_config_t client_config =
    {
      .is_synchronous = false, .max_num_event_msg = 5,
      .async = { .client_event_callback = usb_client_event_cb, .callback_arg = NULL }
    };
    esp_err_t clientRegisterReturn = usb_host_client_register(&client_config, &client_hdl);
    if (clientRegisterReturn != ESP_OK) { logEspError(clientRegisterReturn, 0, "usb_host_client_register"); } else
    {
      logMessage("USB Subsystem Online.");
      xTaskCreatePinnedToCore(usb_host_lib_task, "usb_lib", 4096, NULL, 2, &usbLibHandle, 0);
      xTaskCreatePinnedToCore(usb_client_task, "usb_client", 4096, NULL, 2, &usbClientHandle, 0);      
    }
  }
}


// ============================================================================
// WEB INTERFACE
// ============================================================================
// HTML Web interface for monitoring the logs. 
//
// The ESP32's USB port is occupied in this device, so we can't use it for log
// output. Instead, host an HTML web page here on the ESP32 which can use its
// built-in WiFi and you can view the logs there.
// ============================================================================


// ============================================================================
// Create the simple web HTML page which displays the ESP32 logs.
// ============================================================================
String build_html_page()
{
  String html = "";
  html += "<html><head>";
  html += "<title>Steam Puck Controller Wake Device - Logs</title>";
  html += "<style>";
  html += "body{font-family:monospace;background:#1e1e1e;color:#d4d4d4;padding:5px;} h1{color:#4fc1ff;}";
  html += "pre{background:#2d2d2d;padding:5px;border-radius:5px;display:inline-block;white-space:pre;overflow:visible;}";
  html += "td{background-color:#2d2d2d;text-align:center;vertical-align:middle;padding:5px 15px;border-radius:5px;}";
  html += "</style></head>";
  html += "<body><h1>Steam Puck Controller Wake Device - Logs</h1>";
  html += "<table><tr>";
  html += "<td><button onclick=\"if(confirm('Are you sure?')){fetch('/reboot', {method:'POST'})}\">Reboot ESP32</button><br /><small>(Reboots the Wake Device, not the console.)</small></td>";
  html += "<td style=\"width:10px;background-color:#1e1e1e;\"></td>";
  html += "<td><button onclick=\"if(confirm('Are you sure?')){fetch('/pulsePower', {method:'POST'})}\">Pulse Power Button</button><br /><small>(Presses the console power button.)</small></td>";
  html += "</tr></table>";
  html += "<pre id='log-viewer'>" + get_logs_html() + "</pre>";

  // Special javascript code that only updates the logs in the browser when they
  // have changed. This allows users to select and copy the log text without
  // their selection cursor disappearing every few seconds. Users may still
  // have a tough time if the logs are verbose and are updating quickly.
  html += "<script>"
          "let lastLogData = '';" // Save the displayed log, to test if it changed.
          ""
          "setInterval(function() {"
          "    fetch('/logs')"
          "        .then(response => response.text())"
          "        .then(data => {"
          "            /* Compare raw server data directly with the cached string */"
          "            if (data !== lastLogData) {"
          "                lastLogData = data;" // Update the saved version.
          "                document.getElementById('log-viewer').innerHTML = data;" // Update the text only when it changed.
          "            }"
          "        })"
          "        .catch(err => console.error('Error fetching logs:', err));"
          "}, 1000);" // Log refresh time in milliseconds.
          "</script>";
  html += "</body></html>";
  return html;
}

// ============================================================================
// Handle the main web page when a user surfs to it with their web browser.
// ============================================================================
void handle_root_request(AsyncWebServerRequest *request)
{
  request->send(200, "text/html", build_html_page());
}

// ============================================================================
// There is a "reboot" button on the web page which calls this function. It only
// reboots this Controller Wake circuit, it does not reboot the gaming console
// it's attached to.
// ============================================================================
void handle_reboot_request(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", "OK");
  shouldReboot = true;
}

// ============================================================================
// There is a "pulse" button on the web page which calls this function. It
// pulses the power button as if you had turned on the controller. Can be used
// to turn the BC-250 on and off remotely from WiFi.
// ============================================================================
void handle_pulse_request(AsyncWebServerRequest *request)
{
  request->send(200, "text/plain", "OK");
  logMessage("******************************************************************************");
  logMessage("***** Pulse Button Request Received from Web Page. Pulsing Power Button. *****");
  logMessage("******************************************************************************");
  digitalWrite(POWER_ON_PULSE_PIN, HIGH);
  delay(500);
  digitalWrite(POWER_ON_PULSE_PIN, LOW);  
}


// ============================================================================
// Initialize the WiFi connection.
// ============================================================================
void setup_networking()
{
  // Attempted bugfixes for GitHub issue #2. Try to add additional commands
  // which hopefully might help it stay connected to WiFi more stably. If these
  // do not fix the problem, implement a watchdog in the main loop which
  // monitors WiFi.status() != WL_CONNECTED and attempts a more serious
  // reconnect.
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);

  // Connect to WiFi using the password from "secrets.ini".
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  logMessage("Attempting Wi-Fi connection to: " + String(WIFI_SSID));

  // Wait for the initial WiFi connection for up to 15 seconds (200ms delay
  // inside the loop). Even after this loop, it will still try to connect
  // and/or reconnect. Blink the LED faster while it is still trying to connect
  // to WiFi. This way, while looking at the board, I can see how quickly it
  // connects to WiFi without having to look at the log file in the browser.
  // TODO: if I ever implement a WiFi watchdog, make this part of the watchdog
  // instead of blocking the startup (simply blink the LED faster whenever the
  // WiFi is not connected).
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 75)
  {
    analogWrite(ONBOARD_LED, ONBOARD_LED_BRIGHTNESS);
    ledIsOn = true; 
    delay(100);
    analogWrite(ONBOARD_LED, 0);
    ledIsOn = false; 
    delay(100);
    timeout++;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    logMessage("Wi-Fi Connection online.");
  }
  else
  {
    logMessage("Wi-Fi Link registration failed. Operating in hardware fallback loop.");
  }

  // Start the web server regardless of whether WiFi successfully connected.
  // That way, if the WiFi eventually connects, it'll be there waiting. This
  // fixes a small secondary part of GitHub issue #2, where I noticed that, in
  // one case, the ESP32 connected to the WiFi but I couldn't see its web
  // page.
  if (MDNS.begin(WIFI_HOSTNAME))
  {
    logVerbose("mDNS identity registered: http://" + String(WIFI_HOSTNAME) + ".local");
  }
  server.on("/", HTTP_GET, handle_root_request);
  server.on("/reboot", HTTP_POST, handle_reboot_request);
  server.on("/pulsePower", HTTP_POST, handle_pulse_request);
  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(200, "text/plain", get_logs_html()); });
  server.begin();
  logMessage("Logging Service online at http://" + String(WIFI_HOSTNAME) + ".local");

  // Activate OTA WiFi Firmware Updates. Note that I must set the hostname yet
  // again here, because for some reason, the OTA functions trounce on the host
  // name that I already set before.
  ArduinoOTA.setHostname(WIFI_HOSTNAME); 
  ArduinoOTA.setPort(3232);
  ArduinoOTA.setPassword(OTA_PASSWORD);    
  ArduinoOTA.begin();
  logMessage("OTA Firmware Updates online.");
}

// ============================================================================
// OTA FIRMWARE UPDATE FUNCTIONS
// ============================================================================
// Functions for updating the ESP32's running firmware over its built-in Wifi
// link, so you don't have to connect a USB cable to it to burn new firmware.
// 
// HOWEVER - The very first time you program the ESP32, since it doesn't have
// the code below running on it, you still have to use the USB cable the FIRST
// time. See the README.md file accompanying this code for full details.
int lastPrintedFirmwareUploadProgress = -1;

// ============================================================================
// Function which gets called when an OTA firmware update starts.
// ============================================================================
void onOtaStart()
{
  String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
  logMessage("Starting OTA firmware update: Updating " + type + "...");
  lastPrintedFirmwareUploadProgress = -1;
}

// ============================================================================
// Function which gets called when an OTA firmware update finishes.
// ============================================================================
void onOtaEnd()
{
  logMessage("Update Complete.");
}

// ============================================================================
// Intermittently log the OTA firmware update process.
// ============================================================================
void onOtaProgress(unsigned int progress, unsigned int total)
{
  // Only log progress every 20 percent.
  int progressPercentage = progress / (total / 100);
  if (progressPercentage % 20 == 0 && progressPercentage != lastPrintedFirmwareUploadProgress)
  {
    logMessage("Firmware upload progress: " + String(progressPercentage) + "%");
    lastPrintedFirmwareUploadProgress = progressPercentage; 
  }
}

// ============================================================================
// Log OTA firmware update errors.
// ============================================================================
void onOtaError(ota_error_t error)
{
  logMessage("OTA firmware update error: " + String(error));
  if (error == OTA_AUTH_ERROR) logMessage("Auth Failed");
  else if (error == OTA_BEGIN_ERROR) logMessage("Begin Failed");
  else if (error == OTA_CONNECT_ERROR) logMessage("Connect Failed");
  else if (error == OTA_RECEIVE_ERROR) logMessage("Receive Failed");
  else if (error == OTA_END_ERROR) logMessage("End Failed");
}

// ============================================================================
// MAIN PROGRAM CODE - SETUP
// This code is called once when the ESP32 boots up.
// ============================================================================
void setup()
{
  logMessage("System Startup.");

  // I have been told to try setting these output pins to low BEFORE running
  // PinMode, as an attempt to prevent brief spikes to high when first powering
  // on. Not sure if this is truly needed or not.

  // Switch Multiplexer into Monitoring state at startup.
  digitalWrite(SELECT_PIN, LOW);

  // The Power On Pulse pin should be "off" at startup.
  digitalWrite(POWER_ON_PULSE_PIN, LOW);

  // Prepare GPIO pins, starting with the heartbeat LED.
  pinMode(ONBOARD_LED,             OUTPUT);
  pinMode(SELECT_PIN,              OUTPUT);
  pinMode(POWER_ON_PULSE_PIN,      OUTPUT);
  pinMode(POWER_SENSE_PIN, INPUT_PULLDOWN);

  // Register the OTA Update functions.
  ArduinoOTA.onStart(onOtaStart);
  ArduinoOTA.onEnd(onOtaEnd);
  ArduinoOTA.onProgress(onOtaProgress);
  ArduinoOTA.onError(onOtaError);

  // Initialize the other systems.
  setup_networking();
  init_usb_host_subsystem();
  
  // Force the first pin status report at startup.
  logPinStatus(true);
}


// ============================================================================
// MAIN PROGRAM CODE - LOOP
// This code loops repeatedly while the ESP32 is turned on.
// ============================================================================
void loop()
{
  if (timeToWait > 0)
  {
    timeToWait -= MAIN_LOOP_DELAY_MS;
    if (timeToWait % 1000 == 0)
    {
      logMessage("Time to Wait: " + String(timeToWait));
    }
  }

  if (shouldReboot)
  {
    logMessage("******************************************************************************");
    logMessage("******************************* REBOOT IMMINENT ******************************");
    logMessage("******************************************************************************");
    delay(3000);
    ESP.restart();
  }  

  // Heartbeat LED - This blinks the ESP32 onboard LED, dimly, every
  // second, so you can tell if the main loop is running.
  if (millis() - lastLEDHeartbeat > 500)
  {
    lastLEDHeartbeat = millis();
    if (!ledIsOn)
    {
      analogWrite(ONBOARD_LED, ONBOARD_LED_BRIGHTNESS);
      ledIsOn = true;
    }
    else
    {
      analogWrite(ONBOARD_LED, 0);
      ledIsOn = false;
    }
  }

  // Keep listening for OTA firmware update flags, on every main program loop.
  ArduinoOTA.handle(); 

  // Log the state of pins each time through the loop, but only if they have changed.
  logPinStatus();

  // ==========================================================================
  // SHUTDOWN
  // ==========================================================================
  // If the machine's power is off, and we are in runtime mode, put the
  // multiplexer back into monitoring mode. This is for the situations when the
  // user shuts down the machine (either from the controller or from the power
  // button, both of those look the same to this ESP-32). This way, the ESP32
  // can start searching for data on the controller again, to give it a chance
  // to power the machine back on, if the user wants to.
  selectPinReadState = digitalRead(SELECT_PIN);
  sensePinReadState = digitalRead(POWER_SENSE_PIN);
  if (sensePinReadState == LOW && selectPinReadState == HIGH && timeToWait < 1)
  {
    // There is a weird behavior with the controller, where, if you switch to
    // monitor mode instantly after the machine turns off, then the controller
    // stays on for a while and causes data to appear in monitor mode, thus
    // waking the machine back up again. There needs to be a pause after the
    // machine shuts down, so that the controller has time to turn itself off,
    // before we switch back to monitor mode.
    logMessage("Power is off, USB is in Runtime mode. Pausing before switching USB to Monitor mode.");
    delay(5000);

    // If the power is still off after the wait, then go ahead and switch to monitor mode.
    selectPinReadState = digitalRead(SELECT_PIN);
    sensePinReadState = digitalRead(POWER_SENSE_PIN);        
    if (sensePinReadState == LOW && selectPinReadState == HIGH && timeToWait < 1)
    {
      logMessage("Switching USB to Monitor mode.");
      digitalWrite(SELECT_PIN, LOW);
    }
    else
    {
      logMessage("Monitor mode canceled.");
    }
  }

  // Log RAM diagnostics (at timed intervals) to help diagnose memory leaks.
  logDiagnostics();  

  // Loop through all claimed interfaces and submit their transfers.
  // They will sit quietly in the background waiting for hardware data.
  if (dev_hdl != NULL)
  {
    for (uint8_t i = START_INTERFACE; i <= END_INTERFACE; i++)
    {
      if (interface_is_claimed_map[i] && interface_ep_address_map[i] != 0)
      {
        // Allocate only if it has never been allocated before
        if (interface_transfer_map[i] == NULL)
        {
          usb_host_transfer_alloc(64, 0, &interface_transfer_map[i]);
          if (interface_transfer_map[i])
          {
            interface_transfer_map[i]->device_handle = dev_hdl;
            interface_transfer_map[i]->callback = hid_transfer_cb;
            interface_transfer_map[i]->bEndpointAddress = interface_ep_address_map[i];
            interface_transfer_map[i]->num_bytes = 64;
            interface_transfer_map[i]->context = (void*)(uintptr_t)i; // Pass interface number as context

            // Submit the transfer to the background engine, only once.
            esp_err_t err = usb_host_transfer_submit(interface_transfer_map[i]);
            if (err == ESP_OK)
            {
              char setup_msg[64];
              snprintf(setup_msg, sizeof(setup_msg), "Asynchronous stream active on Interface %u", i);
              logMessage(setup_msg);
            }
            else
            {
              logEspError(err, i, "usb_host_transfer_submit");
            }
          }
        }
      }
    }
  }

  // Keep ESP32 from spinning too hard, by slightly slowing the main loop.
  delay(MAIN_LOOP_DELAY_MS); 
}
