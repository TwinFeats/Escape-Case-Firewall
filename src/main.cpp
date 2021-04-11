#include <Arduino.h>
#include <ButtonDebounce.h>
#include <NeoPixelBrightnessBus.h>
#include <arduino-timer.h>
#define PJON_MAX_PACKETS 4
#define PJON_PACKET_MAX_LENGTH 52
#include <PJONSoftwareBitBang.h>
#include "../../Escape Room v2 Master/src/tracks.h"

#define PIN_COLOR1        2
#define PIN_COLOR2        3
#define PIN_COLOR3        4
#define PIN_COLOR4        5
#define PIN_COLOR5        6
#define PIN_ENTER         7
#define PIN_LEDS          8
#define PIN_POWER_LIGHT   9
#define PIN_COMM          13

void convertColorsToNames(RgbColor g[5]);

NeoGamma<NeoGammaTableMethod> colorGamma;

RgbColor white(255, 255, 255);
RgbColor red(255, 0, 0);
RgbColor green(0, 255, 0);
RgbColor blue(0, 0, 255);
RgbColor yellow(255, 255, 0);
RgbColor cyan(0, 255, 255);
RgbColor pink(255, 0, 255);
RgbColor black(0, 0, 0);
RgbColor coral = colorGamma.Correct(RgbColor(255, 127, 80));
RgbColor purple = colorGamma.Correct(RgbColor(138, 43, 226));
RgbColor olive = colorGamma.Correct(RgbColor(128, 128, 0));
RgbColor rosyBrown = colorGamma.Correct(RgbColor(188, 143, 143));
RgbColor yellowGreen = colorGamma.Correct(RgbColor(154, 205, 50));
RgbColor beige = colorGamma.Correct(RgbColor(255, 235, 205));
RgbColor darkSeaGreen = colorGamma.Correct(RgbColor(143, 188, 143));
RgbColor orange = colorGamma.Correct(RgbColor(255, 165, 0));
RgbColor turquoise = colorGamma.Correct(RgbColor(175, 238, 238));
RgbColor plum = colorGamma.Correct(RgbColor(221, 160, 221));

struct CLUE {
  uint8_t correct;
  uint8_t incorrect;
};

boolean isMasterMindRunning = true;
NeoPixelBrightnessBus<NeoRgbFeature, Neo400KbpsMethod> mastermindLights(
    5, PIN_LEDS);

RgbColor colors[8] = {white, red, green, blue, yellow, cyan, pink, black};
char colorNames[8] = {'W', 'R', 'G', 'B', 'Y', 'C', 'P', 'E'};

RgbColor code[5], guess[5] = {black, black, black, black, black};
RgbColor guessCopy[5], codeCopy[5];
RgbColor bad(1, 1, 1), bad2(2, 2, 2);

uint8_t mmLights[5] = {7, 7, 7, 7, 7};

CLUE clue;
char guessColorNames[6] = {'E', 'E', 'E', 'E', 'E', '\0'};


boolean activated = false;

PJON<SoftwareBitBang> bus(12);

void sendLcd(const char *line1, const char *line2);

void send(uint8_t *msg, uint8_t len) {
  bus.send(1, msg, len);
  while (bus.update()) {};//wait for send to be completed
}

void send(const char *msg, int len) {
  uint8_t buf[35];
  memcpy(buf, msg, len);
  send(buf, len);
}

void error_handler(uint8_t code, uint16_t data, void *custom_pointer) {
  if(code == PJON_CONNECTION_LOST) {
    Serial.print("Connection lost with device id ");
    Serial.println(bus.packets[data].content[0], DEC);
  }
}

void commReceive(uint8_t *data, uint16_t len, const PJON_Packet_Info &info) {
  if (data[0] == 'A') {
    activated = true;
    digitalWrite(PIN_POWER_LIGHT, HIGH);
    convertColorsToNames(code);
    uint8_t msg[6];
    msg[0] = 'C';
    for (int i=0;i<5;i++) {
      msg[i+1] = guessColorNames[4-i];  //solution is backwards, how original
    }
    bus.send(COMM_ID_CONTROL_ROOM, msg, 6);
    while (bus.update()) {};//wait for send to be completed
  } else if (data[0] == 'W') {  //player has won

  } else if (data[0] == 'L') {  //player has lost

  } else if (data[0] == 'B') {  //brightness
    mastermindLights.SetBrightness(data[1]);
    mastermindLights.Show();
  }
}

void sendLcd(const char *line1, const char *line2) {
  uint8_t msg[35];
  msg[0] = 'L';
  strncpy((char *)&msg[1], line1, 17);
  strncpy((char *)&msg[18], line2, 17);
  Serial.print("Sending ");
  Serial.println((char *)msg);
  send(msg, 35);
}

void sendMp3(int track) {
  uint8_t msg[2];
  msg[0] = 'M';
  msg[1] = track;
  send(msg, 2);
}

void sendTone(int tone) {
  uint8_t msg[2];
  msg[0] = 'T';
  msg[1] = tone;
  send(msg, 2);
}

void initComm() {
  bus.strategy.set_pin(PIN_COMM);
  bus.include_sender_info(false);
  bus.set_error(error_handler);
  bus.set_receiver(commReceive);
  bus.begin();
}


// Lock combo is 4219


/* --------------------- MASTERMIND ---------------------------*/

