#include <assert.h>
#include "hdl.h"
#include "assembly.h"

Port::
Port(Worker &w, ezxml_t x, Port *sp, int ordinal, WIPType type, const char *defaultName,
     const char *&err)
  : m_clone(false), m_worker(&w), m_ordinal(0), count(0), master(false), m_xml(x), type(type),
    pattern(NULL), clock(0), clockPort(0), myClock(false), m_specXml(x) {
  if (sp) {
    // A sort of copy constructor from a spec port to an impl port
    m_name = sp->m_name;
    m_ordinal = sp->m_ordinal;
    count = sp->count; // may be overridden?
    m_countExpr = sp->m_countExpr;
    master = sp->master;
    m_specXml = sp->m_xml;
  } else {
    const char *name = ezxml_cattr(x, "Name");
    m_name = name ? name : "";
    const char *portImplName = ezxml_cattr(x, "implName");
    if (m_name.empty()) {
      if (portImplName)
	m_name = portImplName;
      else if (defaultName)
	if (ordinal == -1)
	  m_name = defaultName;
	else
	  OU::format(m_name, "%s%u", defaultName, ordinal);
      else {
	err = "Missing \"name\" attribute for port";
	return;
      }
    } else if (portImplName) {
      err = OU::esprintf("Both \"Name\" and \"ImplName\" attributes of %s element are present",
			 x->name);
      return;
    }
    if (w.findPort(m_name.c_str())) {
      err = OU::esprintf("Can't create port named \"%s\" since it already exists",
			 m_name.c_str());
      return;
    }
    if ((err = OE::getBoolean(m_xml, "master", &master)) ||
	(err = getExprNumber(m_xml, "count", count, NULL, &m_countExpr, &w)))
      return;
    m_ordinal = w.m_ports.size();
  }
  pattern = ezxml_cattr(m_xml, "Pattern");
  if (sp)
    w.m_ports[m_ordinal] = this;
  else if (type == WCIPort && !master) {
    // UGLY:  we manage the port list here, and a slave control port must be
    // the first, which means we need to hack ordinals when we add a control port
    // after other ports are established
    for (PortsIter pi = w.m_ports.begin(); pi != w.m_ports.end(); pi++)
      (*pi)->m_ordinal++;
    m_ordinal = 0;
    w.m_ports.insert(w.m_ports.begin(), this);
  } else
    w.m_ports.push_back(this);
}

// Only used by cloning - a special copy constructor
// Note we don't clone the m_countExpr since the count must be resolved at the
// instance's port in the assembly that we are cloning/externalizing.
Port::
Port(const Port &other, Worker &w, std::string &name, size_t count, const char *&err)
  : m_clone(true), m_worker(&w), m_name(name), m_ordinal(w.m_ports.size()), count(count),
    master(other.master), m_xml(other.m_xml), type(other.type), pattern(NULL),
    clock(NULL), clockPort(NULL), myClock(false), m_specXml(other.m_specXml)
{
  err = NULL; // this is the base class for everything
  w.m_ports.push_back(this);
}

Port &Port::
clone(Worker &, std::string &, size_t, OCPI::Util::Assembly::Role *, const char *&) const {
  throw OU::Error("Port type: %s cannot be cloned", typeName());
}

Port::
~Port() {
}

// Second pass parsing when ports might refer to one another.
const char *Port::
parse() {
  return NULL;
}

const char *Port::
resolveExpressions(OU::IdentResolver &ir) {
  return m_countExpr.length() ?
    parseExprNumber(m_countExpr.c_str(), count, NULL, &ir) : NULL;
}

bool Port::
needsControlClock() const {
  return false;
}

// Default
bool Port::
masterIn() const {
  return !master;
}

static const char *wipNames[] =
  { "Unknown", "WCI", "WSI", "WMI", "WDI", "WMemI", "WTI", "CPMaster",
    "uNOC", "Metadata", "TimeService", "TimeBase", "RawProperty", 0};
const char *Port::
typeName() const { return wipNames[type]; }

