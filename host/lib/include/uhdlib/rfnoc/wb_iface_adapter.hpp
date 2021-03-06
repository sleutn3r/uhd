//
// Copyright 2016 Ettus Research LLC
// Copyright 2018 Ettus Research, a National Instruments Company
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#ifndef INCLUDED_RFNOC_WB_IFACE_ADAPTER_HPP
#define INCLUDED_RFNOC_WB_IFACE_ADAPTER_HPP

#include <uhd/config.hpp>
#include <uhd/types/wb_iface.hpp>
#include <boost/function.hpp>

namespace uhd {
    namespace rfnoc {

class UHD_API wb_iface_adapter : public uhd::timed_wb_iface
{
public:
    typedef boost::shared_ptr<wb_iface_adapter> sptr;
    typedef boost::function<void(wb_addr_type, uint32_t)> poke32_type;
    typedef boost::function<uint32_t(wb_addr_type)> peek32_type;
    typedef boost::function<uint64_t(wb_addr_type)> peek64_type;
    typedef boost::function<time_spec_t(void)> gettime_type;
    typedef boost::function<void(const time_spec_t&)> settime_type;

    wb_iface_adapter(
        const poke32_type &,
        const peek32_type &,
        const peek64_type &,
        const gettime_type &,
        const settime_type &
    );

    wb_iface_adapter(
        const poke32_type &,
        const peek32_type &,
        const peek64_type &
    );

    virtual ~wb_iface_adapter(void) {};

    virtual void poke32(const wb_addr_type addr, const uint32_t data);
    virtual uint32_t peek32(const wb_addr_type addr);
    virtual uint64_t peek64(const wb_addr_type addr);
    virtual time_spec_t get_time(void);
    virtual void set_time(const time_spec_t& t);

private:
    const poke32_type   poke32_functor;
    const peek32_type   peek32_functor;
    const peek64_type   peek64_functor;
    const gettime_type  gettime_functor;
    const settime_type  settime_functor;
};

}} // namespace uhd::rfnoc

#endif /* INCLUDED_RFNOC_WB_IFACE_ADAPTER_HPP */
