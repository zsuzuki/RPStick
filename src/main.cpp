//
// Copyright 2023 Suzuki Yoshinori(wave.suzuki.z@gmail.com)
//
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <USBKeyboard.h>
#include <array>
#include <vector>
#include <functional>
#include <string>
extern "C"
{
#include <hardware/flash.h>
}
#include "btn.h"
#include "layer.h"
#include "menu.h"

int Button::REPEAT_START = 500;
int Button::REPEAT_CONTINUE = 200;

namespace
{
  // ピン配置
  constexpr uint8_t Power = 11;
  constexpr int16_t LED_PIN = 12;
  constexpr uint16_t NUMPIXELS = 1;
  constexpr uint8_t SDA = 6;
  constexpr uint8_t SCL = 7;

  /// カスタマイズ項目
  struct Settings
  {
    static constexpr uint32_t MAGIC = 0x464C5354; //"FLST"
    uint32_t magic;
    uint8_t layer_index;
    uint8_t os_index;
    uint16_t repeat_start;
    uint16_t repeat_continue;
  };
  // FLASH配置
  constexpr size_t FLASH_TARGET_OFFSET = 512 * 1024; // FLASH上の設定保存アドレス
  const Settings *settings = (const Settings *)(XIP_BASE + FLASH_TARGET_OFFSET);

  Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);
  U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE, SCL, SDA);
  // U8G2_SSD1306_128X32_WINSTAR_F_HW_I2C u8g2(U8G2_R2, /* reset=*/U8X8_PIN_NONE, SCL, SDA);
  USBKeyboard keyboard(true, 0x16c0, 0x27db, 0x0001);

  Button btnA(A0);
  Button btnB(A1);
  Button btnC(A2);
  Button modKey(A3);

  //
  // この文字列を上書きするには
  // picotool load str.txt -t bin -o 0x10180000
  //
  std::vector<std::string> messages = {
      "Hello", "World", "./build/"};

  std::string getString(const char *msg)
  {
    size_t len = 0;
    for (auto *ptr = msg; *ptr != '\0' && *ptr != 0xa; ptr++)
    {
      len++;
    }
    std::string ret;
    ret.assign(msg, len);
    return ret;
  }

  void overrideMessages()
  {
    const char *msgAdrs = reinterpret_cast<const char *>(0x10180000);
    if (*msgAdrs == '\0')
    {
      return;
    }

    auto numStr = getString(msgAdrs);

    auto n = std::stoi(numStr);
    if (n == 0)
    {
      return;
    }
    messages.resize(n);
    msgAdrs += numStr.length() + 1;

    for (int i = 0; i < n; i++)
    {
      messages[i] = getString(msgAdrs);
      msgAdrs += messages[i].length() + 1;
    }
  }

  LayerList macLayers{{
      {{arduino::RIGHT_ARROW, arduino::KEY_CTRL, 0},
       {arduino::LEFT_ARROW, arduino::KEY_CTRL, 0},
       {'\t', arduino::KEY_ALT, 0},
       0,
       32,
       0,
       "M:Win Change",
       "Btn C: Win Sw"},
      {{'\t', arduino::KEY_CTRL, 0},
       {'\t', arduino::KEY_CTRL | arduino::KEY_SHIFT, 0},
       {'\n', 0, 0},
       32,
       0,
       0,
       "M:Tab Change",
       "Btn C: Enter"},
      {{arduino::RIGHT_ARROW, arduino::KEY_LOGO, 0},
       {arduino::LEFT_ARROW, arduino::KEY_LOGO, 0},
       {'r', arduino::KEY_LOGO, 0},
       0,
       0,
       32,
       "M:History",
       "Btn C: Reload"},
      {{'5', arduino::KEY_LOGO | arduino::KEY_SHIFT, 0},
       {'\n', 0, 0},
       {0x1b, 0, 0}, // escape
       32,
       32,
       0,
       "M:Screen Shot",
       "A:Shot B:OK C:NG"},
      {{arduino::KEY_PAGE_UP, 0, 0},
       {arduino::KEY_PAGE_DOWN, 0, 0},
       {' ', 0, 0},
       32,
       0,
       32,
       "M:Page Scroll",
       "Btn C: Space"},
      {{'\0', 0, 0},
       {'\0', 0, 1},
       {'\0', 0, 2},
       0,
       32,
       32,
       "M:Page Scroll",
       "Btn C: Space"},
  }};
  LayerList winLayers{{
      {{'\t', arduino::KEY_CTRL, 0},
       {'\t', arduino::KEY_CTRL | arduino::KEY_SHIFT, 0},
       {'\n', 0, 0},
       0,
       32,
       32,
       "W:Tab Change",
       "Btn C: Enter"},
      {{'\t', arduino::KEY_ALT, 0},
       {'\t', arduino::KEY_ALT | arduino::KEY_SHIFT, 0},
       {'\n', 0, 0},
       32,
       32,
       32,
       "W:App Change",
       "Btn C: App List"},
  }};

  std::array<LayerList *, 2> layerList{&macLayers, &winLayers};

  bool on_menu = false;
  uint8_t layer_index = 0;
  int os_index = 0;

  void save();
  void load();

  MenuList os_sel{"OS Select", {"MacOS", "Windows"}};
  MenuInt rep_st{"Start", 500, 100, 2000, 100, "ms"};
  MenuInt rep_cn{"Continue", 200, 50, 1000, 10, "ms"};
  MenuChild rep_set{"Repeat", {&rep_st, &rep_cn}};
  MenuFunc save_menu{"Save", save};
  MenuFunc load_menu{"Load", load};
  MenuChild topMenu{"Menu", {&os_sel, &rep_set, &save_menu, &load_menu}};

  Menu *current_menu = &topMenu;
  std::vector<Menu *> menu_stack;
  bool mod_long_press = false;

  ///
  /// カスタマイズ項目読み込み
  ///
  void load()
  {
    if (settings->magic == Settings::MAGIC)
    {
      os_index = settings->os_index;
      layer_index = settings->layer_index;
      Button::REPEAT_START = settings->repeat_start;
      Button::REPEAT_CONTINUE = settings->repeat_continue;
      u8g2.clearBuffer();
      u8g2.drawStr(0, 12, "Load");
      u8g2.drawStr(20, 28, "OK");
      u8g2.sendBuffer();
    }
    else
    {
      u8g2.clearBuffer();
      u8g2.drawStr(0, 12, "Load");
      u8g2.drawStr(20, 28, "Failed");
      u8g2.sendBuffer();
    }
    delay(1000);
  }

  ///
  /// カスタマイズ項目保存
  ///
  void save()
  {
    u8g2.clearBuffer();
#if 1
    u8g2.drawStr(0, 12, "Save");
    u8g2.drawStr(20, 28, "->Erase");
    u8g2.sendBuffer();
    delay(500);

    uint8_t buffer[FLASH_PAGE_SIZE];
    auto *s = reinterpret_cast<Settings *>(buffer);
    *s = *settings;
    s->magic = Settings::MAGIC;
    s->os_index = os_index;
    s->layer_index = layer_index;
    s->repeat_start = Button::REPEAT_START;
    s->repeat_continue = Button::REPEAT_CONTINUE;

    constexpr size_t m = FLASH_PAGE_SIZE - 1;
    constexpr size_t sz = sizeof(Settings) + m;
    constexpr size_t wsz = sz - (sz % FLASH_PAGE_SIZE);
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "Save");
    u8g2.drawStr(20, 28, "->Program");
    u8g2.sendBuffer();
    delay(500);

    flash_range_program(FLASH_TARGET_OFFSET, buffer, wsz);

    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "Save");
    u8g2.drawStr(20, 28, " Complete");
    u8g2.sendBuffer();
    delay(1000);
