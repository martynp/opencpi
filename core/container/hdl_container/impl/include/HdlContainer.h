#ifndef HdlContainer_H
#define HdlContainer_H
#include <uuid/uuid.h>
#include "HdlAccess.h"
#include "OcpiContainerManager.h"

namespace OCPI {
  namespace HDL {
    class Artifact;
    class Port;
    class Application;
    class Accessor;
    class Container
      : public OCPI::Container::ContainerBase<Driver, Container, Application, Artifact>,
	private Access {
      Access m_bufferSpace;
      std::string m_endpoint;
      std::string m_device, m_esn, m_position, m_loadParams;
      uuid_t m_loadedUUID;
      friend class WciControl;
      friend class Driver;
      friend class Port;
      friend class Artifact;
    protected:
      Container(const char *name,
		Access &controlAccessor,
		Access &dataAccessor,
		std::string &endpoint,
		ezxml_t config = NULL, const OCPI::API::PValue *params = NULL);
    public:
      virtual ~Container();
    protected:
      bool isLoadedUUID(const std::string &uuid);
      void getWorkerAccess(unsigned index,
			   Access &worker,
			   Access &properties);
      void releaseWorkerAccess(unsigned index,
			       Access & worker,
			       Access & properties);
    public:
      void start();
      void stop();
      bool dispatch();
      OCPI::Container::Artifact &
	createArtifact(OCPI::Library::Artifact &lart, const OCPI::API::PValue *artifactParams);
      OCPI::API::ContainerApplication *
	createApplication(const char *name, const OCPI::Util::PValue *props)
	throw ( OCPI::Util::EmbeddedException );
      bool needThread();
    };
  }
}
#endif
