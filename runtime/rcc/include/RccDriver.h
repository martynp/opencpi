#ifndef RCC_DRIVER_H
#define RCC_DRIVER_H
#include <pthread.h>
#include "ContainerManager.h"

namespace OCPI {
  namespace RCC {
    extern const char *rcc;
    class Container;
    class Driver : public OCPI::Container::DriverBase<Driver, Container, rcc> {
      //      OCPI::DataTransport::TransportGlobal *m_tpg_events, *m_tpg_no_events;
      //      unsigned m_count;
    public:
      static pthread_key_t s_threadKey;
      Driver() throw();
      OCPI::Container::Container *
	probeContainer(const char *which, std::string &error, const OCPI::API::PValue *props)
	throw ( OCPI::Util::EmbeddedException );
      // Per driver discovery routine to create devices
      unsigned
	search(const OCPI::API::PValue* props, const char **exclude, bool discoveryOnly)
	throw ( OCPI::Util::EmbeddedException );
      ~Driver() throw ( );
    };
  }
}
#endif