#else
    u8g2.drawStr(0, 12, "Save");
    u8g2.drawStr(20, 28, "not support yet");
    u8g2.sendBuffer();
    delay(1000);
#endif
  }

  ///
  /// メニュー決定処理
  ///
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

  ///
  /// メニュー表示＆操作
  ///
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

  ///
  /// キースキャン＆送信
  ///
  void keymode()
  {
    static int capcnt = 0;

    auto &layers = *layerList[os_index];

    static bool layerBackMode = false;

    if (modKey.pressed())
    {
      // MODキー長押し
      if (layerBackMode == false && modKey.long_pressed(1000))
      {
        // 単独で押し続けたらそのままメニューへ
        if (!mod_long_press)
        {
          on_menu = true;
          mod_long_press = true;
          return;
        }
      }
    }
    else if (modKey.release())
    {
      if (layerBackMode == false && mod_long_press == false)
      {
        // 短押しはレイヤー切り替え
        layer_index = (layer_index + 1) % layers.size();
        capcnt = 0;
      }
      layerBackMode = false;
      mod_long_press = false;
    }

    auto putKey = [](const Layer::Key &key)
    {
      if (key.code != '\0')
      {
        keyboard.key_code(key.code, key.mod);
      }
      else
      {
        keyboard.puts(messages[key.str].c_str());
      }
    };

    auto &layer = layers[layer_index];
    if (btnA.repeat())
      putKey(layer.A_);
    if (btnB.repeat())
      putKey(layer.B_);
    if (btnC.repeat())
    {
      if (modKey.pressed())
        layerBackMode = true;
      if (layerBackMode)
      {
        // レイヤーを戻す
        layer_index = (layer_index + layers.size() - 1) % layers.size();
        capcnt = 0;
      }
      else
        putKey(layer.C_);
    }

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
}

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

  overrideMessages();

  load();
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
