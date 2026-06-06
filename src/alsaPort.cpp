#include "alsaPort.hpp"
#include <cstdio>

// ── AlsaClient ─────────────────────────────────────────────────────────────────

AlsaClient::~AlsaClient()
{
    if (seq_) snd_seq_close(seq_);
}

snd_seq_t* AlsaClient::seq()
{
    if (tried_) return seq_;
    tried_ = true;
    if (snd_seq_open(&seq_, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
        fprintf(stderr, "AlsaClient: could not open ALSA sequencer\n");
        seq_ = nullptr;
        return nullptr;
    }
    snd_seq_set_client_name(seq_, "luvie");
    return seq_;
}

// ── AlsaPort ───────────────────────────────────────────────────────────────────

AlsaPort::AlsaPort(AlsaClient* client, std::string name)
    : Port(std::move(name)), client_(client)
{
    snd_seq_t* seq = client_ ? client_->seq() : nullptr;
    if (!seq) return;
    portId_ = snd_seq_create_simple_port(
        seq, name_.c_str(),
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (portId_ < 0) {
        fprintf(stderr, "AlsaPort: could not create port '%s'\n", name_.c_str());
        portId_ = -1;
    }
}

AlsaPort::~AlsaPort()
{
    snd_seq_t* seq = client_ ? client_->seq() : nullptr;
    if (seq && portId_ >= 0) snd_seq_delete_simple_port(seq, portId_);
}

void AlsaPort::rename(const std::string& n)
{
    name_ = n;
    snd_seq_t* seq = client_ ? client_->seq() : nullptr;
    if (!seq || portId_ < 0) return;
    snd_seq_port_info_t* info;
    snd_seq_port_info_alloca(&info);
    if (snd_seq_get_port_info(seq, portId_, info) < 0) return;
    snd_seq_port_info_set_name(info, n.c_str());
    snd_seq_set_port_info(seq, portId_, info);
}

void AlsaPort::send(snd_seq_event_t& ev)
{
    snd_seq_t* seq = client_ ? client_->seq() : nullptr;
    if (!seq || portId_ < 0) return;
    snd_seq_ev_set_source(&ev, portId_);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    snd_seq_event_output_direct(seq, &ev);
}

void AlsaPort::noteOn(int ch, int pitch, int vel)
{
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_noteon(&ev, ch & 0x0F, pitch & 0x7F, vel & 0x7F);
    send(ev);
}

void AlsaPort::noteOff(int ch, int pitch)
{
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_noteoff(&ev, ch & 0x0F, pitch & 0x7F, 0);
    send(ev);
}

void AlsaPort::cc(int ch, int num, int val)
{
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_controller(&ev, ch & 0x0F, num & 0x7F, val & 0x7F);
    send(ev);
}

void AlsaPort::pitchBend(int ch, int value14)
{
    // Our value14 is 0..16383 (center 8192); ALSA wants signed -8192..8191.
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_pitchbend(&ev, ch & 0x0F, value14 - 8192);
    send(ev);
}

void AlsaPort::programChange(int ch, int bankMsb, int bankLsb, int program)
{
    if (bankMsb >= 0) cc(ch, 0,  bankMsb);
    if (bankLsb >= 0) cc(ch, 32, bankLsb);
    if (program >= 0) {
        snd_seq_event_t ev;
        snd_seq_ev_clear(&ev);
        snd_seq_ev_set_pgmchange(&ev, ch & 0x0F, program & 0x7F);
        send(ev);
    }
}
