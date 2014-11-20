#include "wip.h"
#include "hdl.h"
#include "hdl-device.h"
#include "hdl-slot.h"
#include "assembly.h"

DeviceTypes DeviceType::s_types;

Worker *HdlDevice::
create(ezxml_t xml, const char *xfile, Worker *parent, const char *&err) {
  HdlDevice *hd = new HdlDevice(xml, xfile, "", parent, err);
  if (err) {
    delete hd;
    hd = NULL;
  }
  return hd;
}
HdlDevice::
HdlDevice(ezxml_t xml, const char *file, const char *parentFile, Worker *parent,
	  const char *&err)
  : Worker(xml, file, parentFile, Worker::Device, parent, NULL, err) {
  m_isDevice = true;
  if (err ||
      (err = OE::checkTag(xml, "HdlDevice", "Expected 'HdlDevice' as tag in '%s'", file)) ||
      (err = OE::checkAttrs(xml, PARSED_ATTRS, IMPL_ATTRS, HDL_TOP_ATTRS, HDL_IMPL_ATTRS,
			    "interconnect", "control", (void*)0)) ||
      (err = OE::checkElements(xml, IMPL_ELEMS, HDL_IMPL_ELEMS, (void*)0)) ||
      (err = OE::getBoolean(xml, "interconnect", &m_interconnect)) ||
      (err = OE::getBoolean(xml, "control", &m_canControl)))
    return;
  if ((err = parseHdl()))
    return;
  // Parse submodule requirements - note that this information is only used
  // for platform configurations and containers, but we do a bit of error checking here
  for (ezxml_t rqx = ezxml_cchild(m_xml, "requires"); rqx; rqx = ezxml_next(rqx)) {
    std::string worker;
    if ((err = OE::checkAttrs(rqx, "worker", NULL)) ||
	(err = OE::checkElements(rqx, "connect", NULL)) ||
	(err = OE::getRequiredString(rqx, worker, "worker")))
      return;
    std::string rqFile;
    OU::format(rqFile, "../%s.hdl/%s.xml", worker.c_str(),  worker.c_str());
    DeviceType *dt = DeviceType::get(rqFile.c_str(), m_file.c_str(), err);
    if (!dt) {
      err = OU::esprintf("for required worker %s: %s", worker.c_str(), err);
      return;
    }
    m_requireds.push_back(Required(*dt));
    m_requireds.back().parse(rqx, *this);
    for (ezxml_t cx = ezxml_cchild(rqx, "connect"); cx; cx = ezxml_next(cx)) {
      std::string port, to;
      size_t index;
      bool idxFound = false;
      if ((err = OE::checkAttrs(cx, "port", "to", "index", NULL)) ||
	  (err = OE::checkElements(cx, NULL)) ||
	  (err = OE::getRequiredString(cx, port, "port")) ||
	  (err = OE::getRequiredString(cx, to, "to")) ||
	  (err = OE::getNumber(cx, "index", &index, &idxFound)))
	return ;
    }
  }
}

HdlDevice *HdlDevice::
get(const char *name, const char *parent, const char *&err) {
  DeviceType *dt = NULL;
  for (DeviceTypesIter dti = s_types.begin(); !dt && dti != s_types.end(); dti++)
    if (!strcasecmp(name, (*dti)->name()))
      dt = *dti;
  if (!dt) {
    // New device type, which must be a file.
    ezxml_t xml;
    std::string xfile;
    if (!(err = parseFile(name, parent, NULL, &xml, xfile))) {
      dt = new DeviceType(xml, xfile.c_str(), parent, NULL, err);
      if (err) {
	delete dt;
	dt = NULL;
      } else
	s_types.push_back(dt);
    }
  }
  return dt;
}
const char *DeviceType::
name() const {
  return m_implName;
}

static const char*
decodeSignal(std::string &name, std::string &base, size_t &index, bool &hasIndex) {
  const char *sname = name.c_str();
  const char *paren = strchr(sname, '(');
  base = name;
  if (paren) {
    char *end;
    errno = 0;
    index = strtoul(paren + 1, &end, 0);
    if (errno != 0 || end == paren + 1 || *end != ')')
      return OU::esprintf("Bad numeric format in signal index: %s", sname);
    base.resize(paren - sname);
    hasIndex = true;
  } else {
    hasIndex = false;
    index = 0;
  }
  return NULL;
}


