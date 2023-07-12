#pragma once

constexpr uint16_t SOI = 0xFFD8;
constexpr uint16_t EOI = 0xFFD9;
constexpr uint16_t SOS = 0xFFDA;
constexpr uint16_t DQT = 0xFFDB;
constexpr uint16_t DRI = 0xFFDD;

constexpr uint16_t SOF = 0xFFC0;
constexpr uint16_t DHT = 0xFFC4;

constexpr uint16_t RST[8] = {0xFFD0, 0xFFD1, 0xFFD2, 0xFFD3, 0xFFd4, 0xFFD5, 0xFFD6, 0xFFD7};