/*
   Simple ESP32/Arduino sketch to switch on/off channels of 4CH Sonoff switches
   - looks up addresses of specific devices with mDNS @start
   - WIFI credentials and device IDs and keys have to be configured in devices.h
   
   Essential function:
   int doswitch (int deviceNo, int channel, int function)
      deviceNo: 0...nummber of devices in devices.h
      channel: 1...4
      function: 0 for off, 1 for on
      returns http response code

   For testing simple two letter input commands can be entered.
*/

#include <string.h>
#include <stdio.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <hwcrypto/aes.h>
#include "MD5Builder.h"
#include "mbedtls/base64.h"

#include "devices.h" // device IDs and keys

#define AES_blkSize 16
#define TEST false // set to true for test data and more debug output

IPAddress *ipaddresses; // for collection of actual IP addresses on LAN

#include <HTTPClient.h>
HTTPClient http; // for sending commands as POST request

/*
   input for testing
*/
int charCount; // status of input reading
int incomingByte; // for incoming serial data
char command[3]; // the command buffer

unsigned long lastTime = 0;

#if TEST
static inline int32_t _getCycleCount(void) {
  int32_t ccount;
  asm volatile("rsr %0,ccount":"=a" (ccount));
  return ccount;
}
#endif

char plaintext[256];
char encrypted[256];
unsigned char output[256];

/*char * toDecode = "zJBk79hXPpG4LB7eoQbAAw==";
  size_t outputLength;
  unsigned char * decoded = base64_decode((const unsigned char *)toDecode, strlen(toDecode), &outputLength);

  Serial.print("Length of decoded message: ");
  Serial.println(outputLength);

  //Serial.printf("%.*s\n", outputLength, decoded);
  for (int i = 0; i < 16; i++) Serial.printf("%02x", decoded[i]);
  Serial.println();
  free(decoded);*/

/*
   read Serial input until ENTER and compose a command from [channel 1-4][0=off or 1=on]
   return true if valid command in command array
   to be called in loop()
*/
bool checkCommand(void)
{
  if (charCount < 0)
  {
    Serial.printf("Enter a command [1-4][0-1]:");
    Serial.readStringUntil('\n'); // eliminate rest in input buffer
    charCount = 0;
  }
  if (Serial.available() > 0) {
    incomingByte = Serial.read();
    if ((charCount == 2) && (incomingByte == '\n')) // valid command
    {
      charCount = -1;
      return true;
    }
    else if ((charCount == 0) && ((incomingByte >= '1') && (incomingByte <= '4'))) // valid 1st letter
    {
      command[0] = incomingByte;
      charCount++;
    }
    else if ((charCount == 1) && ((incomingByte >= '0') && (incomingByte <= '1'))) // valid 2nd letter
    {
      command[1] = incomingByte;
      charCount++;
    }
    else  // invalid condition
    {
      charCount = -1;
      Serial.printf("Invalid input!\n\n");
    }
  }
  return false; // in any other case than final valid command
}

String encrypt(char* strDeviceId, char* strDeviceKey, char* strPlainText)
{
  uint8_t iv[16]; //initialisation vector
  uint8_t ivrandom[16]; //random source
  char *devicekey = strDeviceKey;
  uint8_t keyhash[16];
  size_t outputLength;
  String message;
#if TEST
  Serial.print("* devicekey "); Serial.println(devicekey);
  devicekey = "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP";
#endif

  for (int i = 0; i < 16; i++) ivrandom[i] = random(256) & 0xff; //fill with 16 random bytes
  /*
     build the encryption key from the device key
  */
  MD5Builder nonce_md5;
  nonce_md5.begin();
  nonce_md5.add(devicekey);
  nonce_md5.calculate();
  nonce_md5.getBytes(keyhash);
  MD5Builder _md5;

  //keyhash[16] = 0;

#if TEST
  memcpy(keyhash, "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP", 16); // to look for defined output
  memcpy(iv, "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP", 16);
#else
  for (int i = 0; i < 16; i++) iv[i] = ivrandom[i]; //get the random bytes
#endif

#if TEST
  Serial.print("* keyhash   "); for (int i = 0; i < 16; i++) Serial.printf("%02x", keyhash[i]); Serial.println();
  Serial.print("* ivrandom  "); for (int i = 0; i < 16; i++) Serial.printf("%02x", iv[i]); Serial.println();
  strcpy(plaintext, R"({"switches": [{"switch": "off", "outlet": 0}, {"switch": "off", "outlet": 3}]})");
#else
  strcpy(plaintext, strPlainText);
#endif
#if TEST
  Serial.print("* plaintext "); Serial.println(plaintext);
#endif
  /*
     encrypt the command = data
  */
  esp_aes_context ctx;
  esp_aes_init( &ctx );
  esp_aes_setkey( &ctx, (const unsigned char *)keyhash, 128 );

  /* use CMS padding to AES_blkSize */
  byte padlen = AES_blkSize - (strlen(plaintext) % AES_blkSize);
  int padded_size = strlen(plaintext) + padlen;
  for (int i = strlen(plaintext); i % AES_blkSize; plaintext[i++] = padlen);

#if TEST
  Serial.printf("padded_size = %d\n", padded_size);
  Serial.print("* padded    "); for (int i = 0; i < padded_size; i++) Serial.printf("%02x", plaintext[i]); Serial.println();
  int32_t start = _getCycleCount();
#endif
  /*
     perform the encryption
  */
  esp_aes_crypt_cbc( &ctx, ESP_AES_ENCRYPT, padded_size, iv, (uint8_t*)plaintext, (uint8_t*)encrypted );
#if TEST
  int32_t end = _getCycleCount();
  float enctime = (end - start) / 240.0;
  Serial.printf( "Encryption time: %.2fus (%f MB/s)\n", enctime, (sizeof(plaintext) * 1.0) / enctime );
  Serial.print("* data    "); for (int i = 0; i < 80; i++) Serial.printf("%02x", encrypted[i]); Serial.println();
#endif

  /*
     restore iv (modified by encryption)
  */
#if TEST
  strncpy((char*)iv, "ABCDEFGHIJKLMNOPABCDEFGHIJKLMNOP", 16);
#else
  for (int i = 0; i < 16; i++) iv[i] = ivrandom[i]; //get the random bytes
#endif
  esp_aes_free( &ctx );
  /*
     create the final command message
  */
  message += "{\n\"sequence\": \"";
  message += "1639815547714"; //String(millis());
  message += "\", \n\"deviceid\": \"";
  message += String(strDeviceId);
  message += "\", \n\"selfApikey\": \"123\", \n\"data\": \"";
  mbedtls_base64_encode(output, 256, &outputLength, (unsigned char*)encrypted, padded_size);
  if (TEST) Serial.printf("b64data %d %.*s\n", outputLength, outputLength, output);
  message += String((char*)output);
  message += "\", \n\"encrypt\": true, \n\"iv\": \"";
  mbedtls_base64_encode(output, 64, &outputLength, (unsigned char*)iv, 16);
  if (TEST) Serial.printf("b64iv %d %.*s\n", outputLength, outputLength, output);
  message += String((char*)output);
  message += "\"\n}";
  if (TEST) Serial.printf("%s\n", message.c_str());
  return message;
}

