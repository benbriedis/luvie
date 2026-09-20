// SPDX-FileCopyrightText: Ben Briedis
// SPDX-License-Identifier: Apache-2.0

#include "debugPort.hpp"
#include <cstdio>

void DebugPort::noteOn(int ch, int pitch, int vel)
{
    std::string name = pitchName ? pitchName(pitch) : std::to_string(pitch);
    printf("[debug] port \"%s\"  note on   ch=%-2d  note=%-4s  vel=%d\n",
           name_.c_str(), ch + 1, name.c_str(), vel);
}

void DebugPort::noteOff(int ch, int pitch)
{
    std::string name = pitchName ? pitchName(pitch) : std::to_string(pitch);
    printf("[debug] port \"%s\"  note off  ch=%-2d  note=%s\n",
           name_.c_str(), ch + 1, name.c_str());
}

void DebugPort::cc(int ch, int num, int val)
{
    printf("[debug] port \"%s\"  cc        ch=%-2d  cc=%-3d  val=%d\n",
           name_.c_str(), ch + 1, num, val);
}

void DebugPort::pitchBend(int ch, int value14)
{
    printf("[debug] port \"%s\"  pitchbend ch=%-2d  val=%d\n",
           name_.c_str(), ch + 1, value14);
}

void DebugPort::programChange(int ch, int bankMsb, int bankLsb, int program)
{
    printf("[debug] port \"%s\"  program   ch=%-2d  bankMsb=%d  bankLsb=%d  program=%d\n",
           name_.c_str(), ch + 1, bankMsb, bankLsb, program);
}

void DebugPort::raw(const uint8_t* msg, int len)
{
    // The base decodes everything the typed printers above cover; only what it has
    // nowhere to go — channel pressure, poly aftertouch — is printed as bytes, so
    // pass-through of those is still visible here.
    const int status = len > 0 ? (msg[0] & 0xF0) : 0;
    if (status != 0xA0 && status != 0xD0) { Port::raw(msg, len); return; }
    printf("[debug] port \"%s\"  %s ch=%-2d ",
           name_.c_str(), status == 0xA0 ? "poly-at  " : "pressure ", (msg[0] & 0x0F) + 1);
    for (int i = 1; i < len; i++) printf(" %02X", msg[i]);
    printf("\n");
}
