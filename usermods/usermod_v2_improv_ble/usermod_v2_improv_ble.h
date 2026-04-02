#pragma once

#include "wled.h"

// Only compile if enabled and running on ESP32
#if defined(USERMOD_IMPROV_BLE) && defined(ESP32)

#ifndef USERMOD_ID_IMPROV_BLE
#define USERMOD_ID_IMPROV_BLE 97
#endif

#include <NimBLEDevice.h>

class UsermodImprovBLE : public Usermod {
  private:
    bool bleRunning = false;
    unsigned long lastCheck = 0;
    
    // Improv BLE UUIDs
    const char* IMPROV_SERVICE_UUID         = "00467768-6228-2272-4663-277478268000";
    const char* IMPROV_STATUS_UUID          = "00467768-6228-2272-4663-277478268001";
    const char* IMPROV_ERROR_UUID           = "00467768-6228-2272-4663-277478268002";
    const char* IMPROV_RPC_COMMAND_UUID     = "00467768-6228-2272-4663-277478268003";
    const char* IMPROV_RPC_RESULT_UUID      = "00467768-6228-2272-4663-277478268004";

    NimBLEServer* pServer = nullptr;
    NimBLEService* pImprovService = nullptr;
    NimBLECharacteristic* pStatusChar = nullptr;
    NimBLECharacteristic* pErrorChar = nullptr;
    NimBLECharacteristic* pRpcCommandChar = nullptr;
    NimBLECharacteristic* pRpcResultChar = nullptr;

    const uint8_t IMPROV_VERSION = 1;
    bool processingCommand = false;

    // Helper to send status notifications
    void updateStatus(uint8_t state) {
      if (pStatusChar) {
        pStatusChar->setValue(&state, 1);
        pStatusChar->notify();
      }
    }

    // Helper to send error notifications
    void updateError(uint8_t errorState) {
      if (pErrorChar) {
        pErrorChar->setValue(&errorState, 1);
        pErrorChar->notify();
      }
    }

    // Sends the payload to the RPC Result characteristic
    void sendRPCResult(uint8_t commandId, const std::vector<std::string>& dataSets) {
      if (!pRpcResultChar) return;
      
      uint8_t resultData[256];
      resultData[0] = commandId;
      
      uint8_t dataLenOffset = 1;
      uint8_t dataOffset = 2;
      uint8_t totalDataLen = 0;
      
      for (const auto& data : dataSets) {
        uint8_t len = data.length();
        if (dataOffset + 1 + len >= 256) break;
        resultData[dataOffset++] = len;
        memcpy(&resultData[dataOffset], data.c_str(), len);
        dataOffset += len;
        totalDataLen += (1 + len);
      }
      
      resultData[dataLenOffset] = totalDataLen;
      
      pRpcResultChar->setValue(resultData, dataOffset);
      pRpcResultChar->notify();
    }

    // Command callback handler
    class ImprovCommandCallbacks : public NimBLECharacteristicCallbacks {
      UsermodImprovBLE* usermod;
      
    public:
      ImprovCommandCallbacks(UsermodImprovBLE* u) : usermod(u) {}
      