// A device is not in its own file.
// It is an instance of a device type either on a board
Device::
Device(Board &b, DeviceType &dt, ezxml_t xml, bool single, unsigned ordinal,
       SlotType *stype, const char *&err)
  : m_board(b), m_deviceType(dt), m_ordinal(ordinal) {
  err = NULL;
  std::string wname;
  if ((err = OE::getRequiredString(xml, wname, "worker")))
    return;
  const char *cp = strchr(wname.c_str(), '.');
  if (cp)
    wname.resize(cp - wname.c_str());
  if (single)
    m_name = wname;
  else {
    wname += "%u";
    OE::getNameWithDefault(xml, m_name, wname.c_str(), ordinal);
  }
  // Now we parse the mapping between device-type signals and board signals
  for (ezxml_t xs = ezxml_cchild(xml, "Signal"); !err && xs; xs = ezxml_next(xs)) {
    std::string name, base;
    size_t index;
    bool hasIndex;
    if ((err = OE::getRequiredString(xs, name, "name")) ||
	(err = decodeSignal(name, base, index, hasIndex)))
      return;
    Signal *devSig = m_deviceType.m_sigmap.findSignal(base);
    if (!devSig) {
      err = OU::esprintf("Signal \"%s\" is not defined for device type \"%s\"",
			 base.c_str(), m_deviceType.name());
      return;
    }
    if (hasIndex) {
      if (devSig->m_width == 0) {
	err = OU::esprintf("Device signal \"%s\" cannot be indexed", base.c_str());
	return;
      } else if (index >= devSig->m_width) {
	err = OU::esprintf("Device signal \"%s\" has index higher than signal width (%zu)",
			   name.c_str(), devSig->m_width);
	return;
      }
    }
    const char *plat = ezxml_cattr(xs, "platform");
    const char *slot = ezxml_cattr(xs, "slot");
    const char *board;
    Signal *boardSig = NULL; // the signal we are mapping a device signal TO
    if (stype) {
      board = slot;
      if (plat)
        err = OU::esprintf("The platform attribute cannot be specified for signals on cards");
      else if (!slot)
        err = OU::esprintf("For signal \"%s\" for a device on a card, "
			   "the slot attribute must be specified", name.c_str());
      else if (*slot && !(boardSig = stype->m_sigmap.findSignal(slot)))
	// The slot signal is not a signal for the slot type of this card
	err = OU::esprintf("For signal \"%s\", the slot signal \"%s\" is not defined "
			   "for slot type \"%s\"", name.c_str(), slot, stype->name());
    } else {
      if (slot)
	err = OU::esprintf("For signal \"%s\" for a platform device, "
			   "the slot attribute cannot be specified", name.c_str());
      else if (!plat)
	err = OU::esprintf("For signal \"%s\", the platform attribute must be specified",
			   name.c_str());
      else if (*plat) {
	board = plat;
	if (!(boardSig = b.m_extmap.findSignal(board))) {
	  boardSig = new Signal();
	  boardSig->m_name = board;
	  boardSig->m_direction = devSig->m_direction;
	  b.m_extmap[board] = boardSig;
	  b.m_extsignals.push_back(boardSig);
	}      
      }
    }
    if (err)
      return;
    // Check compatibility between device and slot signal
    if (boardSig) {
      switch (boardSig->m_direction) {
      case Signal::IN: // input to board
	if (devSig->m_direction != Signal::IN)
	  err = OU::esprintf("Board signal \"%s\" is input to board, "
			     "but \"%s\" is not input to device", boardSig->name(),
			     devSig->name());
	break;
      case Signal::OUT: // output from board
	if (devSig->m_direction != Signal::OUT)
	  err = OU::esprintf("Slot signal \"%s\" is output from board, "
			     "but \"%s\" is not output from device", boardSig->name(),
			     devSig->name());
	break;
      case Signal::INOUT: // FIXME: is this really allowed for slots?
	if (devSig->m_direction != Signal::INOUT)
	  err = OU::esprintf("Slot signal \"%s\" is inout to/from board, "
			     "but \"%s\" is not inout at device", boardSig->name(),
			     devSig->name());
	break;
      case Signal::BIDIRECTIONAL:
	if (devSig->m_direction == Signal::INOUT)
	  err = OU::esprintf("Slot signal \"%s\" is bidirectional to or from board, "
			     "but \"%s\" is inout at device", boardSig->name(),
			     devSig->name());
	break;
      default:
	;
      }
      if (err)
	return;
      // Here board and boardSig are set
      Signal *other = b.m_bd2dev.findSignal(board);
      if (other) {
	// Multiple device signals to the same board signal.  The other signal was ok by itself.
	switch (other->m_direction) {
	case Signal::IN:
	  if (devSig->m_direction != Signal::IN) {
	    err = OU::esprintf("Multiple, incompatible device signals assigned to \"%s\" signal",
			       board);
	    return;
	  }
	case Signal::OUT:
	  err = OU::esprintf("Multiple device output signals driving \"%s\" signal ", board);
	  return;
	default:
	  ;
	}
      } else {
	// First time we have seen this board signal
	// Map to find device signal and index from slot/board signal
	b.m_bd2dev.insert(board, devSig, index);
      }
    }
    // boardSig might be NULL here.
    std::string devSigIndexed = devSig->name();
    if (devSig->m_width)
      OU::formatAdd(devSigIndexed, "(%zu)", index);
    m_strings.push_front(devSigIndexed);
    m_sigmap[m_strings.front().c_str()] = boardSig;
  }
}