const char *Port::
doPattern(int n, unsigned wn, bool in, bool master, std::string &suff, bool port) {
  const char *pat = port ? m_worker->m_portPattern : (pattern ? pattern : m_worker->m_pattern);
  const char *holdPat = pat;
  if (!pat) {
    suff = "";
    return 0;
  }
  char
    c,
    *s = (char *)malloc(strlen(name()) + strlen(pat) * 3 + 10),
    *base = s;
  while ((c = *pat++)) {
    if (c != '%')
      *s++ = c;
    else if (!*pat)
      *s++ = '%';
    else {
      bool myMaster = master;
      //      bool noOrdinal = false;
      if (*pat == '!') {
	myMaster = !master;
	pat++;
      }
      if (*pat == '*') {
	//	noOrdinal = true;
	pat++;
      }
      switch (*pat++) {
      case '%':
	*s++ = '%';
	break;
      case 'm':
	*s++ = myMaster ? 'm' : 's';
	break;
      case 'M':
	*s++ = myMaster ? 'M' : 'S';
	break;
      case '0': // zero origin ordinal-within-profile
      case '1':
	sprintf(s, "%u", wn + (pat[-1] - '0'));
	while (*s) s++;
	break;
      case 'i':
	*s++ = in ? 'i' : 'o';
	break;
      case 'I':
	*s++ = in ? 'i' : 'o';
	break;
      case 'n':
	strcpy(s, in ? "in" : "out");
	while (*s)
	  s++;
	break;
      case 'N':
	strcpy(s, in ? "In" : "Out");
	while (*s)
	  s++;
	break;
      case 's': // interface name as is
      case 'S': // capitalized interface name
	strcpy(s, name());
	if (pat[-1] == 'S')
	  *s = (char)toupper(*s);
	while (*s)
	  s++;
	// Port indices are not embedded in VHDL names since they are proper arrays
	if (count > 1 || m_countExpr.length())
	  switch (n) {
	  case -1:
	    *s++ = '%';
	    *s++ = 's'; // allow for inserting ordinals later, optionally, hence %s
	    break;
	  case -2:
	    break;
	  default:
	    sprintf(s, "%u", n);
	    while (*s)
	      s++;
	  }
	break;
      case 'W': // capitalized profile name
	strcpy(s, typeName());
	s++;
        while (*s) {
	  *s = (char)tolower(*s);
	  s++;
	}
	break;
      case 'w': // lower case profile name
	strcpy(s, typeName());
        while (*s) {
	  *s = (char)tolower(*s);
	  s++;
	}
	break;
      default:
	free(base);
	return OU::esprintf("Invalid pattern rule: %s", holdPat);
      }
    }
  }
  *s++ = 0;
  suff = base;
  free(base);
  return NULL;
}

const char *Port::
doPatterns(unsigned nWip, size_t &maxPortTypeName) {
  const char *err;
  bool mIn = masterIn();
  // ordinal == -1 means insert "%u" into the name for using later
  if ((err = doPattern(-1, nWip, true, !mIn, fullNameIn)) ||
      (err = doPattern(-1, nWip, false, !mIn, fullNameOut)) ||
      (err = doPattern(-1, nWip, true, !mIn, typeNameIn, true)) ||
      (err = doPattern(-1, nWip, false, !mIn, typeNameOut, true)))
    return err;
  if (typeNameIn.length() > maxPortTypeName)
      maxPortTypeName = typeNameIn.length();
  if (typeNameOut.length() > maxPortTypeName)
    maxPortTypeName = typeNameOut.length();
  return NULL;
}

void Port::
addMyClock() {
  clock = m_worker->addClock();
  OU::format(clock->m_name, "%s_Clk", name());
  clock->port = this;
}

// Here are the cases for "clock" and "myclock":
// 1. None: assume WciClock, and it there is not one from a WCI, make one.
// 2. Clock, but not myclock, referring to a port: I'll have what he has.
// 3. myclock but not clock: define a clock for this port to own
// Minimal error checking has already be done with early parsing.
const char *Port::
checkClock() {
  if (clock)
    return NULL;
  const char *clockName = m_xml ? ezxml_cattr(m_xml, "Clock") : NULL;
  if (clockName) {
    Port *other = m_worker->findPort(clockName, this);
    if (other)
      clockPort = other; // I'll have what she is having
    else if (!(clock = m_worker->findClock(clockName)))
      return OU::esprintf("Clock for interface \"%s\", \"%s\" is not defined for the worker",
			  name(), clockName);
  } else if (myClock)
    addMyClock();
  else if (needsControlClock()) {
    if (m_worker->m_wci)
      // If no clock specified, and we have a WCI slave port then use its clock indirectly
      clockPort = m_worker->m_wci;
    else if (m_worker->m_wciClock)
      // If no clock specified, and no WCI slave, use a wciClock if it exists
      clock = m_worker->m_wciClock;
    else
      clock = m_worker->addWciClockReset();
  }
  return NULL;
}

