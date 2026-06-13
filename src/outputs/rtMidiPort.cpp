#include "rtMidiPort.hpp"
#include <RtMidi.h>
#include <cstdio>

RtMidiPort::RtMidiPort(std::string name)
    : Port(std::move(name))
{
    try {
        out_ = std::make_unique<RtMidiOut>(RtMidi::UNSPECIFIED, "luvie");
        out_->openVirtualPort(name_);
    } catch (const RtMidiError& e) {
        fprintf(stderr, "RtMidiPort: could not open '%s': %s\n",
                name_.c_str(), e.getMessage().c_str());
        out_.reset();
    }
}

RtMidiPort::~RtMidiPort() = default;   // unique_ptr closes the virtual port

void RtMidiPort::rename(const std::string& n)
{
    name_ = n;
    if (!out_) return;
    // Renames the live virtual port in place, preserving its connections.
    try { out_->setPortName(n); }
    catch (const RtMidiError&) {}
}

void RtMidiPort::send3(int status, int d1, int d2)
{
    if (!out_) return;
    unsigned char msg[3] = {
        (unsigned char)(status & 0xFF),
        (unsigned char)(d1 & 0x7F),
        (unsigned char)(d2 & 0x7F),
    };
    try { out_->sendMessage(msg, 3); }
    catch (const RtMidiError&) {}
}

void RtMidiPort::noteOn (int ch, int pitch, int vel) { send3(0x90 | (ch & 0x0F), pitch, vel); }
void RtMidiPort::noteOff(int ch, int pitch)          { send3(0x80 | (ch & 0x0F), pitch, 0);   }
void RtMidiPort::cc     (int ch, int num, int val)   { send3(0xB0 | (ch & 0x0F), num, val);   }

void RtMidiPort::pitchBend(int ch, int value14)
{
    int v = value14 & 0x3FFF;   // 0..16383, center 8192; split into 7-bit LSB/MSB
    send3(0xE0 | (ch & 0x0F), v & 0x7F, (v >> 7) & 0x7F);
}

void RtMidiPort::programChange(int ch, int bankMsb, int bankLsb, int program)
{
    if (bankMsb >= 0) cc(ch, 0,  bankMsb);
    if (bankLsb >= 0) cc(ch, 32, bankLsb);
    if (program >= 0) {
        if (!out_) return;
        unsigned char msg[2] = {
            (unsigned char)(0xC0 | (ch & 0x0F)),
            (unsigned char)(program & 0x7F),
        };
        try { out_->sendMessage(msg, 2); }
        catch (const RtMidiError&) {}
    }
}
