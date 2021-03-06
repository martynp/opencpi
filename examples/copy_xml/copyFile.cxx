#include <cstdio>
#include <cassert>
#include <string>
#include "OcpiApi.h"

namespace OA = OCPI::API;

int main(int argc, char **argv) {
  std::string hello("<application package='ocpi'>"
		    // instance name defaults to file_read since there is only one
		    "  <instance component='file_read'>"
		    "    <property name='filename' value='hello.file'/>"
		    "    <property name='messageSize' value='4'/>"
		    "  </instance>"
		    "  <instance component='file_write'>"
		    "    <property name='filename' value='out.file'/>"
		    "  </instance>"
		    "  <connection ");
  if (argv[1]) {
    hello += "transport='";
    hello += argv[1];
    hello += "'";
  }
  hello +=          ">"
                    "<port instance='file_read' name='out'/>"
		    "    <port instance='file_write' name='in'/>"
		    "  </connection>"
		    "</application>";

  try {
    OA::Application app(hello);
    fprintf(stderr, "Application XML parsed and deployments (containers and implementations) chosen\n");
    app.initialize();
    fprintf(stderr, "Application established: containers, workers, connections all created\n");
    fprintf(stderr, "Communication with the application established\n");
    app.start();
    fprintf(stderr, "Application started/running\n");
    app.wait();
    fprintf(stderr, "Application finished\n");
    std::string name, value;
    for (unsigned n = 0; app.getProperty(n, name, value); n++)
      fprintf(stderr, "Property %2u: %s = \"%s\"\n", n, name.c_str(), value.c_str());
    return 0;
  } catch (std::string &e) {
    fprintf(stderr, "Exception thrown: %s\n", e.c_str());
  }
  return 1;
}