void Port::
emitPortDescription(FILE *f, Language lang) const {
  const char *comment = hdlComment(lang);
  std::string nbuf;
  if (count > 1 || m_countExpr.length())
    if (m_countExpr.length())
      nbuf = m_countExpr;
    else
      OU::format(nbuf, " %zu", count);
  fprintf(f,
	  "\n  %s The%s %s interface%s named \"%s\", with \"%s\" acting as %s%s:\n",
	  comment, nbuf.c_str(), typeName(),
	  nbuf.length() ? "s" : "", name(), m_worker->m_implName,
	  isOCP() ? "OCP " : "", masterIn() ? "slave" : "master");
  if (clockPort)
    fprintf(f, "  %s   Clock: uses the clock from interface named \"%s\"\n", comment,
	    clockPort->name());
  else if (myClock)
    fprintf(f, "  %s   Clock: this interface has its own clock, named \"%s\"\n", comment,
	    clock->signal());
  else if (clock)
    fprintf(f, "  %s   Clock: this interface uses the worker's clock named \"%s\"\n", comment,
	    clock->signal());
}

const char *Port::
finalizeRccDataPort() {
  return NULL;
}
const char *Port::
finalizeOclDataPort() {
  return NULL;
}
const char *Port::
finalizeHdlDataPort() {
  return NULL;
}

const char *Port::
deriveOCP() {
  return "Unexpected deriveOCP on non-OCP port";
}

void Port::
emitVhdlShell(FILE *, Port*) {
}

#if 0
const char *Port::
adjustConnection(Port &/*consumer*/, const char *, Language /*lang*/,
		 OcpAdapt */*prodAdapt*/, OcpAdapt */*consAdapt*/) {
  return "invalid data port type for connection";
}
#endif

void Port::
emitImplAliases(FILE */*f*/, unsigned /*n*/, Language /*lang*/) {
}
void Port::emitImplSignals(FILE */*f*/) {
}

void Port::
emitSkelSignals(FILE */*f*/) {
}

void Port::
emitRecordTypes(FILE *f) {
  fprintf(f,"\n"
	  "  -- The following record(s) are for the inner/worker interfaces for port \"%s\"\n",
	  name());
  emitRecordDataTypes(f);
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  // Introduce record for data port, with common signals
  fprintf(f,
	  "  type worker_%s_t is record\n", 
	  in.c_str());
  if ((clock != m_worker->m_wciClock && type != WTIPort) || this == m_worker->m_wci)
    fprintf(f,
	    "    clk              : std_logic;        -- %s\n",
	    type == WCIPort ? "control clock for this worker" :
	    " this port has a clk different from the control clock\n");
  if (type != WTIPort)
    fprintf(f,
	    "    reset            : Bool_t;           -- this port is being reset from the outside peer\n");
  emitRecordInputs(f);
  fprintf(f,
	  "  end record worker_%s_t;\n", in.c_str());
  if (haveWorkerOutputs()) {
    fprintf(f,
	    "  type worker_%s_t is record\n", out.c_str());
    emitRecordOutputs(f);
    fprintf(f,
	    "  end record worker_%s_t;\n", out.c_str());
  }
}

void Port::
emitRecordDataTypes(FILE *) {}
void Port::
emitRecordInputs(FILE *) {}
void Port::
emitRecordOutputs(FILE *) {}
void Port::
emitRecordInterface(FILE */*f*/, const char */*implName*/) {}
void Port::
emitRecordInterfaceConstants(FILE */*f*/) {}

void Port::
emitRecordArray(FILE *f) {
  if (count > 1 || m_countExpr.length()) {
    std::string scount;
    if (m_countExpr.length())
      OU::format(scount, "ocpi_port_%s_count", name());
    else
      OU::format(scount, "%zu", count);
    if (haveInputs()) {
      std::string in;
      OU::format(in, typeNameIn.c_str(), "");
      fprintf(f,
	      "  type %s_array_t is array(0 to %s-1) of %s_t;\n",
	      in.c_str(), scount.c_str(), in.c_str());
    }
    if (haveOutputs()) {
      std::string out;
      OU::format(out, typeNameOut.c_str(), "");
      fprintf(f,
	      "  type %s_array_t is array(0 to %s-1) of %s_t;\n",
	      out.c_str(), scount.c_str(), out.c_str());
    }
  }
}

