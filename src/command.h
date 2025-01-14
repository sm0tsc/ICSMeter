// Copyright (c) F4HWN Armel. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Send CI-V Command by Bluetooth
void sendCommandBt(char *request, size_t n, char *buffer, uint8_t limit)
{
  uint8_t byte1, byte2, byte3;
  uint8_t counter = 0;

  while (counter != limit)
  {
    for (uint8_t i = 0; i < n; i++)
    {
      CAT.write(request[i]);
    }

    vTaskDelay(100);

    while (CAT.available())
    {
      byte1 = CAT.read();
      byte2 = CAT.read();

      if (byte1 == 0xFE && byte2 == 0xFE)
      {
        counter = 0;
        byte3 = CAT.read();
        while (byte3 != 0xFD)
        {
          buffer[counter] = byte3;
          byte3 = CAT.read();
          counter++;
          if (counter > limit)
          {
            if (DEBUG)
            {
              Serial.print(" Overflow");
            }
            break;
          }
        }
      }
    }
    startup = false;
  }
  // Serial.println(" Ok");
}

// Send CI-V Command by Wifi
void sendCommandWifi(char *request, size_t n, char *buffer, uint8_t limit)
{
  static uint8_t proxyError = 0;

  HTTPClient http;
  uint16_t httpCode;

  String command = "";
  String response = "";

  char s[4];

  for (uint8_t i = 0; i < n; i++)
  {
    sprintf(s, "%02x,", request[i]);
    command += String(s);
  }

  command += BAUDE_RATE + String(",") + SERIAL_DEVICE;

  http.begin(civClient, PROXY_URL + String(":") + PROXY_PORT + String("/") + String("?civ=") + command); // Specify the URL
  http.addHeader("User-Agent", "M5Stack");                                                               // Specify header
  http.addHeader("Connection", "keep-alive");                                                            // Specify header
  http.setTimeout(100);                                                                                  // Set Time Out
  httpCode = http.GET();                                                                                 // Make the request

  if (httpCode == 200)
  {
    proxyConnected = true;
    proxyError = 0;

    response = http.getString(); // Get data
    response.trim();
    response = response.substring(4);

    if (response == "")
    {
      txConnected = false;
    }
    else
    {
      txConnected = true;
      startup = false;

      for (uint8_t i = 0; i < limit; i++)
      {
        buffer[i] = strtol(response.substring(i * 2, (i * 2) + 2).c_str(), NULL, 16);
      }

      if (DEBUG)
      {
        Serial.println("-----");
        Serial.print(response);
        Serial.print(" ");
        Serial.println(response.length());

        for (uint8_t i = 0; i < limit; i++)
        {
          Serial.print(int(buffer[i]));
          Serial.print(" ");
        }
        Serial.println(" ");
        Serial.println("-----");
      }
    }
  }
  else
  {
    proxyError++;
    if (proxyError > 10)
    {
      proxyError = 10;
      proxyConnected = false;
    }
  }
  http.end(); // Free the resources
}

// Send CI-V Command dispatcher
void sendCommand(char *request, size_t n, char *buffer, uint8_t limit)
{
  if (IC_MODEL == 705 && IC_CONNECT == BT)
    sendCommandBt(request, n, buffer, limit);
  else
    sendCommandWifi(request, n, buffer, limit);
}

// Get Smeter
void getSmeter()
{
  String valString;

  static char buffer[6];
  char request[] = {0xFE, 0xFE, CI_V_ADDRESS, 0xE0, 0x15, 0x02, 0xFD};
  char str[2];

  uint8_t val0 = 0;
  float_t val1 = 0;
  static uint8_t val2 = 0;

  float_t angle = 0;

  size_t n = sizeof(request) / sizeof(request[0]);

  sendCommand(request, n, buffer, 6);

  sprintf(str, "%02x%02x", buffer[4], buffer[5]);
  val0 = atoi(str);

  if (val0 <= 120)
  { // 120 = S9 = 9 * (40/3)
    val1 = val0 / (40 / 3.0f);
  }
  else
  { // 240 = S9 + 60
    val1 = (val0 - 120) / 2.0f;
  }

  if (abs(val0 - val2) > 1 || reset == true)
  {
    val2 = val0;
    reset = false;

    if (val0 <= 13)
    {
      angle = 42.50f;
      valString = "S " + String(int(round(val1)));
    }
    else if (val0 <= 120)
    {
      angle = mapFloat(val0, 14, 120, 42.50f, -6.50f); // SMeter image start at S1 so S0 is out of image on the left...
      valString = "S " + String(int(round(val1)));
    }
    else
    {
      angle = mapFloat(val0, 121, 241, -6.50f, -43.0f);
      if (int(round(val1) < 10))
        valString = "S 9 + 0" + String(int(round(val1))) + " dB";
      else
        valString = "S 9 + " + String(int(round(val1))) + " dB";
    }

    // Debug trace
    if (DEBUG)
    {
      Serial.print("Get S");
      Serial.print(val0);
      Serial.print(" ");
      Serial.print(val1);
      Serial.print(" ");
      Serial.println(angle);
    }

    // Draw line
    needle(angle);

    // Write Value
    value(valString);

    // If led strip...
    /*
    uint8_t limit = map(val0, 0, 241, 0, NUM_LEDS_STRIP);

    for (uint8_t i = 0; i < limit; i++)
    {
      if (i < NUM_LEDS_STRIP / 2)
      {
        strip[i] = CRGB::Blue;
      }
      else
      {
        strip[i] = CRGB::Red;
      }
    }

    for (uint8_t i = limit; i < NUM_LEDS_STRIP; i++)
    {
      strip[i] = CRGB::White;
    }
    FastLED.show();
    */
  }
}