ButtonDebounce mm1(PIN_COLOR1, 100);
ButtonDebounce mm2(PIN_COLOR2, 100);
ButtonDebounce mm3(PIN_COLOR3, 100);
ButtonDebounce mm4(PIN_COLOR4, 100);
ButtonDebounce mm5(PIN_COLOR5, 100);
ButtonDebounce mmEnter(PIN_ENTER, 100);

void initCode() {
  uint8_t msg[9];
  msg[0] = 'C';
  uint8_t r;
  for (uint8_t i = 0; i < 5; i++) {
    r = random(8);
    code[i] = colors[r];
    msg[i+1] = r;
  }
  bus.send(12, msg, 6);
}

boolean compareRGB(RgbColor color1, RgbColor color2) {
  return color1.R == color2.R && color1.G == color2.G && color1.B == color2.B;
}

void mastermindComplete() {
  sendMp3(TRACK_FIREWALL_BREECHED);
  send((uint8_t *)"D", 1);
  activated = false;
  digitalWrite(PIN_POWER_LIGHT, LOW);
}

/* updates clue */
void evaluateGuess() {
  for (int i = 0; i < 5; i++) {
    guessCopy[i] = guess[i];
    codeCopy[i] = code[i];
  }
  clue.correct = 0;
  clue.incorrect = 0;
  for (int i = 0; i < 5; i++) {
    if (compareRGB(codeCopy[i], guessCopy[i])) {
      clue.correct++;
      guessCopy[i] = bad;
      codeCopy[i] = bad2;
    }
  }
  int i = 0;
next:
  while (i < 5) {
    for (int j = 0; j < 5; j++) {
      if (compareRGB(codeCopy[i], guessCopy[j])) {
        clue.incorrect++;
        guessCopy[j] = bad;
        codeCopy[i] = bad2;
        i++;
        goto next;
      }
    }
    i++;
  }
}

/* Updated guessColorNames */
void convertColorsToNames(RgbColor g[5]) {
  for (int i = 0; i < 5; i++) {
    for (int c = 0; c < 8; c++) {
      if (compareRGB(g[i], colors[c])) {
        guessColorNames[i] = colorNames[c];
        break;
      }
    }
  }
}

void showClue() {
  char msg1[17];
  sprintf(msg1, "%i %s", clue.correct, "exactly right");
  char msg2[17];
  sprintf(msg2, "%i %s", clue.incorrect, "need reorder");
  sendLcd(msg1, msg2);
}

void mmNextLight(int index) {
  mmLights[index] = (mmLights[index] + 1) % 8;
  guess[index] = colors[mmLights[index]];
  mastermindLights.SetPixelColor(index, guess[index]);
  mastermindLights.Show();
}

// state is HIGH/LOW
void mm1Pressed(const int state) {
  if (activated &&
      state == LOW) {
    mmNextLight(0);
  }
}

void mm2Pressed(const int state) {
  if (activated &&
      state == LOW) {
    mmNextLight(1);
  }
}

void mm3Pressed(const int state) {
  if (activated &&
      state == LOW) {
    mmNextLight(2);
  }
}

void mm4Pressed(const int state) {
  if (activated &&
      state == LOW) {
    mmNextLight(3);
  }
}

void mm5Pressed(const int state) {
  if (activated &&
      state == LOW) {
    mmNextLight(4);
  }
}

void mmEnterPressed(const int state) {
  if (activated &&
      state == LOW) {
    evaluateGuess();
    showClue();
    if (clue.correct == 5) {
      mastermindComplete();
    }
  }
}

void initMasterMind() {
  pinMode(PIN_COLOR1, INPUT_PULLUP);
  pinMode(PIN_COLOR2, INPUT_PULLUP);
  pinMode(PIN_COLOR3, INPUT_PULLUP);
  pinMode(PIN_COLOR4, INPUT_PULLUP);
  pinMode(PIN_COLOR5, INPUT_PULLUP);
  pinMode(PIN_ENTER, INPUT_PULLUP);

  mastermindLights.Begin();
  mastermindLights.Show();  // init lights off
  mastermindLights.SetBrightness(128);
  initCode();
  convertColorsToNames(code);
  mm1.setCallback(mm1Pressed);
  mm2.setCallback(mm2Pressed);
  mm3.setCallback(mm3Pressed);
  mm4.setCallback(mm4Pressed);
  mm5.setCallback(mm5Pressed);
  mmEnter.setCallback(mmEnterPressed);
}

/* ------------------- END MASTERMIND------------------- */

void startup() {
  delay(8000*1 + 1000);  //wait for modem panel
  digitalWrite(PIN_POWER_LIGHT, HIGH);

  for (int i=0;i<5;i++) {
    mastermindLights.SetPixelColor(i, red);
    mastermindLights.Show();
    delay(500);
    mastermindLights.SetPixelColor(i, black);
    mastermindLights.Show();
  }
  digitalWrite(PIN_POWER_LIGHT, LOW);
  convertColorsToNames(code);
  sendLcd("Firewall",guessColorNames);
}

void setup() {
  Serial.begin(9600);
  Serial.println("Starting");
  randomSeed(analogRead(0));
  pinMode(PIN_POWER_LIGHT, OUTPUT);
  digitalWrite(PIN_POWER_LIGHT, LOW);
  delay(2000);
  initComm();
  initMasterMind();
  startup();
}

void loop() {
  mm1.update();
  mm2.update();
  mm3.update();
  mm4.update();
  mm5.update();
  mmEnter.update();

  bus.update();
  bus.receive(750);
}