void Port::
emitRecordSignal(FILE *f, std::string &last, const char *prefix, bool inWorker,
		 const char */*defaultIn*/, const char */*defaultOut*/) {
  if (inWorker ? haveWorkerInputs() : haveInputs()) {
    if (last.size())
      fprintf(f, last.c_str(), ";\n");
    std::string in;
    OU::format(in, typeNameIn.c_str(), "");
    OU::format(last,
	       "  %-*s : in  %s%s%s_t%%s",
	       (int)m_worker->m_maxPortTypeName, in.c_str(), prefix, in.c_str(),
	       count > 1 || m_countExpr.length() ? "_array" : "");
  }
  if (inWorker ? haveWorkerOutputs() : haveOutputs()) {
    if (last.size())
      fprintf(f, last.c_str(), ";\n");
    std::string out;
    OU::format(out, typeNameOut.c_str(), "");
    OU::format(last,
	       "  %-*s : out %s%s%s_t%%s",
	       (int)m_worker->m_maxPortTypeName, out.c_str(), prefix, out.c_str(),
	       count > 1 || m_countExpr.length() ? "_array" : "");
  }
}

// Default is to just use record signals for VHDL
void Port::
emitSignals(FILE *f, Language lang, std::string &last, bool inPackage, bool inWorker,
	    bool /*convert*/) {
  if (lang == VHDL) {
    std::string prefix;
    if (!inPackage)
      OU::format(prefix, "work.%s_defs.", m_worker->m_implName);
    emitRecordSignal(f, last, prefix.c_str(), inWorker);
  }	  
}

void Port::
emitVerilogSignals(FILE */*f*/) {}

void Port::
emitVerilogPortParameters(FILE */*f*/) {}

void Port::
emitVHDLShellPortMap(FILE *f, std::string &last) {
  if (haveWorkerInputs()) {
    std::string in;
    OU::format(in, typeNameIn.c_str(), "");
    fprintf(f,
	    "%s    %s => %s",
	    last.c_str(),
	    in.c_str(), in.c_str());
  }
  if (haveWorkerOutputs()) {
    std::string out;
    OU::format(out, typeNameOut.c_str(), "");
    fprintf(f,
	    "%s    %s => %s",
	    last.c_str(),
	    out.c_str(), out.c_str());
  }
}

void Port::
emitVHDLSignalWrapperPortMap(FILE *f, std::string &last) {
  emitVHDLShellPortMap(f, last);
}

void Port::
emitVHDLRecordWrapperSignals(FILE */*f*/) {
}
void Port::
emitVHDLRecordWrapperAssignments(FILE */*f*/) {
}

void Port::
emitVHDLRecordWrapperPortMap(FILE */*f*/, std::string &/*last*/) {
  ocpiAssert("Non-application port types can't be in VHDL assy wrappers" == 0);
}

void Port::
emitConnectionSignal(FILE */*f*/, bool /*output*/, Language /*lang*/, std::string &/*signal*/) {
}

void Port::
emitPortSignals(FILE *f, Attachments &atts, Language /*lang*/, const char *indent,
		bool &any, std::string &comment, std::string &last, const char *myComment,
		OcpAdapt */*adapt*/) {
  doPrev(f, last, comment, myComment);
  std::string in, out, index;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  Attachment *at = atts.front();
  const char *mName, *sName;
  if (at) {
    Connection &c = at->m_connection;
    // We need to know the indexing of the other attachment
    Attachment *otherAt = NULL;
    for (AttachmentsIter ai = c.m_attachments.begin(); ai != c.m_attachments.end(); ai++)
      if (*ai != at) {
	otherAt = *ai;
	break;
      }
    assert(otherAt);
    // Indexing is necessary only when we are smaller than the other
    if (count < otherAt->m_instPort.m_port->count)
      if (c.m_count > 1)
	OU::format(index, "(%zu to %zu)", otherAt->m_index, otherAt->m_index + c.m_count - 1);
      else
	OU::format(index, "(%zu)", otherAt->m_index);
    mName = c.m_masterName.c_str();
    sName = c.m_slaveName.c_str();
  } else
    mName = sName = "open";
  // input, then output
  if (haveInputs()) {
    fprintf(f, "%s%s => %s%s",
	    any ? indent : "", in.c_str(),
	    master ?
	    (at ? sName : slaveMissing()) : (at ? mName : masterMissing()),
	    index.c_str());
    any = true;
  }
  if (haveOutputs())
    fprintf(f, "%s%s%s => %s%s",
	    haveInputs() ? ",\n" : "", any ? indent : "", out.c_str(),
	    master ? mName : sName, index.c_str());
}

void Port::
emitXML(FILE *) {}

const char *Port::
emitRccCppImpl(FILE *) {
  return NULL;
}
void Port::
emitRccCImpl(FILE *) {}
void Port::
emitRccCImpl1(FILE *) {}
void Port::
emitRccArgTypes(FILE *, bool &) {}

