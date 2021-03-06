#include "assembly.h"
#include "hdl.h"

DevSignalsPort::
DevSignalsPort(Worker &w, ezxml_t x, Port *sp, int ordinal, const char *&err)
  : Port(w, x, sp, ordinal, DevSigPort, "dev", err),
    m_hasInputs(false), m_hasOutputs(false) {
  if ((err = Signal::parseSignals(x, w.m_file, m_signals, m_sigmap)))
    return;
  for (SignalsIter si = m_signals.begin(); si != m_signals.end(); si++) {
    Signal &s = **si;
    switch (s.m_direction) {
    case Signal::IN:
      if (master)
	m_hasInputs = true;
      else
	m_hasOutputs = true;
      break;
    case Signal::OUT:
      if (master)
	m_hasOutputs = true;
      else
	m_hasInputs = true;
      break;
    default:
      err = "Unsupported signal direction (not in or out) in devsignals port";
      return;
    }
  }
}

// Our special copy constructor
DevSignalsPort::
DevSignalsPort(const DevSignalsPort &other, Worker &w, std::string &name, size_t count,
	       const char *&err)
  : Port(other, w, name, count, err) {
  if (err)
    return;
  m_signals = other.m_signals;
  m_sigmap = other.m_sigmap;
  m_hasInputs = other.m_hasInputs;
  m_hasOutputs = other.m_hasOutputs;
}

Port &DevSignalsPort::
clone(Worker &w, std::string &name, size_t count, OCPI::Util::Assembly::Role *, const char *&err)
  const {
  return *new DevSignalsPort(*this, w, name, count, err);
}

void DevSignalsPort::
emitRecordTypes(FILE *f) {
  fprintf(f, "\n");
  if (m_hasInputs)
    fprintf(f,
	    "  -- Record for DevSignals input signals for port \"%s\" of worker \"%s\"\n"
	    "  alias worker_%s_in_t is work.%s_defs.%s_in_t;\n",
	    name(), m_worker->m_implName, name(),
	    m_worker->m_implName, name());
  if (m_hasOutputs)
    fprintf(f,
	    "  -- Record for DevSignals output signals for port \"%s\" of worker \"%s\"\n"
	    "  alias worker_%s_out_t is work.%s_defs.%s_out_t;\n",
	    name(), m_worker->m_implName, name(),
	    m_worker->m_implName, name());
}

void DevSignalsPort::
emitRecordInterface(FILE *f, const char *implName) {
  std::string in, out;
  OU::format(in, typeNameIn.c_str(), "");
  OU::format(out, typeNameOut.c_str(), "");
  // Define input record
  if (m_hasInputs) {
    fprintf(f,
	    "\n"
	    "  -- Record for the %s input signals for port \"%s\" of worker \"%s\"\n"
	    "  type %s_t is record\n",
	    master ? "master" : "slave", name(), implName, in.c_str());
    std::string last;
    for (SignalsIter si = m_signals.begin(); si != m_signals.end(); si++) {
      Signal &s = **si;
      if (s.m_direction == (master ? Signal::IN : Signal::OUT))
	emitSignal(s.m_name.c_str(), f, VHDL, Signal::NONE, last,
		   s.m_width ? (int)s.m_width : -1, 0, "", s.m_type);
    }
    emitLastSignal(f, last, VHDL, false);
    fprintf(f,
	    "  end record %s_t;\n", in.c_str());
  }
  if (m_hasOutputs) {
    fprintf(f,
	    "\n"
	    "  -- Record for the %s output signals for port \"%s\" of worker \"%s\"\n"
	    "  type %s_t is record\n",
	    master ? "master" : "slave", name(), implName, out.c_str());
    std::string last;
    for (SignalsIter si = m_signals.begin(); si != m_signals.end(); si++) {
      Signal &s = **si;
      if (s.m_direction == (master ? Signal::OUT : Signal::IN))
	emitSignal(s.m_name.c_str(), f, VHDL, Signal::NONE, last,
		   s.m_width ? (int)s.m_width : -1, 0, "", s.m_type);
    }
    emitLastSignal(f, last, VHDL, false);
    fprintf(f,
	    "  end record %s_t;\n", out.c_str());
  }
  emitRecordArray(f);
}

