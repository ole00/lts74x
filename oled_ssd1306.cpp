/*
*****************************************************************************
Based on Deneyap OLED library v 1.0.3
*****************************************************************************
*/

#include "oled_ssd1306.h"

/*
There are some SSD1306 OLED displays with a buggy controller which do not
properly increment Y position in vertical mode.  These seem to be SSD1306 clones 
with modern/crispier OLED screen. The display PCB of these clones is a few
mm smaller than the original (mounting hole pitch does not match).
Because of these buggy controllers the Vertical mode is not used. 
Instead, the Vertical mode is simulated in Horizontal mode by setting a vertical
clip of exactly one column.

If you ask where is the frame buffer, then there is none. The text pixels are drawn directly
to the controller. We use a 8 byte line buffer comprising a single vertical line to speed
up the data transfers and rendering.
*/


#define FONT_DATA_OFFSET 2


#define SSD1306_Command_Mode          0x80
#define SSD1306_Data_Mode             0x40
#define SSD1306_Display_Off_Cmd       0xAE
#define SSD1306_Display_On_Cmd        0xAF
#define SSD1306_Normal_Display_Cmd    0xA6
#define SSD1306_Inverse_Display_Cmd   0xA7
#define SSD1306_Set_Brightness_Cmd    0x81

static const uint8_t *m_font;     // Current font.
static uint8_t m_address;         // I2C address
static uint8_t m_font_width;      // Font witdth.
static uint8_t m_draw_pos;        // screen position - current column

static void send_command(unsigned char command) {
  Wire.beginTransmission(m_address); // begin I2C communication
  Wire.write(SSD1306_Command_Mode); // Set OLED Command mode
  Wire.write(command);
  Wire.endTransmission(); // End I2C communication
}

static void reset_segment() {
  Wire.beginTransmission(m_address);
  Wire.write(SSD1306_Command_Mode);
  Wire.write(0x22);
  Wire.write(SSD1306_Command_Mode);
  Wire.write(0);
  Wire.write(SSD1306_Command_Mode);
  Wire.write(7);
  Wire.endTransmission();
}

static void send_data(unsigned char* data, unsigned char len) {
  uint8_t resetSegment = len != 8;

  Wire.beginTransmission(m_address);
  if (m_draw_pos != 0xFF) {
      // set the clip region to be exactly one column (simulating vertical mode)
      Wire.write(SSD1306_Command_Mode);
      Wire.write(0x21);
      Wire.write(SSD1306_Command_Mode);
      Wire.write(m_draw_pos);
      Wire.write(SSD1306_Command_Mode);
      Wire.write(m_draw_pos);
  }

  //send pixel data
  Wire.write(SSD1306_Data_Mode); 
  while (len--) {
      Wire.write(*data);
      data++;
  }
  m_draw_pos++;
  Wire.endTransmission();

  if (resetSegment) {
      reset_segment();
  }
}


static void oled_init(void) {
  send_command(0xAE); // display off
  send_command(0xA6); // Set Normal Display (default)
  send_command(0xAE); // DISPLAYOFF
  send_command(0xD5); // SETDISPLAYCLOCKDIV
  send_command(0x80); // the suggested ratio 0x80
  send_command(0xA8); // SSD1306_SETMULTIPLEX
  send_command(0x3F);
  send_command(0xD3);       // SETDISPLAYOFFSET
  send_command(0);
  send_command(0x40 | 0x0); // SETSTARTLINE
  send_command(0x8D);       // CHARGEPUMP
  send_command(0x14);
  send_command(0xA1); // SEGREMAP   Mirror screen horizontally (A0)
  send_command(0xC8); // COMSCANDEC Rotate screen vertically (C0)
  send_command(0xDA); // 0xDA
  send_command(0x12); // COMSCANDEC
  send_command(0x81); // SETCONTRAST
  send_command(0xCF); //
  send_command(0xd9); // SETPRECHARGE
  send_command(0xF1);
  send_command(0xDB); // SETVCOMDETECT
  send_command(0x40);
  send_command(0xA4); // DISPLAYALLON_RESUME
  send_command(0xA6); // NORMALDISPLAY
  send_command(0x2E); // Stop scroll
  send_command(0x20); // Set Memory Addressing Mode
  send_command(0); // Horizontal mode - for compatibility. Some OLED display clones do not support vertical mode

  
  m_font = font_custom;
  m_font_width = pgm_read_byte(&m_font[0]);
  oled_clear_display();
}

bool oled_begin(uint8_t address, TwoWire &wirePort) {
  m_address = address >> 1;
  Wire.begin();
  Wire.setClock(400000UL);
  oled_init();
  if (!Wire.endTransmission())
    return true;
  return false;
}