RawPropPort::
RawPropPort(Worker &w, ezxml_t x, Port *sp, int ordinal, const char *&err)
  : Port(w, x, sp, ordinal, PropPort, "rawProps", err) {
}

// Our special copy constructor
RawPropPort::
RawPropPort(const RawPropPort &other, Worker &w , std::string &name, size_t count,
	    const char *&err)
  : Port(other, w, name, count, err) {
  if (err)
    return;
}

Port &RawPropPort::
clone(Worker &w, std::string &name, size_t count, OCPI::Util::Assembly::Role *, const char *&err)
  const {
  return *new RawPropPort(*this, w, name, count, err);
}

void RawPropPort::
emitRecordTypes(FILE *f) {
  fprintf(f,
	  "\n"
	  "  -- Record for the RawProp input signals for port \"%s\" of worker \"%s\"\n"
	  "  alias worker_%s_in_t is wci.raw_prop_%s_t;\n"
	  "  -- Record for the RawProp  output signals for port \"%s\" of worker \"%s\"\n"
	  "  alias worker_%s_out_t is wci.raw_prop_%s_t;\n",
	  name(), m_worker->m_implName, name(), master ? "in" : "out",
	  name(), m_worker->m_implName, name(), master ? "out" : "in");
}

void RawPropPort::
emitRecordInterface(FILE *f, const char *implName) {
  std::string scount = m_countExpr;
  if (scount.empty())
    OU::format(scount, "%zu", count);
  else
    OU::format(scount, "to_integer(%s)", m_countExpr.c_str());
  fprintf(f, "  constant ocpi_port_%s_count : natural := %s;\n", name(), scount.c_str());
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  fprintf(f,
	  "\n"
	  "  -- Record for the %s input signals for port \"%s\" of worker \"%s\"\n"
	  "  alias %s_t is wci.raw_prop_%s_t;\n"
	  "  -- Record for the %s output signals for port \"%s\" of worker \"%s\"\n"
	  "  alias %s_t is wci.raw_prop_%s_t;\n",
	  typeName(), name(), implName,
	  in.c_str(), master ? "in" : "out",
	  typeName(), name(), implName,
	  out.c_str(), master ? "out" : "in");
  if (count > 1 || m_countExpr.length())
      fprintf(f,
	      "  subtype %s_array_t is wci.raw_prop_%s_array_t(0 to ocpi_port_%s_count-1);\n"
	      "  subtype %s_array_t is wci.raw_prop_%s_array_t(0 to ocpi_port_%s_count-1);\n",
	      in.c_str(), master ? "in" : "out", name(),
	      out.c_str(), master ? "out" : "in", name());
}

void RawPropPort::
emitConnectionSignal(FILE *f, bool output, Language /*lang*/, std::string &signal) {
  fprintf(f, "  signal %s : wci.raw_prop_%s%s_t",
	  signal.c_str(), master == output ? "out" : "in",
	  count > 1 || m_countExpr.length() ? "_array" : "");
  if (count > 1 || m_countExpr.length())
    fprintf(f, "(0 to %s.%s_defs.ocpi_port_%s_count-1)", m_worker->m_implName, m_worker->m_implName, name());
  fprintf(f, ";\n");
}

const char *RawPropPort::
masterMissing() const {
  return "wci.raw_prop_out_zero";
}
const char *RawPropPort::
slaveMissing() const {
  return "wci.raw_prop_in_zero";
}




CpPort::
CpPort(Worker &w, ezxml_t x, Port *sp, int ordinal, const char *&err)
  : Port(w, x, sp, ordinal, CPPort, "cp", err) {
}

// Our special copy constructor
CpPort::
CpPort(const CpPort &other, Worker &w , std::string &name, size_t count, const char *&err)
  : Port(other, w, name, count, err) {
  if (err)
    return;
}

// Virtual constructor: the concrete instantiated classes must have a clone method,
// which calls the corresponding specialized copy constructor
Port &CpPort::
clone(Worker &w, std::string &name, size_t count, OCPI::Util::Assembly::Role */*role*/,
      const char *&err) const {
  return *new CpPort(*this, w, name, count, err);
}

