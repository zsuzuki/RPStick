#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <USBKeyboard.h>
#include <array>
#include <vector>
#include <functional>
#include <string>
#include <hardware/flash.h>

#include "btn.h"
#include "layer.h"
#include "menu.h"

namespace
{
  int Power = 11;
  int PIN = 12;
#define NUMPIXELS 1

  int SDA = 6;
  int SCL = 7;

  Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
  U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R2, /* reset=*/U8X8_PIN_NONE, /* clock=*/SCL, /* data=*/SDA);
  USBKeyboard keyboard(true, 0x16c0, 0x27db, 0x0001);

  Button btnA(A0);
  Button btnB(A1);
  Button btnC(A2);
  Button modKey(A3);

  LayerList macLayers{{
      {{'\t', arduino::KEY_CTRL},
       {'\t', arduino::KEY_CTRL | arduino::KEY_SHIFT},
       {'\n', 0},
       5,
       0,
       0,
       "M:Tab Change",
       "Btn C: Enter"},
      {{arduino::RIGHT_ARROW, arduino::KEY_CTRL},
       {arduino::LEFT_ARROW, arduino::KEY_CTRL},
       {' ', arduino::KEY_LOGO},
       0,
       5,
       0,
       "M:Win Change",
       "Btn C: Spotlight"},
      {{'\t', arduino::KEY_LOGO},
       {'\t', arduino::KEY_LOGO | arduino::KEY_SHIFT},
       {'\t', arduino::KEY_ALT},
       0,
       0,
       5,
       "M:App Change",
       "Btn C: Next Win"},
      {{'5', arduino::KEY_LOGO | arduino::KEY_SHIFT},
       {'\n', 0},
       {0x1b, 0}, // escape
       5,
       5,
       0,
       "M:Screen Shot",
       "A:Shot B:OK C:NG"},
      {{arduino::KEY_PAGE_UP, 0},
       {arduino::KEY_PAGE_DOWN, 0},
       {' ', 0},
       5,
       0,
       5,
       "M:Page Scroll",
       "Btn C: Space"},
  }};
  LayerList winLayers{{
      {{'\t', arduino::KEY_CTRL},
       {'\t', arduino::KEY_CTRL | arduino::KEY_SHIFT},
       {'\n', 0},
       0,
       5,
       5,
       "W:Tab Change",
       "Btn C: Enter"},
      {{'\t', arduino::KEY_ALT},
       {'\t', arduino::KEY_ALT | arduino::KEY_SHIFT},
       {'\n', 0},
       5,
       5,
       5,
       "W:App Change",
       "Btn C: App List"},
  }};

  std::array<LayerList *, 2> layerList{&macLayers, &winLayers};

  bool on_menu = false;
  uint8_t layer_index = 0;
  int os_index = 0;

  MenuList os_sel{"OS Select", {"MacOS", "Windows"}};
  MenuInt rep_st{"Start", 500, 100, 2000, 100, "ms"};
  MenuInt rep_cn{"Continue", 200, 50, 1000, 10, "ms"};
  MenuChild rep_set{"Repeat", {&rep_st, &rep_cn}};
  MenuChild topMenu{"Menu", {&os_sel, &rep_set}};

  Menu *current_menu = &topMenu;
  std::vector<Menu *> menu_stack;
  bool mod_long_press = false;
}

int Button::REPEAT_START = 500;
int Button::REPEAT_CONTINUE = 200;

void setup()
{
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenR10_tr);

  pixels.begin();
  pinMode(Power, OUTPUT);
  digitalWrite(Power, HIGH);

  btnA.init();
  btnB.init();
  btnC.init();
  modKey.init();
}

void decide()
{
  // os
  int os = os_sel.index();
  if (os != os_index)
  {
    os_index = os;
    layer_index = 0;
  }

  // repeat
  int rs = rep_st.value();
  if (rs != Button::REPEAT_START)
    Button::REPEAT_START = rs;
  int rc = rep_cn.value();
  if (rc != Button::REPEAT_CONTINUE)
    Button::REPEAT_CONTINUE = rc;
}

void menu()
{
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, current_menu->caption());
  u8g2.drawStr(0, 28, current_menu->valueString().c_str());
  u8g2.sendBuffer();

  auto cancel = [&]
  {
    current_menu->cancel();
    current_menu = menu_stack.back();
    menu_stack.pop_back();
  };

  if (btnA.repeat())
    current_menu->up();
  else if (btnB.repeat())
    current_menu->down();
  else if (btnC.repeat())
  {
    auto *new_menu = current_menu->decide();
    if (new_menu)
    {
      decide();
      menu_stack.push_back(current_menu);
      current_menu = new_menu;
    }
    else
      cancel();
  }

  if (modKey.pressed() && modKey.long_pressed(1000))
  {
    if (!mod_long_press)
    {
      mod_long_press = true;
      on_menu = false;
      u8g2.clearBuffer();
      u8g2.sendBuffer();
      return;
    }
  }
  else if (modKey.release())
  {
    if (!mod_long_press && !menu_stack.empty())
      cancel();
    mod_long_press = false;
  }

  pixels.clear();
  delay(5);
  pixels.show();
  delay(5);
}

void keymode()
{
  static int capcnt = 0;

  auto &layers = *layerList[os_index];

  if (modKey.pressed() && modKey.long_pressed(1000))
  {
    if (!mod_long_press)
    {
      on_menu = true;
      mod_long_press = true;
      return;
    }
  }
  else if (modKey.release())
  {
    if (!mod_long_press)
    {
      layer_index = (layer_index + 1) % layers.size();
      capcnt = 0;
    }
    mod_long_press = false;
  }

  auto &layer = layers[layer_index];
  if (btnA.repeat())
    keyboard.key_code(layer.A_.code, layer.A_.mod);
  if (btnB.repeat())
    keyboard.key_code(layer.B_.code, layer.B_.mod);
  if (btnC.repeat())
    keyboard.key_code(layer.C_.code, layer.C_.mod);

  u8g2.clearBuffer();
  if (capcnt < 180)
  {
    if (layer.caption0_)
      u8g2.drawStr(0, 12, layer.caption0_);
    if (layer.caption1_)
      u8g2.drawStr(0, 28, layer.caption1_);
    u8g2.drawHLine(0, 31, capcnt / 2);
    capcnt++;
  }
  u8g2.sendBuffer();

  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(layer.r_, layer.g_, layer.b_));
  delay(5);
  pixels.show();
  delay(5);
}

void loop()
{
  btnA.update();
  btnB.update();
  btnC.update();
  modKey.update();

  if (on_menu)
    menu();
  else
    keymode();
}