void oled_clear_display(void) {
  uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0 };
  unsigned char i, j;
  oled_set_y(0);
  send_command(SSD1306_Display_Off_Cmd); // display off
  for (j = 0; j < 128; j++) {
    send_data(data, 8);
  }
  send_command(SSD1306_Display_On_Cmd); // display on
}




void oled_set_brightness(unsigned char brightness) {
  send_command(SSD1306_Set_Brightness_Cmd);
  send_command(brightness);
}


void oled_set_y(unsigned char y) {
  m_draw_pos = y;
}


static void draw_glyph_line(uint8_t* buf, uint8_t l, uint8_t x) {
  uint8_t i = 0;
  uint8_t offset = 7 - (x >> 3); // 8 bytes per row
  uint8_t maskX = 1 << (7 - (x & 0b111));
  while (i < 7) { //font glyph  is 7 pixels high
    //pixel should be drawn
    if ((1 << i) & l) {
      buf[offset] |= maskX;
    }
    offset += 8; //display width in bytes (64 pixels / 8)
    i++;
  }
}

static void draw_glyph_line_inv(uint8_t* buf, uint8_t l, uint8_t x) {
  uint8_t i = 0;
  uint8_t offset = 7 - (x >> 3); // 8 bytes per row
  uint8_t maskX = 1 << (7 - (x & 0b111));
  while (i < 8) { //font glyph  is 7 pixels high + 1 bottom line
    //pixel should be drawn
    if (!((1 << i) & l)) {
      buf[offset] |= maskX;
    }
    offset += 8; //display width in bytes (64 pixels / 8)
    i++;
  }
}

void oled_draw_string(char* text, bool inverse) {
  uint8_t buf[8 * 8]= {0};
  uint8_t i = 0, j;
  uint8_t x = 0;

  while (text[i]) {
    uint16_t index = (text[i] - 32) * m_font_width +  FONT_DATA_OFFSET;
    for (j = 0; j < m_font_width; j++) {
      uint8_t l = pgm_read_byte(&m_font[index]);
      if (inverse) {
        draw_glyph_line_inv(buf, l, x);
      } else {
        draw_glyph_line(buf, l, x);
      }
      x++;
      index++;
    }
    if (inverse) {
      draw_glyph_line_inv(buf, 0x0, x); //draw bottom line
    }
    x++;
    i++;
  }

  for (i = 0; i < 56; i += 8) {
    send_data(buf + i, 8);
  }
  if (inverse) {
    send_data(buf + 56, 8);
  }

}

void oled_draw_h_line(unsigned char x1, unsigned char x2, unsigned char y, unsigned char color) {
  uint8_t i, s = 255, cnt = 0;
  uint8_t buf[8]= {0};

  while (x1 < x2) {
    i = 63 - x1;
    buf[i >> 3] |= 1 << ((i & 0b111));
    x1++;
  }
  i = 0;
  while (i < 8) {
    if (buf[i] != 0) {
      if (s == 255) {
        s = i;
      }
      cnt++;
    }
    i++;
  }

  m_draw_pos = y;
  cnt += s;
  s = 0;
  
  if (color) {
    send_data(buf + s, cnt);
  } else {
    uint8_t buf2[8]= {0};
    send_data(buf2 + s, cnt);
  }
}

void oled_draw_v_line(unsigned char y1, unsigned char y2, unsigned char x) {
  uint8_t i, b;
  uint8_t buf[8];
  uint8_t pos = m_draw_pos;
  //undo the clip (set clip to cover the whole screen)
  Wire.beginTransmission(m_address);
  Wire.write(SSD1306_Command_Mode);
  Wire.write(0x21);
  Wire.write(SSD1306_Command_Mode);
  Wire.write(0);
  Wire.write(SSD1306_Command_Mode);
  Wire.write(127);
  Wire.write(SSD1306_Command_Mode);
  Wire.write(0x22);
  Wire.write(SSD1306_Command_Mode);
  Wire.write(0);
  Wire.write(SSD1306_Command_Mode);
  Wire.write(7);
  Wire.endTransmission();

  i = 63 - x;
  b = 1 << ((i & 0b111));

  send_command(0xB0 + (i >> 3));                 // set page address
  send_command(0x00 + (y1 & 0x0F));        // set column lower addr
  send_command(0x10 + ((y1 >> 4) & 0x0F)); // set column higher addr

  for (i = 0; i < 8; i++) buf[i] = b;

  while (y1 < y2) {
    uint8_t d = y2 - y1;
    if (d > 8) {
      d = 8;
    }
    m_draw_pos = 0xFF; // draw in raw mode (without changing the clip)
    send_data(buf, d);
    y1 += d;
  }

  m_draw_pos = pos;
  reset_segment();
}

void oled_set_inverse(bool inverse) {
  send_command(inverse ? SSD1306_Inverse_Display_Cmd : SSD1306_Normal_Display_Cmd);
}


