/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdint>
#include <cstring>

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/clock.hpp>

namespace
{

constexpr uint32_t PLL_REF_CLOCK_HZ = 32768;

constexpr uint32_t STATUS_OFFSET = 0x0;
constexpr uint32_t CFG1_OFFSET = 0x4;
constexpr uint32_t CFG2_OFFSET = 0x8;
constexpr uint32_t CFG3_OFFSET = 0xc;

constexpr uint32_t STATUS_LOCK = 1U << 0;

constexpr uint32_t CFG1_PLL_ENABLE = 1U << 0;
constexpr uint32_t CFG1_POSTDIV_OUTPUT_ENABLE = 1U << 1;

constexpr uint32_t CFG2_FBDIV_MASK = 0x3fff;
constexpr uint32_t CFG2_POSTDIV_MASK = 0xf;
constexpr uint32_t CFG2_POSTDIV_SHIFT = 14;
constexpr uint32_t CFG2_VCO_DIV2 = 1U << 18;

// The Siracusa PLL starts at approximately 50 MHz after reset.
constexpr uint32_t RESET_CFG1 = CFG1_PLL_ENABLE | CFG1_POSTDIV_OUTPUT_ENABLE;
constexpr uint32_t RESET_CFG2 = 3052U | CFG2_VCO_DIV2;

} // namespace


class SiracusaPll : public vp::Component
{
public:
    SiracusaPll(vp::ComponentConf &config);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);

private:
    static void ref_clock_sync(vp::Block *__this, bool value);
    static void ref_clock_set_frequency(vp::Block *__this, int64_t frequency);

    void reset(bool active);
    void update_frequency();

    vp::Trace trace;
    vp::IoSlave input;
    vp::ClockMaster clock_output;
    vp::ClockSlave ref_clock_input;

    uint32_t status;
    uint32_t cfg1;
    uint32_t cfg2;
    uint32_t cfg3;
};


SiracusaPll::SiracusaPll(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    input.set_req_meth(&SiracusaPll::req);
    new_slave_port("input", &input);

    new_master_port("clock_out", &clock_output);

    ref_clock_input.set_sync_meth(&SiracusaPll::ref_clock_sync);
    ref_clock_input.set_set_frequency_meth(&SiracusaPll::ref_clock_set_frequency);
    new_slave_port("ref_clock", &ref_clock_input);
}


void SiracusaPll::reset(bool active)
{
    if (active)
    {
        status = STATUS_LOCK;
        cfg1 = RESET_CFG1;
        cfg2 = RESET_CFG2;
        cfg3 = 0;
    }
    else
    {
        update_frequency();
    }
}


void SiracusaPll::update_frequency()
{
    uint64_t frequency = 0;

    if ((cfg1 & CFG1_PLL_ENABLE) && (cfg1 & CFG1_POSTDIV_OUTPUT_ENABLE))
    {
        uint32_t fbdiv = cfg2 & CFG2_FBDIV_MASK;
        uint32_t postdiv = ((cfg2 >> CFG2_POSTDIV_SHIFT) & CFG2_POSTDIV_MASK) + 1;
        uint32_t vco_divider = (cfg2 & CFG2_VCO_DIV2) ? 2 : 1;

        frequency = static_cast<uint64_t>(fbdiv) * PLL_REF_CLOCK_HZ;
        frequency /= postdiv * vco_divider;
        status |= STATUS_LOCK;
    }
    else
    {
        status &= ~STATUS_LOCK;
    }

    trace.msg("Setting PLL output frequency (frequency: %llu Hz, cfg1: 0x%x, cfg2: 0x%x)\n",
        static_cast<unsigned long long>(frequency), cfg1, cfg2);
    clock_output.set_frequency(frequency);
}


vp::IoReqStatus SiracusaPll::req(vp::Block *__this, vp::IoReq *req)
{
    SiracusaPll *_this = static_cast<SiracusaPll *>(__this);
    uint64_t offset = req->get_addr();
    uint64_t size = req->get_size();
    uint8_t *data = req->get_data();
    bool is_write = req->get_is_write();

    if (size != sizeof(uint32_t) || (offset & 0x3) != 0)
    {
        return vp::IO_REQ_INVALID;
    }

    uint32_t *reg = nullptr;
    switch (offset)
    {
        case STATUS_OFFSET: reg = &_this->status; break;
        case CFG1_OFFSET: reg = &_this->cfg1; break;
        case CFG2_OFFSET: reg = &_this->cfg2; break;
        case CFG3_OFFSET: reg = &_this->cfg3; break;
        default: return vp::IO_REQ_INVALID;
    }

    if (is_write)
    {
        if (offset == STATUS_OFFSET)
        {
            return vp::IO_REQ_INVALID;
        }

        std::memcpy(reg, data, sizeof(uint32_t));
        if (offset == CFG1_OFFSET || offset == CFG2_OFFSET)
        {
            _this->update_frequency();
        }
    }
    else
    {
        std::memcpy(data, reg, sizeof(uint32_t));
    }

    return vp::IO_REQ_OK;
}


void SiracusaPll::ref_clock_sync(vp::Block *__this, bool value)
{
    (void)__this;
    (void)value;
}


void SiracusaPll::ref_clock_set_frequency(vp::Block *__this, int64_t frequency)
{
    (void)__this;
    (void)frequency;
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new SiracusaPll(config);
}