Device *Device::
create(Board &b, ezxml_t xml, const char *parent, bool single, unsigned ordinal,
       SlotType *stype, const char *&err) {
  std::string wname;
  DeviceType *dt;
  if ((err = OE::getRequiredString(xml, wname, "worker")) ||
      !(dt = DeviceType::get(wname.c_str(), parent, err)))
    return NULL;
  Device *d = new Device(b, *dt, xml, single, ordinal, stype, err);
  if (err) {
    delete d;
    d = NULL;
  }
  return d;
}


Board::
Board(SigMap &sigmap, Signals &signals)
  : m_extmap(sigmap), m_extsignals(signals) {
}

// Add all the devices for a platform or a card - static
const char *Board::
parseDevices(ezxml_t xml, SlotType *stype) {
  // These devices are declaring that they are part of the board.
  for (ezxml_t xs = ezxml_cchild(xml, "Device"); xs; xs = ezxml_next(xs)) {
    const char *worker = ezxml_cattr(xs, "worker");
    bool single = true;
    bool seenMe = false;
    unsigned n = 0; // ordinal of devices of same type in this platform/card
    for (ezxml_t x = ezxml_cchild(xml, "Device"); x; x = ezxml_next(x)) {
      const char *w = ezxml_cattr(x, "worker");
      if (x == xs)
	seenMe = true;
      else if (worker && w && !strcasecmp(worker, w)) {
	single = false;
	if (!seenMe)
	  n++;
      }
    }
    const char *err;
    Device *dev = Device::create(*this, xs, xml->name, single, n, stype, err);
    if (dev)
      m_devices.push_back(dev);
    else
      return err;
  }
  return NULL;
}
// Static
const Device *Device::
find(const char *name, const Devices &devices) {
  for (DevicesIter di = devices.begin(); di != devices.end(); di++) {
    Device &dev = **di;
    if (!strcasecmp(name, dev.m_name.c_str()))
      return &dev;
  }
  return NULL;
}

const Device &Device::
findRequired(const DeviceType &dt, unsigned ordinal, const Devices &devices) {
  for (DevicesIter di = devices.begin(); di != devices.end(); di++) {
    Device &dev = **di;
    // FIXME: intern the workers
    if (!strcasecmp(dev.m_deviceType.m_implName, dt.m_implName) && dev.m_ordinal == ordinal)
      return dev;
  }
  assert("Required device not found"==0);
  return *(Device*)NULL;
}