int doswitch (int deviceNo, int channel, int function)
{
  String m;
  char command[50];
  String url;
  if ((deviceNo >= sizeof(devices) / sizeof(devices[0])) || (channel < 1) || (channel > 4) || (function < 0) || (function > 1)) return -1;

  // Specify content-type header
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "close");
  http.addHeader("Accept", "*/*");

  // Your Domain name with URL path or IP address with path
  url = "http://" + ipaddresses[deviceNo].toString() + ":8081/zeroconf/switches";
  http.begin(url);
#if TEST
  int httpResponseCode = http.POST(R"({'sequence': '1639593148516', 'deviceid': '1000f4d171', 'selfApikey': '123', 'data': {'switches': [{'switch': 'off', 'outlet': 0}, {'switch': 'off', 'outlet': 3}]}})");
#else
  snprintf(command, 50, "{\"switches\": [{\"switch\": \"%s\", \"outlet\": %d}]}", function ? "on" : "off", channel - 1);
  if (TEST) Serial.println(command);
  //  m = encrypt("1000f4d171", "88d861d6-f5f4-4c4d-84ff-4a173db5901b", command);
  m = encrypt(devices[deviceNo].id, devices[deviceNo].key, command);

  int httpResponseCode = http.POST(m.c_str());
#endif
  if (httpResponseCode == 200)
    Serial.printf("OK\n");
  else
    Serial.printf("HTTP Response code: %d\n");

  // Free resources
  http.end();
  return httpResponseCode;
}

void loop() {

  //Check WiFi connection status
  if (WiFi.status() == WL_CONNECTED) {
    if (checkCommand()) {
      Serial.printf("*%s*\n", command);
      doswitch (1, command[0] - '0', command[1] - '0');
    }
  }
}

void setup() {
  char device[10 + 1];
  Serial.begin(115200);
  command[2] = 0;
  charCount = -1;

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting...\n");
    delay(500);
  }
  Serial.print("Connected\n");

  if (mdns_init() != ESP_OK) {
    Serial.println("mDNS failed to start");
    return;
  }

  int nrOfServices = MDNS.queryService("ewelink", "tcp");
  try
  {
    if (nrOfServices == 0) {
      Serial.println("No services were found.");
    }
    else {
      Serial.print("Number of services found: ");
      Serial.println(nrOfServices);
      ipaddresses = new(IPAddress[nrOfServices]);

      for (int i = 0; i < nrOfServices; i = i + 1) {
#if TEST
        Serial.print("Hostname: ");
        Serial.println(MDNS.hostname(i));
        Serial.print("IP address: ");
        Serial.print(MDNS.IP(i));
        Serial.print(" Port: ");
        Serial.println(MDNS.port(i));
        Serial.println("---------------");
#endif
        if (sscanf(MDNS.hostname(i).c_str(), "eWeLink_%10s", &device) == 1)
        {
          Serial.printf("found device %d %s @%s\n", i + 1, device, MDNS.IP(i).toString().c_str());
          //for(int j =0; j < MDNS.numTxt(i); j++)
          //Serial.printf("val=",MDNS.txt(i, j));
          for (int j = 0; j < sizeof(devices) / sizeof(devices[0]); j++)
          {
            if (!strcmp(device, devices[j].id))
            {
              ipaddresses[j] = MDNS.IP(i);
#if TEST
              Serial.print(MDNS.IP(i)); Serial.print("="); Serial.print(ipaddresses[j]); Serial.println();
#endif
            }
          }
        }
      }
      Serial.println();
    }
  }
  catch (...)
  {
    Serial.println("Error: mDNS address lookup failed");
  }
}
