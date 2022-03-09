#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <SPI.h>
#include <HX711.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

ESP8266WebServer    server(80);

HX711 scale;  // Initializes library functions

#define TFT_CS         4 // D2
#define TFT_RST        16 // D0
#define TFT_DC         2 // D4
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// define the loadcell stuff
const int LOADCELL_DOUT_PIN = 0; // D3
const int LOADCELL_SCK_PIN = 3; // Rx
int calibration_factor = 0; // Defines calibration factor we'll use for calibrating.
//21145 looks like a good starting point

// Web page stuff
char descBuf[10 + 1];
char descBuf2[10 + 1];
char descBuf3[10 + 1];
char descBuf4[10 + 1];
// for the web page variables
struct settings {
  char Tare[10];
  char Size[10];
  char Factor[10];
  char Rate[10];
} gas_config = {};

// define the graphics array for Cylinder and fill level - these use the graphics.c tab in this script
extern uint8_t LCyl[];
extern uint8_t fil[];
extern uint8_t Logo[];

// variables for delay
int UpdateRate = 5000;
unsigned long prevTime = 0;

// List of colours Those unused can be removed
#define BLACK       0x0000
#define NAVY        0x000F
#define DARKGREEN   0x03E0
#define DARKCYAN    0x03EF
#define MAROON      0x7800
#define PURPLE      0x780F
#define OLIVE       0x7BE0
#define LIGHTGREY   0xD69A
#define DARKGREY    0x7BEF
#define BLUE        0x001F
#define GREEN       0x07E0
#define CYAN        0x07FF
#define RED         0xF800
#define MAGENTA     0xF81F
#define YELLOW      0xFFE0
#define WHITE       0xFFFF
#define ORANGE      0xFDA0
#define GREENYELLOW 0xB7E0
#define PINK        0xFE19
#define BROWN       0x9A60
#define GOLD        0xFEA0
#define SILVER      0xC618
#define SKYBLUE     0x867D
#define VIOLET      0x915C

// define some General Variables for the calculations
int CylTare = 0; // weight of empty gas bottle (shown on neck of cylinder)
float CylSize = 0.00; // size of cylinder e.g. 47kg
int CylWeight = 0; // used to convert weight to integer
int NewCal = 0;
int NewTare = 0;
int NewSize = 0;
int LastTare = 0;
int LastSize = 0;
int LastCalibration = 0;
int LastRate = 0;
int newlevel = 0;
float level = 0;
float LastLevel = 0;
float Lastpct = 0;

//Define the pixel co-ordinates for the cylinder display - this is just the picture of the cylinder
int BottomOfCyl = 118;
int TopOfCyl = 42;


///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////  SETUP  /////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void setup(void)
{
  delay(1000);
  Serial.begin(9600);
      Serial.println("*** STARTUP ***");
  WiFi.mode(WIFI_AP);
  WiFi.softAP("CylMon " + String(WiFi.softAPmacAddress().substring(12, 18)), ("CylMon" + String(WiFi.softAPmacAddress().substring(15, 18)))); 
  // password will be 'CylMon plus last 2 chrs of Mac address e.g. CylMonA9
  tft.initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab
  tft.setRotation(1); // 90 deg clockwise rotation
  tft.fillScreen(BLACK);
  drawBitmap(1, 1, Logo, 160, 128, YELLOW); // Draw logo screen
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);   // Initializes the scaling process.
  scale.set_scale();
  scale.tare();          // Resets the scale to 0.
  EEPROM.begin(512);  //EEPROM.begin(Size)
  delay(2000);
  int FirstRun = 0;
  EEPROM.get(1, FirstRun);
  if (  FirstRun == 0) // is this first run - if so Write some default values to EEPROM for first run
  {
    EEPROM.put(1, 1);
    Serial.println("*** FIRST RUN ***");
    CylTare = 35;
    CylSize = 47;
    calibration_factor = 21145;
    UpdateRate = 5000;
    WriteEEPROM();
       Serial.println("*** EEPROM Written ***");
    delay(1000);
  }
  ReadEEPROM(); // get the stored values for Tare, Cyl Size and cal factor
  server.on("/",  handlePortal);
  server.begin();
  ShowMainScreen();
}
//////////////////////////////// END of SETUP  ////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////  LOOP  //////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void loop() {
  server.handleClient();
  ShowData();
  yield();
}
//////////////////////////////// END of LOOP  //////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////  TFT Gas Level Display  //////////////////////
////////////////////////////////////////////////////////////////////////////////
void showlevel(int gaslevel) // show gas level on TFT display
{
  for (int i = BottomOfCyl; i >= (TopOfCyl); i = i - (CylSize / 80)) // 80 pixels is the max gas level travel
  {
    if (i >= gaslevel)
    {
      drawBitmap(8, i, fil, 26, 1, GREEN); // Draw level in Green until we hit the level value
    }
    else
    {
      drawBitmap(8, i, fil, 26, 1, BLACK); // then continue in Black to the top of ther cylinder (i.e. erase the green bit as the gas level reduces)
    }
  }
}
//////////////////////////////// END of TFT Gas Level Display  //////////////////