void DevSignalsPort::
emitConnectionSignal(FILE *f, bool output, Language /*lang*/, std::string &signal) {
  if (output && !haveOutputs() ||
      !output && !haveInputs())
    return;
  std::string tname;
  OU::format(tname, output ? typeNameOut.c_str() : typeNameIn.c_str(), "");
  std::string stype;
  OU::format(stype, "%s.%s_defs.%s%s_t", m_worker->m_library, m_worker->m_implName,
	     tname.c_str(),
	     count > 1 ? "_array" : "");
  //  if (count > 1)
  //    OU::formatAdd(stype, "(0 to %zu)", count - 1);
  fprintf(f,
	  "  signal %s : %s;\n", signal.c_str(), stype.c_str());
}

// Emit for one direction: note the connection is optional
void DevSignalsPort::
emitPortSignalsDir(FILE *f, bool output, const char *indent, bool &any, std::string &comment,
		   std::string &last, Attachment *other) {
  std::string port;
  OU::format(port, output ? typeNameOut.c_str() : typeNameIn.c_str(), "");
  std::string conn;
  if (other)
    conn = master != output ?
      other->m_connection.m_slaveName.c_str() : other->m_connection.m_masterName.c_str();

  if (!master)
    output = !output; // signals that are included are not relevant
  for (SignalsIter si = m_signals.begin(); si != m_signals.end(); si++)
    if ((*si)->m_direction == Signal::IN && !output ||
	(*si)->m_direction == Signal::OUT && output)
      for (size_t n = 0; n < count; n++) {
	std::string myindex;
	if (count > 1)
	  OU::format(myindex, "(%zu)", n);
	std::string otherName = conn;
	if (other) {
	  if (other && other->m_instPort.m_port->count > count || count > 1)
	    OU::formatAdd(otherName, "(%zu)", n + other->m_index);
	  OU::formatAdd(otherName, ".%s", (*si)->m_name.c_str());
	} else
	  otherName =
	    master == ((*si)->m_direction == Signal::IN) ?
	    ((*si)->m_width ? "(others => '0')" : "'0'") : "open";
	doPrev(f, last, comment, hdlComment(VHDL));
	fprintf(f, "%s%s%s.%s => %s",
		any ? indent : "", port.c_str(), myindex.c_str(), (*si)->m_name.c_str(),
		otherName.c_str());
	any = true;
      }
}
// These signal bundles are defined on both sides independently, so they must
// be assigned individually...
void DevSignalsPort::
emitPortSignals(FILE *f, Attachments &atts, Language /*lang*/, const char *indent,
		bool &any, std::string &comment, std::string &last, const char */*myComment*/,
		OcpAdapt */*adapt*/) {
  Attachment *at = atts.front();
  Attachment *otherAt = NULL;
  if (at) {
    Connection &c = at->m_connection;
    // We need to know the indexing of the other attachment
    for (AttachmentsIter ai = c.m_attachments.begin(); ai != c.m_attachments.end(); ai++)
      if (*ai != at) {
	otherAt = *ai;
	break;
      }
    assert(otherAt);
  }
  if (haveInputs())
    emitPortSignalsDir(f, false, indent, any, comment, last, otherAt);
  if (haveOutputs())
    emitPortSignalsDir(f, true, indent, any, comment, last, otherAt);
}

void DevSignalsPort::
emitExtAssignment(FILE *f, bool int2ext, const std::string &extName, const std::string &intName,
		  const Attachment &extAt, const Attachment &intAt, size_t connCount) const {
  // We can't assume record type compatibility so we must assign individual signals.
  for (size_t n = 0; n < connCount; n++) {
    std::string ours = extName;
    if (count > 1)
      OU::formatAdd(ours, "(%zu)", extAt.m_index + n);
    std::string theirs = intName;
    OU::formatAdd(theirs, "(%zu)", intAt.m_index + n);
    for (SignalsIter si = m_signals.begin(); si != m_signals.end(); si++)
      fprintf(f, "  %s.%s <= %s.%s;\n",
	      int2ext ? ours.c_str() : theirs.c_str(), (*si)->cname(),
	      int2ext ? theirs.c_str() : ours.c_str(), (*si)->cname());
  }
}