const Device *Board::
findDevice(const char *name) const {
  for (DevicesIter di = m_devices.begin(); di != m_devices.end(); di++) {
    Device &dev = **di;
    if (!strcasecmp(name, dev.name()))
      return &dev;
  }
  return NULL;
}

ReqConnection::
ReqConnection()
  : m_port(NULL), m_rq_port(NULL), m_index(0), m_indexed(false) {
}
const char *ReqConnection::
parse(ezxml_t cx, Worker &w, Required &r) {
  const char *err;
  std::string port, to;
  if ((err = OE::checkAttrs(cx, "port", "signal", "to", "index", (void *)0)) ||
      (err = OE::checkElements(cx, NULL)) ||
      (err = OE::getRequiredString(cx, port, "port")) ||
      (err = OE::getRequiredString(cx, to, "to")) ||
      (err = OE::getNumber(cx, "index", &m_index, &m_indexed, 0, false)) ||
      (err = w.getPort(port.c_str(), m_port)) ||
      (err = r.m_type.getPort(to.c_str(), m_rq_port)))
    return err;
  if (m_rq_port->type != m_port->type)
    return OU::esprintf("Required worker port \"%s\" is not the same type", to.c_str());
  if (m_rq_port->master == m_port->master)
    return OU::esprintf("Required worker port \"%s\" has same role (master) as port \"%s\"",
			to.c_str(), port.c_str());
  if (m_rq_port->count > 1) {
    if (!m_indexed)
      return OU::esprintf("Required worker port \"%s\" has count > 1, index must be specified",
			  to.c_str());
    if (m_index >= m_rq_port->count)
      return OU::esprintf("Required worker port \"%s\" has count %zu, index (%zu) too high",
			  to.c_str(), m_rq_port->count, m_index);
  }      
  // FIXME: check signal compatibility...
  return NULL;
}

Required::
Required(const DeviceType &dt)
  : m_type(dt) {
}

const char *Required::
parse(ezxml_t rqx, Worker &w) {
  const char *err;
  std::string worker;
  if ((err = OE::checkAttrs(rqx, "worker", (void *)0)) ||
      (err = OE::checkElements(rqx, "connect", (void*)0)) ||
      (err = OE::getRequiredString(rqx, worker, "worker")))
    return err;
  for (ezxml_t cx = ezxml_cchild(rqx, "connect"); cx; cx = ezxml_next(cx)) {
    m_connections.push_back(ReqConnection());
    if ((err = m_connections.back().parse(cx, w, *this)))
      return err;
  }
  return NULL;
}

// This does instance parsing for instances of HdlDevice workers.
// There is no HdlInstance class, so it is done here for now.
const char *Worker::
parseInstance(Instance &i, ezxml_t x) {
  const char *err;
  for (ezxml_t sx = ezxml_cchild(x, "signal"); sx; sx = ezxml_next(sx)) {
    std::string name, base, external;
    size_t index;
    bool hasIndex;
    if ((err = OE::getRequiredString(sx, name, "name")) ||
	(err = OE::getRequiredString(sx, external, "external")) ||
	(err = decodeSignal(name, base, index, hasIndex)))
      return err;
    Signal *s = m_sigmap.findSignal(base);
    if (!s)
      return OU::esprintf("Worker \"%s\" of instance \"%s\" has no signal \"%s\"",
			  m_implName, i.name, name.c_str());
    assert(!hasIndex || s->m_width);
    if (external.length()) {
      bool single;
      if (i.m_extmap.findSignal(s, index, single))
	return OU::esprintf("Duplicate signal \"%s\" for worker \"%s\" instance \"%s\"",
			    name.c_str(), m_implName, i.name);
      size_t dummy;
      if (i.m_extmap.findSignal(external, dummy) && s->m_direction == Signal::OUT)
	return OU::esprintf("Multiple outputs drive external \"%s\" for worker \"%s\" "
			    "instance \"%s\"", external.c_str(), m_implName, i.name);
    }
    i.m_extmap.push_back(s, index, external, hasIndex);
  }
  return NULL;
}