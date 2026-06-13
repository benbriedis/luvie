#include "jackPort.hpp"
#include "jackTransport.hpp"
#include <cstdint>

JackPort::JackPort(JackTransport* jack, std::string name)
    : Port(std::move(name)), jack_(jack)
{
    if (jack_) jack_->addMidiPort(name_);
}

JackPort::~JackPort()
{
    if (jack_) jack_->removeMidiPort(name_);
}

void JackPort::rename(const std::string& n)
{
    if (jack_) jack_->renameMidiPort(name_, n);
    name_ = n;
}

void JackPort::noteOn(int ch, int pitch, int vel)
{
    uint8_t m[3] = { static_cast<uint8_t>(0x90 | (ch & 0x0F)),
                     static_cast<uint8_t>(pitch & 0x7F),
                     static_cast<uint8_t>(vel & 0x7F) };
    if (jack_) jack_->enqueue(name_, m, 3);
}

void JackPort::noteOff(int ch, int pitch)
{
    uint8_t m[3] = { static_cast<uint8_t>(0x80 | (ch & 0x0F)),
                     static_cast<uint8_t>(pitch & 0x7F), 0 };
    if (jack_) jack_->enqueue(name_, m, 3);
}

void JackPort::cc(int ch, int num, int val)
{
    uint8_t m[3] = { static_cast<uint8_t>(0xB0 | (ch & 0x0F)),
                     static_cast<uint8_t>(num & 0x7F),
                     static_cast<uint8_t>(val & 0x7F) };
    if (jack_) jack_->enqueue(name_, m, 3);
}

void JackPort::pitchBend(int ch, int value14)
{
    uint8_t m[3] = { static_cast<uint8_t>(0xE0 | (ch & 0x0F)),
                     static_cast<uint8_t>(value14 & 0x7F),
                     static_cast<uint8_t>((value14 >> 7) & 0x7F) };
    if (jack_) jack_->enqueue(name_, m, 3);
}

void JackPort::programChange(int ch, int bankMsb, int bankLsb, int program)
{
    if (jack_) jack_->sendProgramChange(name_, ch, bankMsb, bankLsb, program);
}