// Get SWR
void getSWR()
{
  String valString;

  static char buffer[6];
  char request[] = {0xFE, 0xFE, CI_V_ADDRESS, 0xE0, 0x15, 0x12, 0xFD};
  char str[2];

  uint8_t val0 = 0;
  float_t val1 = 0;
  static uint8_t val3 = 0;

  float_t angle = 0;

  size_t n = sizeof(request) / sizeof(request[0]);

  sendCommand(request, n, buffer, 6);

  sprintf(str, "%02x%02x", buffer[4], buffer[5]);
  val0 = atoi(str);

  if (val0 != val3 || reset == true)
  {
    val3 = val0;
    reset = false;

    if (val0 <= 48)
    {
      angle = mapFloat(val0, 0, 48, 42.50f, 32.50f);
      val1 = mapFloat(val0, 0, 48, 1.0, 1.5);
    }
    else if (val0 <= 80)
    {
      angle = mapFloat(val0, 49, 80, 32.50f, 24.0f);
      val1 = mapFloat(val0, 49, 80, 1.5, 2.0);
    }
    else if (val0 <= 120)
    {
      angle = mapFloat(val0, 81, 120, 24.0f, 10.0f);
      val1 = mapFloat(val0, 81, 120, 2.0, 3.0);
    }
    else if (val0 <= 155)
    {
      angle = mapFloat(val0, 121, 155, 10.0f, 0.0f);
      val1 = mapFloat(val0, 121, 155, 3.0, 4.0);
    }
    else if (val0 <= 175)
    {
      angle = mapFloat(val0, 156, 175, 0.0f, -7.0f);
      val1 = mapFloat(val0, 156, 175, 4.0, 5.0);
    }
    else if (val0 <= 225)
    {
      angle = mapFloat(val0, 176, 225, -7.0f, -19.0f);
      val1 = mapFloat(val0, 176, 225, 5.0, 10.0);
    }
    else
    {
      angle = mapFloat(val0, 226, 255, -19.0f, -30.50f);
      val1 = mapFloat(val0, 226, 255, 10.0, 50.0);
    }

    valString = "SWR " + String(val1);

    // Debug trace
    if (DEBUG)
    {
      Serial.print("Get SWR");
      Serial.print(val0);
      Serial.print(" ");
      Serial.print(val1);
      Serial.print(" ");
      Serial.println(angle);
    }

    // Draw line
    needle(angle);

    // Write Value
    value(valString);
  }
}

// Get Power
void getPower()
{
  String valString;

  static char buffer[6];
  char request[] = {0xFE, 0xFE, CI_V_ADDRESS, 0xE0, 0x15, 0x11, 0xFD};
  char str[2];

  uint8_t val0 = 0;
  float_t val1 = 0;
  float_t val2 = 0;
  static uint8_t val3 = 0;

  float_t angle = 0;

  size_t n = sizeof(request) / sizeof(request[0]);

  sendCommand(request, n, buffer, 6);

  sprintf(str, "%02x%02x", buffer[4], buffer[5]);
  val0 = atoi(str);

  if (val0 != val3 || reset == true)
  {
    val3 = val0;
    reset = false;

    if (val0 <= 27)
    {
      angle = mapFloat(val0, 0, 27, 42.50f, 30.50f);
      val1 = mapFloat(val0, 0, 27, 0, 0.5);
    }
    else if (val0 <= 49)
    {
      angle = mapFloat(val0, 28, 49, 30.50f, 23.50f);
      val1 = mapFloat(val0, 28, 49, 0.5, 1.0);
    }
    else if (val0 <= 78)
    {
      angle = mapFloat(val0, 50, 78, 23.50f, 14.50f);
      val1 = mapFloat(val0, 50, 78, 1.0, 2.0);
    }
    else if (val0 <= 104)
    {
      angle = mapFloat(val0, 79, 104, 14.50f, 6.30f);
      val1 = mapFloat(val0, 79, 104, 2.0, 3.0);
    }
    else if (val0 <= 143)
    {
      angle = mapFloat(val0, 105, 143, 6.30f, -6.50f);
      val1 = mapFloat(val0, 105, 143, 3.0, 5.0);
    }
    else if (val0 <= 175)
    {
      angle = mapFloat(val0, 144, 175, -6.50f, -17.50f);
      val1 = mapFloat(val0, 144, 175, 5.0, 7.0);
    }
    else
    {
      angle = mapFloat(val0, 176, 226, -17.50f, -30.50f);
      val1 = mapFloat(val0, 176, 226, 7.0, 10.0);
    }

    val2 = round(val1 * 10);
    if (IC_MODEL == 705)
      valString = "PWR " + String((val2 / 10)) + " W";
    else
      valString = "PWR " + String(val2) + " W";

    // Debug trace
    if (DEBUG)
    {
      Serial.print("Get PWR");
      Serial.print(val0);
      Serial.print(" ");
      Serial.print(val1);
      Serial.print(" ");
      Serial.println(angle);
    }

    // Draw line
    needle(angle);

    // Write Value
    value(valString);
  }
}

