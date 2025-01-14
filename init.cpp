/* SPDX-License-Identifier: LGPL-3.0-or-later */

/** @file
 * @brief init a pmc application
 *
 * @author Erez Geva <ErezGeva2@@gmail.com>
 * @copyright 2021 Erez Geva
 *
 */
#include <unistd.h>
#include "init.h"
#include "err.h"

void Init::close()
{
    if(m_sk != nullptr) {
        m_sk->close();
        delete m_sk;
        m_sk = nullptr;
    }
}

int Init::proccess(const Options &opt)
{
    char net_select = opt.get_net_transport();
    /* handle configuration file */
    if(opt.have('f') && !m_cfg.read_cfg(opt.val('f')))
        return -1;
    if(net_select == 0)
        net_select = m_cfg.network_transport();
    std::string interface;
    if(opt.have('i') && !opt.val('i').empty())
        interface = opt.val('i');
    IfInfo ifObj;
    MsgParams prms = m_msg.getParams();
    if(net_select != 'u') {
        if(interface.empty()) {
            PMC_ERROR("missing interface");
            return -1;
        }
        if(!ifObj.initUsingName(interface))
            return -1;
        Binary clockIdentity(ifObj.mac());
        clockIdentity.eui48ToEui64();
        clockIdentity.copy(prms.self_id.clockIdentity.v);
        prms.self_id.portNumber = 1;
        m_use_uds = false;
    }
    if(opt.have('b'))
        prms.boundaryHops = opt.val_i('b');
    else
        prms.boundaryHops = 1;
    if(opt.have('d'))
        prms.domainNumber = opt.val_i('d');
    else
        prms.domainNumber = m_cfg.domainNumber(interface);
    if(opt.have('t'))
        prms.transportSpecific = strtol(opt.val_c('t'), nullptr, 16);
    else
        prms.transportSpecific = m_cfg.transportSpecific(interface);
    prms.useZeroGet = opt.val('z') == "1";
    switch(net_select) {
        case 'u': {
            SockUnix *sku = new SockUnix;
            if(sku == nullptr) {
                PMC_ERROR("failed to allocate SockUnix");
                return -1;
            }
            m_sk = sku;
            std::string uds_address;
            if(opt.have('s'))
                uds_address = opt.val('s');
            else
                uds_address = m_cfg.uds_address(interface);
            if(!sku->setDefSelfAddress() || !sku->init() ||
                !sku->setPeerAddress(uds_address)) {
                PMC_ERROR("failed to create transport");
                return -1;
            }
            prms.self_id.portNumber = getpid() & 0xffff;
            m_use_uds = true;
            break;
        }
        default:
        case '4': {
            SockIp4 *sk4 = new SockIp4;
            if(sk4 == nullptr) {
                PMC_ERROR("failed to allocate SockIp4");
                return -1;
            }
            m_sk = sk4;
            if(!sk4->setAll(ifObj, m_cfg, interface)) {
                PMC_ERROR("failed to set transport");
                return -1;
            }
            if(opt.have('T') && !sk4->setUdpTtl(opt.val_i('T'))) {
                PMC_ERROR("failed to set udp_ttl");
                return -1;
            }
            if(!sk4->init()) {
                PMC_ERROR("failed to init transport");
                return -1;
            }
            break;
        }
        case '6': {
            SockIp6 *sk6 = new SockIp6;
            if(sk6 == nullptr) {
                PMC_ERROR("failed to allocate SockIp6");
                return -1;
            }
            m_sk = sk6;
            if(!sk6->setAll(ifObj, m_cfg, interface)) {
                PMC_ERROR("failed to set transport");
                return -1;
            }
            if(opt.have('T') && !sk6->setUdpTtl(opt.val_i('T'))) {
                PMC_ERROR("failed to set udp_ttl");
                return -1;
            }
            if(opt.have('S') && !sk6->setScope(opt.val_i('S'))) {
                PMC_ERROR("failed to set udp6_scope");
                return -1;
            }
            if(!sk6->init()) {
                PMC_ERROR("failed to init transport");
                return -1;
            }
            break;
        }
        case '2': {
            SockRaw *skr = new SockRaw;
            if(skr == nullptr) {
                PMC_ERROR("failed to allocate SockRaw");
                return -1;
            }
            m_sk = skr;
            if(!skr->setAll(ifObj, m_cfg, interface)) {
                PMC_ERROR("failed to set transport");
                return -1;
            }
            if(opt.have('P') &&
                !skr->setSocketPriority(opt.val_i('P'))) {
                PMC_ERROR("failed to set socket_priority");
                return -1;
            }
            Binary mac;
            if(opt.have('M') && (!mac.fromMac(opt.val('M')) ||
                    !skr->setPtpDstMac(mac))) {
                PMC_ERROR("failed to set ptp_dst_mac");
                return -1;
            }
            if(!skr->init()) {
                PMC_ERROR("failed to init transport");
                return -1;
            }
            break;
        }
    }
    m_msg.updateParams(prms);
    return 0;
}
