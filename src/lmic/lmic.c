
#include "lmic.h"

bit_t LMIC_setupChannel (u1_t chidx, u4_t freq, u2_t drmap, s1_t band) {
    if( chidx < 72 || chidx >= 72+MAX_XCHANNELS )
        return 0; // channels 0..71 are hardwired
    chidx -= 72;
    LMIC.xchFreq[chidx] = freq;
    LMIC.xchDrMap[chidx] = drmap==0 ? DR_RANGE_MAP(DR_SF10,DR_SF8C) : drmap;
    LMIC.channelMap[chidx>>4] |= (1<<(chidx&0xF));
    return 1;
}

void  LMIC_selectSubBand (u1_t band) {
  ASSERT(band < 8);
  for (int b=0; b<8; ++b) {
    if (band==b)
      LMIC_enableSubBand(b);
    else
      LMIC_disableSubBand(b);
  }
}

void LMIC_setDrTxpow (dr_t dr, s1_t txpow) {
    setDrTxpow(DRCHG_SET, dr, txpow);
}

int LMIC_setTxData2 (u1_t port, xref2u1_t data, u1_t dlen, u1_t confirmed) {
    if( dlen > SIZEOFEXPR(LMIC.pendTxData) )
        return -2;
    if( data != (xref2u1_t)0 )
        os_copyMem(LMIC.pendTxData, data, dlen);
    LMIC.pendTxConf = confirmed;
    LMIC.pendTxPort = port;
    LMIC.pendTxLen  = dlen;
    LMIC_setTxData();
    return 0;
}

void LMIC_reset (void) {
    EV(devCond, INFO, (e_.reason = EV::devCond_t::LMIC_EV,
                       e_.eui    = MAIN::CDEV->getEui(),
                       e_.info   = EV_RESET));
    os_radio(RADIO_RST);
    os_clearCallback(&LMIC.osjob);

    os_clearMem((xref2u1_t)&LMIC,SIZEOFEXPR(LMIC));
    LMIC.devaddr      =  0;
    LMIC.devNonce     =  os_getRndU2();
    LMIC.opmode       =  OP_NONE;
    LMIC.errcr        =  CR_4_5;
    LMIC.adrEnabled   =  FCT_ADREN;
    LMIC.dn2Dr        =  DR_DNW2;   // we need this for 2nd DN window of join accept
    LMIC.dn2Freq      =  FREQ_DNW2; // ditto
    LMIC.rxDelay      =  DELAY_DNW1;
#if !defined(DISABLE_PING)
    LMIC.ping.freq    =  FREQ_PING; // defaults for ping
    LMIC.ping.dr      =  DR_PING;   // ditto
    LMIC.ping.intvExp =  0xFF;
#endif // !DISABLE_PING
#if defined(CFG_us915)
    initDefaultChannels();
#endif
    DO_DEVDB(LMIC.devaddr,      devaddr);
    DO_DEVDB(LMIC.devNonce,     devNonce);
    DO_DEVDB(LMIC.dn2Dr,        dn2Dr);
    DO_DEVDB(LMIC.dn2Freq,      dn2Freq);
#if !defined(DISABLE_PING)
    DO_DEVDB(LMIC.ping.freq,    pingFreq);
    DO_DEVDB(LMIC.ping.dr,      pingDr);
    DO_DEVDB(LMIC.ping.intvExp, pingIntvExp);
#endif // !DISABLE_PING
}

void LMIC_setSession (u4_t netid, devaddr_t devaddr, xref2u1_t nwkKey, xref2u1_t artKey) {
    LMIC.netid = netid;
    LMIC.devaddr = devaddr;
    if( nwkKey != (xref2u1_t)0 )
        os_copyMem(LMIC.nwkKey, nwkKey, 16);
    if( artKey != (xref2u1_t)0 )
        os_copyMem(LMIC.artKey, artKey, 16);

#if defined(CFG_eu868)
    initDefaultChannels(0);
#endif

    LMIC.opmode &= ~(OP_JOINING|OP_TRACK|OP_REJOIN|OP_TXRXPEND|OP_PINGINI);
    LMIC.opmode |= OP_NEXTCHNL;
    stateJustJoined();
    DO_DEVDB(LMIC.netid,   netid);
    DO_DEVDB(LMIC.devaddr, devaddr);
    DO_DEVDB(LMIC.nwkKey,  nwkkey);
    DO_DEVDB(LMIC.artKey,  artkey);
    DO_DEVDB(LMIC.seqnoUp, seqnoUp);
    DO_DEVDB(LMIC.seqnoDn, seqnoDn);
}

// Enable/disable link check validation.
// LMIC sets the ADRACKREQ bit in UP frames if there were no DN frames
// for a while. It expects the network to provide a DN message to prove
// connectivity with a span of UP frames. If this no such prove is coming
// then the datarate is lowered and a LINK_DEAD event is generated.
// This mode can be disabled and no connectivity prove (ADRACKREQ) is requested
// nor is the datarate changed.
// This must be called only if a session is established (e.g. after EV_JOINED)
void LMIC_setLinkCheckMode (bit_t enabled) {
    LMIC.adrChanged = 0;
    LMIC.adrAckReq = enabled ? LINK_CHECK_INIT : LINK_CHECK_OFF;
}