      void onWrite(NimBLECharacteristic* pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
          uint8_t commandId = rxValue[0];
          
          // 0x01 = Send Wifi Settings
          if (commandId == 0x01 && rxValue.length() >= 3) {
            uint8_t ssidLen = rxValue[2];
            if (rxValue.length() >= 3 + ssidLen + 1) {
              std::string ssid = rxValue.substr(3, ssidLen);
              uint8_t passLen = rxValue[3 + ssidLen];
              std::string pass = "";
              if (rxValue.length() >= 4 + ssidLen + passLen) {
                pass = rxValue.substr(4 + ssidLen, passLen);
              }
              
              // Apply credentials to WLED multiWiFi setup
              memset(multiWiFi[0].clientSSID, 0, 32);
              strlcpy(multiWiFi[0].clientSSID, ssid.c_str(), 32);
              
              memset(multiWiFi[0].clientPass, 0, 64);
              strlcpy(multiWiFi[0].clientPass, pass.c_str(), 64);

              // Notify provisioning state
              usermod->updateStatus(0x03);

              // Tell WLED to connect
              usermod->processingCommand = true;
              forceReconnect = true;
              serializeConfigToFS();
            }
          }
          // 0x02 = Request Current State
          else if (commandId == 0x02) {
            uint8_t state = 0x02; // authorized
            if (Network.isConnected()) state = 0x04; // provisioned
            usermod->updateStatus(state);
            
            if (state == 0x04) {
              IPAddress localIP = Network.localIP();
              char urlStr[64];
              sprintf(urlStr, "http://%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
              std::vector<std::string> resp = {std::string(urlStr)};
              usermod->sendRPCResult(0x02, resp);
            }
          }
          // 0x03 = Request Device Info
          else if (commandId == 0x03) {
            char vString[32];
            sprintf_P(vString, PSTR("%s/%i"), versionString, VERSION);
            bool useMdnsName = (strcmp(serverDescription, "WLED") == 0 && strlen(cmDNS) > 0);
            
            std::vector<std::string> info = {
              "WLED", 
              std::string(vString), 
              "esp32", 
              std::string(useMdnsName ? cmDNS : serverDescription)
            };
            usermod->sendRPCResult(0x03, info);
          }
        }
      }
    };

    void startBLE() {
      if (bleRunning) return;
      
      char bleName[64];
      sprintf(bleName, "Improv_%s", serverDescription); // Using WLED configured name as suffix
      
      NimBLEDevice::init(bleName);
      
      pServer = NimBLEDevice::createServer();
      pImprovService = pServer->createService(IMPROV_SERVICE_UUID);
      
      // Status characteristic
      pStatusChar = pImprovService->createCharacteristic(
        IMPROV_STATUS_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
      );
      
      // Error characteristic
      pErrorChar = pImprovService->createCharacteristic(
        IMPROV_ERROR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
      );
      
      // RPC Command characteristic (Write)
      pRpcCommandChar = pImprovService->createCharacteristic(
        IMPROV_RPC_COMMAND_UUID,
        NIMBLE_PROPERTY::WRITE
      );
      pRpcCommandChar->setCallbacks(new ImprovCommandCallbacks(this));
      
      // RPC Result characteristic
      pRpcResultChar = pImprovService->createCharacteristic(
        IMPROV_RPC_RESULT_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
      );
      
      pImprovService->start();
      
      NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
      pAdvertising->addServiceUUID(IMPROV_SERVICE_UUID);
      pAdvertising->start();
      
      bleRunning = true;
      updateStatus(0x02); // Set state to "Authorized" indicating readiness to accept config
    }

    void stopBLE() {
      if (!bleRunning) return;
      
      NimBLEDevice::deinit(true);
      pServer = nullptr;
      pImprovService = nullptr;
      pStatusChar = nullptr;
      pErrorChar = nullptr;
      pRpcCommandChar = nullptr;
      pRpcResultChar = nullptr;
      
      bleRunning = false;
    }

  public:
    void setup() {
      // Nothing needed on strict boot, handled in loop
    }

    void loop() {
      if (millis() - lastCheck > 1000) {
        lastCheck = millis();
        
        bool isConnected = Network.isConnected();
        
        if (isConnected && bleRunning) {
          // Verify we aren't literally in the middle of replying to an Improv command
          if (processingCommand && pRpcResultChar) {
             IPAddress localIP = Network.localIP();
             char urlStr[64];
             sprintf(urlStr, "http://%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
             std::vector<std::string> resp = {std::string(urlStr)};
             
             updateStatus(0x04); // provisioned
             sendRPCResult(0x01, resp); // RPC response for Command 0x01 Send Wifi Settings
             
             processingCommand = false;
             
             // Wait briefly for characteristic notification to send before shutdown
             lastCheck = millis() + 3000;
             return;
          }
          
          stopBLE();
        } else if (!isConnected && !WLED_WIFI_CONFIGURED && !bleRunning && !apActive) {
          // If we fallback to AP mode, we probably also want BLE active, so we can ignore !apActive 
          // Just start BLE if not connected (meaning AP might be active, but we are looking for client connection via Improv)
          startBLE();
        } else if (!isConnected && !bleRunning) {
           startBLE();
        }
      }
    }

    uint16_t getId() {
      return USERMOD_ID_IMPROV_BLE;
    }
};

#endif