void CpPort::
emitRecordTypes(FILE *f) {
  fprintf(f,
	  "\n"
	  "  -- Record for the CPMaster input signals for port \"%s\" of worker \"%s\"\n"
	  "  alias worker_%s_in_t is platform.platform_pkg.occp_out_t;\n"
	  "  -- Record for the CPMaster  output signals for port \"%s\" of worker \"%s\"\n"
	  "  alias worker_%s_out_t is platform.platform_pkg.occp_in_t;\n",
	  name(), m_worker->m_implName, name(), name(), m_worker->m_implName, name());
}
void CpPort::
emitRecordInterface(FILE *f, const char *implName) {
  fprintf(f,
	  "\n"
	  "  -- Record for the %s input signals for port \"%s\" of worker \"%s\"\n"
	  "  alias %s_t is platform.platform_pkg.occp_out_t;\n"
	  "  -- Record for the %s output signals for port \"%s\" of worker \"%s\"\n"
	  "  alias %s_t is platform.platform_pkg.occp_in_t;\n",
	  typeName(), name(), implName, typeNameIn.c_str(),
	  typeName(), name(), implName, typeNameOut.c_str());
}

void CpPort::
emitConnectionSignal(FILE *f, bool output, Language /*lang*/, std::string &signal) {
  fprintf(f, "  signal %s : platform.platform_pkg.occp_%s_t;\n",
	  signal.c_str(), master == output ? "in" : "out");
}

NocPort::
NocPort(Worker &w, ezxml_t x, Port *sp, int ordinal, const char *&err)
  : Port(w, x, sp, ordinal, NOCPort, "unoc", err) {
}

// Our special copy constructor
NocPort::
NocPort(const NocPort &other, Worker &w , std::string &name, size_t count,
		const char *&err)
  : Port(other, w, name, count, err) {
  if (err)
    return;
}

// Virtual constructor: the concrete instantiated classes must have a clone method,
// which calls the corresponding specialized copy constructor
Port &NocPort::
clone(Worker &w, std::string &name, size_t count, OCPI::Util::Assembly::Role */*role*/,
      const char *&err) const {
  return *new NocPort(*this, w, name, count, err);
}

void NocPort::
emitRecordInterface(FILE *f, const char *implName) {
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  fprintf(f,
	  "\n"
	  "  -- Record for the %s input signals for port \"%s\" of worker \"%s\"\n"
	  "  alias %s_t is platform.platform_pkg.unoc_master_%s_t;\n"
	  "  -- Record for the %s output signals for port \"%s\" of worker \"%s\"\n"
	  "  alias %s_t is platform.platform_pkg.unoc_master_%s_t;\n",
	  typeName(), name(), implName, in.c_str(), master ? "in" : "out",
	  typeName(), name(), implName, out.c_str(), master ? "out" : "in");
}

void NocPort::
emitRecordTypes(FILE */*f*/) {
}

void NocPort::
emitConnectionSignal(FILE *f, bool output, Language /*lang*/, std::string &signal) {
  fprintf(f, "  signal %s : platform.platform_pkg.unoc_master_%s_t;\n",
	  signal.c_str(), master == output ? "out" : "in" );
}

TimeServicePort::
TimeServicePort(Worker &w, ezxml_t x, Port *sp, int ordinal, const char *&err)
  : Port(w, x, sp, ordinal, TimePort, "time", err) {
}
// Our special copy constructor
TimeServicePort::
TimeServicePort(const TimeServicePort &other, Worker &w , std::string &name, size_t count,
		const char *&err)
  : Port(other, w, name, count, err) {
}

// Virtual constructor: the concrete instantiated classes must have a clone method,
// which calls the corresponding specialized copy constructor
Port &TimeServicePort::
clone(Worker &w, std::string &name, size_t count, OCPI::Util::Assembly::Role */*role*/,
      const char *&err) const {
  return *new TimeServicePort(*this, w, name, count, err);
}

void TimeServicePort::
emitRecordTypes(FILE *f) {
  fprintf(f,
	  "\n"
	  "  -- Record for the TimeService output signals for port \"%s\" of worker \"%s\"\n"
	  "  alias worker_%s_out_t is platform.platform_pkg.time_service_t;\n",
	  name(), m_worker->m_implName, name());
}

void TimeServicePort::
emitRecordSignal(FILE *f, std::string &last, const char *prefix, bool /*inWorker*/,
		 const char *, const char *) {
  if (last.size())
    fprintf(f, last.c_str(), ";");
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  OU::format(last,
	     "  %-*s : %s  %s%s_t%%s",
	     (int)m_worker->m_maxPortTypeName, 
	     master ? out.c_str() : in.c_str(),
	     master ? "out" : "in", prefix,
	     master ? out.c_str() : in.c_str());
}

void TimeServicePort::
emitRecordInterface(FILE *f, const char *implName) {
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  fprintf(f,
	  "\n"
	  "  -- Record for the %s input signals for port \"%s\" of worker \"%s\"\n"
	  "  alias %s_t is platform.platform_pkg.time_service_t;\n",
	  typeName(), name(), implName, master ? out.c_str() : in.c_str());
}

