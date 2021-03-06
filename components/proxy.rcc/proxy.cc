/*
 * THIS FILE WAS ORIGINALLY GENERATED ON Sun Aug 10 16:57:14 2014 EDT
 * BASED ON THE FILE: proxy.xml
 * YOU *ARE* EXPECTED TO EDIT IT
 *
 * This file contains the implementation skeleton for the proxy worker in C++
 */

#include "proxy-worker.hh"

using namespace OCPI::RCC; // for easy access to RCC data types and constants
using namespace ProxyWorkerTypes;

class ProxyWorker : public ProxyWorkerBase {
  RCCResult start() {
    slave.set_biasValue(m_properties.proxybias);
    return RCC_OK;
  }
  RCCResult run(bool /*timedout*/) {
    return RCC_ADVANCE;
  }
};

PROXY_START_INFO
// Insert any static info assignments here (memSize, memSizes, portInfo)
// e.g.: info.memSize = sizeof(MyMemoryStruct);
PROXY_END_INFO
