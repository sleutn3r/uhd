//
// Copyright 2014 Ettus Research LLC
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <uhd/utils/msg.hpp>
#include "device3_impl.hpp"
#include <algorithm>

using namespace uhd::usrp;

device3_impl::device3_impl()
    : _sid_framer(0)
{
    _type = uhd::device::USRP;
    _async_md.reset(new async_md_type(1000/*messages deep*/));
    _tree = uhd::property_tree::make();
};

//! Returns true if the integer value stored in lhs is smaller than that in rhs
bool _compare_string_indexes(const std::string &lhs, const std::string &rhs)
{
    return boost::lexical_cast<size_t>(lhs) < boost::lexical_cast<size_t>(rhs);
}

void device3_impl::merge_channel_defs(
    const std::vector<rfnoc::block_id_t> &chan_ids,
    const std::vector<uhd::device_addr_t> &chan_args,
    const uhd::direction_t dir
) {
    UHD_ASSERT_THROW(chan_ids.size() == chan_args.size());
    if (dir == uhd::DX_DIRECTION) {
        merge_channel_defs(chan_ids, chan_args, RX_DIRECTION);
        merge_channel_defs(chan_ids, chan_args, TX_DIRECTION);
        return;
    }

    uhd::fs_path chans_root = uhd::fs_path("/channels/") / (dir == RX_DIRECTION ? "rx" : "tx");
    // Store the new positions of the channels:
    std::vector<size_t> chan_idxs;

    // 1. Get sorted list of currently defined channels
    std::vector<std::string> curr_channels = _tree->list(chans_root);
    std::sort(curr_channels.begin(), curr_channels.end(), _compare_string_indexes);

    // 2. Cycle through existing channels to find out where to merge
    //    the new channels. Rules are:
    //    - The order of chan_ids must be preserved
    //    - All block indices that are in chan_ids may be overwritten in the channel definition
    //    - If the channels in chan_ids are not yet in the property tree channel list,
    //      they are appended.
    BOOST_FOREACH(const std::string &chan_idx, curr_channels) {
        rfnoc::block_id_t chan_block_id = _tree->access<rfnoc::block_id_t>(chans_root / chan_idx).get();
        if (std::find(chan_ids.begin(), chan_ids.end(), chan_block_id) != chan_ids.end()) {
            chan_idxs.push_back(boost::lexical_cast<size_t>(chan_idx));
        }
    }
    size_t last_chan_idx = boost::lexical_cast<size_t>(curr_channels.back());
    while (chan_idxs.size() < chan_ids.size()) {
        last_chan_idx++;
        chan_idxs.push_back(last_chan_idx);
    }

    // 3. Write the new channels
    for (size_t i = 0; i < chan_ids.size(); i++) {
        if (not _tree->exists(chans_root / chan_idxs[i])) {
            _tree->create<rfnoc::block_id_t>(chans_root / chan_idxs[i]);
        }
        _tree->access<rfnoc::block_id_t>(chans_root / chan_idxs[i]).set(chan_ids[i]);
        _tree->access<uhd::device_addr_t>(chans_root / chan_idxs[i] / "args").set(chan_args[i]);
    }
}

/***********************************************************************
 * RFNoC-Specific
 **********************************************************************/
void device3_impl::init_radio_ctrl(
        const radio_v_perifs_t &perif,
        const uhd::sid_t &ctrl_sid,
        const size_t mb_i,
        const endianness_t endianness,
        const uhd::rfnoc::radio_ctrl::dboard_type_t dboard_type
) {
    using namespace uhd::rfnoc;

    // 1) Create the block control
    UHD_MSG(status) << "[RFNOC] ------- Radio Setup -----------" << std::endl;
    uhd::rfnoc::make_args_t make_args("Radio");
    make_args.ctrl_iface = perif.ctrl;
    make_args.ctrl_sid = ctrl_sid;
    make_args.device_index = mb_i;
    make_args.tree = _tree->subtree(fs_path("/mboards") / mb_i);
    make_args.is_big_endian = (endianness == ENDIANNESS_BIG);
    radio_ctrl::sptr r_ctrl = boost::dynamic_pointer_cast<radio_ctrl>(block_ctrl_base::make(make_args));

    // 2) Configure the radio control block and the radio itself
    r_ctrl->set_perifs(
            perif.time64,
            perif.framer,
            perif.ddc,
            perif.deframer,
            perif.duc,
            perif.rx_fe,
            perif.tx_fe
    );
    r_ctrl->set_dboard_type(dboard_type);
    r_ctrl->update_muxes(TX_DIRECTION);
    r_ctrl->update_muxes(RX_DIRECTION);

    // 3) Add block to block list and configure default channels
    _rfnoc_block_ctrl.push_back(r_ctrl);

    size_t channel_idx = 0;
    while (_tree->exists(str(boost::format("/channels/tx/%d") % channel_idx))) {
        channel_idx++;
    }
    // This makes the assumption that all radios can Tx and Rx
    _tree->create<uhd::rfnoc::block_id_t>(str(boost::format("/channels/tx/%d") % channel_idx))
            .set(r_ctrl->get_block_id());
    _tree->create<uhd::rfnoc::block_id_t>(str(boost::format("/channels/rx/%d") % channel_idx))
            .set(r_ctrl->get_block_id());
    _tree->create<uhd::device_addr_t>(str(boost::format("/channels/tx/%d/args") % channel_idx))
            .set(uhd::device_addr_t());
    _tree->create<uhd::device_addr_t>(str(boost::format("/channels/rx/%d/args") % channel_idx))
            .set(uhd::device_addr_t());
}