void TimeServicePort::
emitVHDLShellPortMap(FILE *f, std::string &last) {
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  fprintf(f,
	  "%s    %s => %s",
	  last.c_str(),
	  master ? out.c_str() : in.c_str(),
	  master ? out.c_str() : in.c_str());
}

void TimeServicePort::
emitVHDLSignalWrapperPortMap(FILE *f, std::string &last) {
  emitVHDLShellPortMap(f, last);
}

void TimeServicePort::
emitPortSignals(FILE *f, Attachments &atts, Language /*lang*/, const char *indent,
		bool &any, std::string &comment, std::string &last, const char *myComment,
		OcpAdapt */*adapt*/) {
  doPrev(f, last, comment, myComment);
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  // Only one direction - master outputs to slave
  fprintf(f, "%s%s => ",
	  any ? indent : "",
	  master ? out.c_str() : in.c_str());
  //	fputs(p.master ? c.m_masterName.c_str() : c.m_slaveName.c_str(), f);
  Attachment *at = atts.front();
  Connection *c = at ? &at->m_connection : NULL;
  fputs(at ? c->m_masterName.c_str() : "open", f);
}

void TimeServicePort::
emitConnectionSignal(FILE *f, bool /*output*/, Language /*lang*/, std::string &signal) {
  fprintf(f, "  signal %s : platform.platform_pkg.time_service_t;\n", signal.c_str());
}

TimeBasePort::
TimeBasePort(Worker &w, ezxml_t x, Port *sp, int ordinal, const char *&err)
  : Port(w, x, sp, ordinal, TimeBase, "timebase", err) {
}
// Our special copy constructor
TimeBasePort::
TimeBasePort(const TimeBasePort &other, Worker &w , std::string &name, size_t count,
		const char *&err)
  : Port(other, w, name, count, err) {
}

// Virtual constructor: the concrete instantiated classes must have a clone method,
// which calls the corresponding specialized copy constructor
Port &TimeBasePort::
clone(Worker &w, std::string &name, size_t count, OCPI::Util::Assembly::Role */*role*/,
      const char *&err) const {
  return *new TimeBasePort(*this, w, name, count, err);
}

void TimeBasePort::
emitRecordTypes(FILE *f) {
  fprintf(f,
	  "\n"
	  "  -- Record for the Timebase output signals for port \"%s\" of worker \"%s\"\n"
	  "  alias worker_%s_out_t is platform.platform_pkg.time_base_%s_t;\n"
	  "  alias worker_%s_in_t is platform.platform_pkg.time_base_%s_t;\n",
	  name(), m_worker->m_implName,
	  name(), master ? "out" : "in",
	  name(), master ? "in" : "out");
}

#if 0
void TimeBasePort::
emitRecordSignal(FILE *f, std::string &last, const char *prefix, bool /*inWorker*/,
		 const char *, const char *) {
  if (last.size())
    fprintf(f, last.c_str(), ";");
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  OU::format(last,
	     "  %-*s : %s  %s%s_t%%s",
	     (int)m_worker->m_maxPortTypeName, 
	     master ? out.c_str() : in.c_str(),
	     master ? "out" : "in", prefix,
	     master ? out.c_str() : in.c_str());
}
#endif
void TimeBasePort::
emitRecordInterface(FILE *f, const char *implName) {
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  fprintf(f,
	  "\n"
	  "  -- Records for the %s input signals for port \"%s\" of worker \"%s\"\n"
	  "  alias %s_t is platform.platform_pkg.time_base_%s_t;\n"
	  "  alias %s_t is platform.platform_pkg.time_base_%s_t;\n",
	  typeName(), name(), implName,
	  in.c_str(), master ? "in" : "out",
	  out.c_str(), master ? "out" : "in");
}

#if 0
void TimeBasePort::
emitVHDLShellPortMap(FILE *f, std::string &last) {
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  fprintf(f,
	  "%s    %s => %s",
	  last.c_str(),
	  master ? out.c_str() : in.c_str(),
	  master ? out.c_str() : in.c_str());
}
#endif
void TimeBasePort::
emitVHDLSignalWrapperPortMap(FILE *f, std::string &last) {
  emitVHDLShellPortMap(f, last);
}