// Get Data Mode
void getDataMode()
{
  static char buffer[6];
  char request[] = {0xFE, 0xFE, CI_V_ADDRESS, 0xE0, 0x1A, 0x06, 0xFD};

  size_t n = sizeof(request) / sizeof(request[0]);

  sendCommand(request, n, buffer, 6);

  dataMode = buffer[4];
}

// Get Frequency
void getFrequency()
{
  String frequency;
  String frequencyNew;

  static char buffer[8];
  char request[] = {0xFE, 0xFE, CI_V_ADDRESS, 0xE0, 0x03, 0xFD};

  double freq; // Current frequency in Hz
  const uint32_t decMulti[] = {1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};

  uint8_t lenght = 0;

  size_t n = sizeof(request) / sizeof(request[0]);

  sendCommand(request, n, buffer, 8);

  freq = 0;
  for (uint8_t i = 2; i < 7; i++)
  {
    freq += (buffer[9 - i] >> 4) * decMulti[(i - 2) * 2];
    freq += (buffer[9 - i] & 0x0F) * decMulti[(i - 2) * 2 + 1];
  }

  if (transverter > 0)
    freq += double(choiceTransverter[transverter]);

  frequency = String(freq);
  lenght = frequency.length();

  if (frequency != "0")
  {
    int8_t i;

    for (i = lenght - 6; i >= 0; i -= 3)
    {
      frequencyNew = "." + frequency.substring(i, i + 3) + frequencyNew;
    }

    if (i == -3)
    {
      frequencyNew = frequencyNew.substring(1, frequencyNew.length());
    }
    else
    {
      frequencyNew = frequency.substring(0, i + 3) + frequencyNew;
    }
    subValue(frequencyNew);
  }
  else
  {
    subValue("-");
  }
}

// Get Mode
void getMode()
{
  String valString;

  static char buffer[5];
  char request[] = {0xFE, 0xFE, CI_V_ADDRESS, 0xE0, 0x04, 0xFD};

  const char *mode[] = {"LSB", "USB", "AM", "CW", "RTTY", "FM", "WFM", "CW-R", "RTTY-R", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "DV"};

  size_t n = sizeof(request) / sizeof(request[0]);

  sendCommand(request, n, buffer, 5);

  display.setFont(0);
  display.setTextPadding(24);
  display.setTextColor(TFT_WHITE);
  display.setTextDatum(CC_DATUM);

  valString = "FIL" + String(uint8_t(buffer[4]));
  if (valString != filterOld)
  {
    filterOld = valString;
    display.fillRoundRect(40, 198, 40, 15, 2, TFT_MODE_BACK);
    display.drawRoundRect(40, 198, 40, 15, 2, TFT_MODE_BORDER);
    display.drawString(valString, 60, 206);
  }

  valString = String(mode[(uint8_t)buffer[3]]);

  getDataMode(); // Data ON or OFF ?

  if (dataMode == 1)
  {
    valString += "-D";
  }
  if (valString != modeOld)
  {
    modeOld = valString;
    display.fillRoundRect(240, 198, 40, 15, 2, TFT_MODE_BACK);
    display.drawRoundRect(240, 198, 40, 15, 2, TFT_MODE_BORDER);
    display.drawString(valString, 260, 206);
  }
}

// Get TX
uint8_t getTX()
{
  uint8_t value;

  static char buffer[5];
  char request[] = {0xFE, 0xFE, CI_V_ADDRESS, 0xE0, 0x1C, 0x00, 0xFD};

  size_t n = sizeof(request) / sizeof(request[0]);

  sendCommand(request, n, buffer, 5);

  if (buffer[4] <= 1)
  {
    value = buffer[4];
  }
  else
  {
    value = 0;
  }

  return value;
}