/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////  Draw TFT Graphics for Cylinder  //////////////
/////////////////////////////////////////////////////////////////////////////////
//Draw the Graphics on the TFT display
void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color)
{
  int16_t i, j, byteWidth = (w + 7) / 8;
  uint8_t byte;
  for (j = 0; j < h; j++) {
    for (i = 0; i < w; i++) {
      if (i & 7) byte <<= 1;
      else      byte   = pgm_read_byte(bitmap + j * byteWidth + i / 8);
      if (byte & 0x80) tft.drawPixel(x + i, y + j, color);
    }
  }
}
//////////////////////////////// END of Draw TFT Graphics for Cylinder  //////////


/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////  Show Main Screen info  //////////////////////
///////////////////////////////////////////////////////////////////////////////
void ShowMainScreen()
{
  tft.fillScreen(BLACK);
  tft.setTextSize(0);
  tft.setCursor(25, 0); tft.setTextColor(WHITE); tft.println("GAS LEVEL MONITOR");
  CylWeight = CylSize;
  tft.setTextColor(ORANGE); tft.setCursor(55, 20); tft.println("Cal Fact:" + String(calibration_factor));
  tft.setTextColor(ORANGE); tft.setCursor(55, 30); tft.println("Ref Rate:" + String(UpdateRate / 1000) + " sec");
  tft.setTextColor(ORANGE); tft.setCursor(55, 40); tft.println("Cyl Tare:" + String(CylTare) + " Kg");
  tft.setTextColor(ORANGE); tft.setCursor(55, 50); tft.println("Cyl Size:" + String(CylSize, 0) + " Kg");
  tft.setTextColor(WHITE); tft.setCursor(55, 75); tft.println("Remain: ");
  tft.setCursor(55, 95); tft.println("Gas   : ");
  tft.setTextColor(ORANGE); tft.setCursor(55, 115); tft.println("Gross : " + String(level) + " Kg");
  drawBitmap(5, 17, LCyl, 32, 112, RED);
  drawBitmap(8, 41, fil, 26, 1, RED);
  UpdateValues();
}
//////////////////////////////// END of Show Main Screen info  /////////////////


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////  Show all the data  //////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ShowData()
{
  scale.set_scale(calibration_factor);  // Adjusts the calibration factor.
  scale.wait_ready();
  level = (scale.get_units());
  // level = 58; // use this to test with known value (Tare + Gas weight)
  if (level < 0.1) // as weight may fluctuate slightly - set to zero
  {
    level = 0.00;
  }
  // Only refresh values if update rate has expired to stop screen flicker
  unsigned long currentTime = millis();
  if (currentTime - prevTime >= UpdateRate)
  {
    UpdateValues();
    prevTime = currentTime;
  }
}
//////////////////////////////// END of Show all the data  ///////////////////////


//////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////  Update the display  ///////////////////////////
/////////////////////////////////////////////////////////////////////////////////
void UpdateValues()
{
  if (level != (LastLevel + CylTare)) // Oniy do this if value has changed
  {
    tft.fillRect(100, 65, 95, 65, BLACK);  // draw a filled box in place of the values (Saves clearing whole screen)
  }
  Lastpct = (((level - CylTare) / CylSize) * 100); // Store Last % value
  if (Lastpct > 100)
  {
    Lastpct = 100;
  }
  if (isnan(Lastpct))
  {
    Lastpct = 0;
  }
  int RoundedLevel = (BottomOfCyl - ((80 * Lastpct) / 100));
  showlevel(RoundedLevel); // show gas graphic level on TFT
  LastLevel = (level - CylTare); // Store Last level value
  if (level - CylTare > 0)
  {
    tft.setTextColor(WHITE); tft.setTextSize(0); tft.setCursor(105, 75); tft.println(String(Lastpct) + " %"); // print Gas %
    tft.setTextColor(WHITE); tft.setTextSize(0); tft.setCursor(105, 95); tft.println(String((level - CylTare), 2) + " Kg"); // print Gas level
  }
  else //else just print 0.00 for both
  {
    tft.setTextColor(WHITE); tft.setTextSize(0); tft.setCursor(105, 75); tft.println("0.00 %"); // print Gas %
    tft.setTextColor(WHITE); tft.setTextSize(0); tft.setCursor(105, 95); tft.println("0.00 Kg"); // print Gas level
  }
  tft.fillRect(100, 114, 65, 35, BLACK);  // clear last gross value by drawing a filled box in place of gross value (Saves clearing whole screen)
  tft.setTextColor(ORANGE); tft.setCursor(105, 115); tft.println(String(level) + " Kg");
}
//////////////////////////////// END of Update the display  /////////////////////////