#if 0
void TimeBasePort::
emitPortSignals(FILE *f, Attachments &atts, Language /*lang*/, const char *indent,
		bool &any, std::string &comment, std::string &last, const char *myComment,
		OcpAdapt */*adapt*/) {
  doPrev(f, last, comment, myComment);
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  // Only one direction - master outputs to slave
  fprintf(f, "%s%s => ",
	  any ? indent : "",
	  master ? out.c_str() : in.c_str());
  //	fputs(p.master ? c.m_masterName.c_str() : c.m_slaveName.c_str(), f);
  Attachment *at = atts.front();
  Connection *c = at ? &at->m_connection : NULL;
  fputs(at ? c->m_masterName.c_str() : "open", f);
}
#endif

void TimeBasePort::
emitConnectionSignal(FILE *f, bool output, Language /*lang*/, std::string &signal) {
  fprintf(f, "  signal %s : platform.platform_pkg.time_base_%s_t;\n", signal.c_str(), master == output ? "out" : "in");
}

MetaDataPort::
MetaDataPort(Worker &w, ezxml_t x, Port *sp, int ordinal, const char *&err)
  : Port(w, x, sp, ordinal, MetadataPort, "metadata", err) {
}

// Our special copy constructor
MetaDataPort::
MetaDataPort(const MetaDataPort &other, Worker &w , std::string &name, size_t count,
		const char *&err)
  : Port(other, w, name, count, err) {
  if (err)
    return;
}

// Virtual constructor: the concrete instantiated classes must have a clone method,
// which calls the corresponding specialized copy constructor
Port &MetaDataPort::
clone(Worker &w, std::string &name, size_t count, OCPI::Util::Assembly::Role */*role*/,
      const char *&err) const {
  return *new MetaDataPort(*this, w, name, count, err);
}

void MetaDataPort::
emitRecordTypes(FILE *f) {
  fprintf(f,
	  "\n"
	  "  -- Record for the CPMaster input signals for port \"%s\" of worker \"%s\"\n"
	  "  alias worker_%s_in_t is platform.platform_pkg.metadata_out_t;\n"
	  "  -- Record for the CPMaster  output signals for port \"%s\" of worker \"%s\"\n"
	  "  alias worker_%s_out_t is platform.platform_pkg.metadata_in_t;\n",
	  name(), m_worker->m_implName, name(), name(), m_worker->m_implName, name());
}

void MetaDataPort::
emitRecordInterface(FILE *f, const char *implName) {
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  fprintf(f,
	  "\n"
	  "  -- Record for the %s input signals for port \"%s\" of worker \"%s\"\n"
	  "  alias %s_t is platform.platform_pkg.metadata_%s_t;\n"
	  "  -- Record for the %s output signals for port \"%s\" of worker \"%s\"\n"
	  "  alias %s_t is platform.platform_pkg.metadata_%s_t;\n",
	  typeName(), name(), implName,
	  in.c_str(), master ? "in" : "out",
	  typeName(), name(), implName,
	  out.c_str(), master ? "out" : "in");
}

void MetaDataPort::
emitConnectionSignal(FILE *f, bool output, Language /*lang*/, std::string &signal) {
  fprintf(f, "  signal %s : platform.platform_pkg.metadata_%s_t;\n",
	  signal.c_str(), output && master || !output && !master ? "out" : "in");
}

const char *Port::
fixDataConnectionRole(OU::Assembly::Role &role) {
  if (role.m_knownRole)
    return OU::esprintf("Role of port %s of worker %s in connection is incompatible with a port"
			" of type \"%s\"",
			name(), m_worker->m_implName, typeName());
  role.m_provider = !master;
  role.m_bidirectional = false;
  role.m_knownRole = true;
  return NULL;
}

void Port::
initRole(OCPI::Util::Assembly::Role &) {
}

void Port::
emitExtAssignment(FILE *f, bool int2ext, const std::string &extName, const std::string &intName,
		  const Attachment &extAt, const Attachment &intAt, size_t connCount) const {
  std::string ours = extName;
  if (connCount < count) {
    if (connCount == 1)
      OU::formatAdd(ours, "(%zu)", extAt.m_index);
    else
      OU::formatAdd(ours, "(%zu to %zu)", extAt.m_index, extAt.m_index + connCount - 1);
  }
  std::string theirs = intName;
  if (connCount == 1)
    OU::formatAdd(theirs, "(%zu)", intAt.m_index);
  else
    OU::formatAdd(theirs, "(%zu to %zu)", intAt.m_index, intAt.m_index + connCount - 1);
  fprintf(f, "  %s <= %s;\n",
	  int2ext ? ours.c_str() : theirs.c_str(),
	  int2ext ? theirs.c_str() : ours.c_str());
}