/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////  Read Values from EEPROM for config  //////////////
////////////////////////////////////////////////////////////////////////////////////
void ReadEEPROM()
{
  EEPROM.get(30, CylTare);
  EEPROM.get(60, CylSize);
  EEPROM.get(90, calibration_factor);
  EEPROM.get(120, UpdateRate);
  LastTare = CylTare;
  LastSize = CylSize;
  LastCalibration = calibration_factor;
  LastRate = UpdateRate;
}
//////////////////////////////// END of Read Values from EEPROM for config  ////////


////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////  Write Values to EEPROM for config  //////////////
///////////////////////////////////////////////////////////////////////////////////

void WriteEEPROM()
{
  // Only write values if they have changed else write the old value
  if (CylTare == 0)
  {
    CylTare = LastTare;
  }
  if (CylSize == 0)
  {
    CylSize = LastSize;
  }
  if (calibration_factor == 0)
  {
    calibration_factor = LastCalibration;
  }
  if (UpdateRate == 0)
  {
    UpdateRate = LastRate;
  }
  EEPROM.put(30, CylTare);
  EEPROM.put(60, CylSize);
  EEPROM.put(90, calibration_factor);
  EEPROM.put(120, UpdateRate);
  delay(1000);
  EEPROM.commit();
}
//////////////////////////////// END of Write Values to EEPROM for config  /////////


////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////  All the web page stuff for the config  //////////
///////////////////////////////////////////////////////////////////////////////////

// Web Page stuff
void handlePortal() {
  if (server.method() == HTTP_POST) {
    strncpy(gas_config.Tare,     server.arg("Tare").c_str(),     sizeof(gas_config.Tare) );
    strncpy(gas_config.Size, server.arg("Size").c_str(), sizeof(gas_config.Size) );
    strncpy(gas_config.Factor, server.arg("Factor").c_str(), sizeof(gas_config.Factor) );
    strncpy(gas_config.Rate, server.arg("Rate").c_str(), sizeof(gas_config.Rate) );
    gas_config.Rate[server.arg("Rate").length()] = gas_config.Tare[server.arg("Tare").length()] = gas_config.Size[server.arg("Size").length()] = gas_config.Factor[server.arg("Factor").length()] = '\0';
    strncpy(descBuf, gas_config.Tare, 10);
    strncpy(descBuf2, gas_config.Size, 10);
    strncpy(descBuf3, gas_config.Factor, 10);
    strncpy(descBuf4, gas_config.Rate, 10);
    // convert all the char variables into Strings
    String stringOne = String(descBuf);
    String stringTwo = String(descBuf2);
    String stringThree = String(descBuf3);
    String stringFour = String(descBuf4);
    // convert all the Strings variables into Int
    CylTare = stringOne.toInt();
    CylSize = stringTwo.toInt();
    calibration_factor = stringThree.toInt();
    UpdateRate = (stringFour.toInt() * 1000);
    server.send(200,   "text/html",  "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Gas Level Monitor Configuration</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1,p{text-align: center}</style> </head> <body><main class='form-signin'> <h1>Gas Level Monitor Configuration</h1> <br/> <p>Your settings have been saved successfully!</p></main></body></html>" );
  } else {
    server.send(200,   "text/html", "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width/1.5, initial-scale=.75'><title>Gas Level Monitor Configuration</title> <style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:75%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{cursor: pointer;border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1{text-align: center}</style> </head> <body><main class='form-signin'> <form action='/' method='post'> <h1 class=''>Gas Level Monitor Configuration</h1><br/><div class='form-floating'><label>Sample rate (Seconds - Max 7200)</label><input type='number' min = 1 max=7200 class='form-control' name='Rate'></div><div class='form-floating'><br/><label>Calibration factor (Max 99999)</label><input type='number' min = 1 max=99999 class='form-control' name='Factor'></div><div class='form-floating'><br/><label>Tare Weight (Max = 47 Kg)</label><input type='number' min = 1 max=47 class='form-control' name='Tare'> </div><div class='form-floating'><br/><label>Cylinder Size (Max = 47 Kg)</label><input type='number' min = 1 max=47 class='form-control' name='Size'></div><br/><br/><button type='submit'>Save Configuration</button><p style='text-align: right'></p></form></main> </body></html>" );
  }
  WriteEEPROM();
  ShowMainScreen();
  UpdateValues();
}
/////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// END of Sketch  //////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////
