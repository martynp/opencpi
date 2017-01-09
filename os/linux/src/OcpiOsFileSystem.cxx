
/*
 *  Copyright (c) Mercury Federal Systems, Inc., Arlington VA., 2009-2010
 *
 *    Mercury Federal Systems, Incorporated
 *    1901 South Bell Street
 *    Suite 402
 *    Arlington, Virginia 22202
 *    United States of America
 *    Telephone 703-413-0781
 *    FAX 703-413-0784
 *
 *  This file is part of OpenCPI (www.opencpi.org).
 *     ____                   __________   ____
 *    / __ \____  ___  ____  / ____/ __ \ /  _/ ____  _________ _
 *   / / / / __ \/ _ \/ __ \/ /   / /_/ / / /  / __ \/ ___/ __ `/
 *  / /_/ / /_/ /  __/ / / / /___/ ____/_/ / _/ /_/ / /  / /_/ /
 *  \____/ .___/\___/_/ /_/\____/_/    /___/(_)____/_/   \__, /
 *      /_/                                             /____/
 *
 *  OpenCPI is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenCPI is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with OpenCPI.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <cctype>
#include <ctime>
#include <cstdio>
#include "OcpiOsAssert.h"
#include "OcpiOsFileSystem.h"
#include "OcpiOsFileIterator.h"
#include "OcpiOsSizeCheck.h"
#include "OcpiOsPosixError.h"

/*
 * ----------------------------------------------------------------------
 * File Name Mapping
 * ----------------------------------------------------------------------
 */

namespace OCPI {
  namespace OS {
    namespace FileSystem {
/*
 * Unix file names are very close to our "abstract" file names. In fact,
 * our abstract names can be used as a native name. In the other direction,
 * we have to strip "." and ".."  path components.
 */

std::string
toNativeName (const std::string & name)
  throw (std::string)
{
  return name;
}

std::string
fromNativeName (const std::string & nativeName)
  throw (std::string)
{
  std::string name;
  std::string::size_type slashPos;

  if (nativeName.length() == 0) {
    throw std::string ("empty filename");
  }

  /*
   * Whether the nativeName may "back up" beyond the current directory
   * (using '..'). Only a relative name may do so, and at most once.
   */

  bool mayBackUp;

  /*
   * Process first path component
   */

  slashPos = nativeName.find ('/');

  if (slashPos == 0) {
    /*
     * Absolute name
     */

    name = '/';
    mayBackUp = false;
  }
  else {
    /*
     * Relative name
     */

    /*
     * See if the first (or only) path component is '.' or '..'
     */

    if ((slashPos == 1 ||
         (slashPos == std::string::npos && nativeName.length() == 1)) &&
        nativeName[0] == '.') {
      name = cwd ();
    }
    else if ((slashPos == 2 ||
              (slashPos == std::string::npos && nativeName.length() == 2)) &&
             nativeName[0] == '.' && nativeName[1] == '.') {
      std::string currentDir = cwd ();

      ocpiAssert (currentDir.length() && currentDir[0] == '/');

      if (currentDir.length() == 1) {
        throw std::string ("invalid file name '..' in top-level directory");
      }

      std::string::size_type lastSlashPos = currentDir.rfind ('/');
      name = currentDir.erase (lastSlashPos);
    }
    else if (slashPos != std::string::npos) {
      name = nativeName.substr (0, slashPos);
    }
    else if (slashPos == std::string::npos) {
      /*
       * File name has a single path component only
       */

      return nativeName;
    }

    mayBackUp = true;
  }

  /*
   * Process remaining path components. Name contains the first
   * path component, or '/', and slashPos points to the slash at
   * the end of the first path component (0 in the case of an
   * absolute name.
   */

  do {
    std::string::size_type oldSlashPos = slashPos;
    std::string::size_type newSlashPos;

    newSlashPos = slashPos = nativeName.find ('/', slashPos + 1);

    if (newSlashPos == std::string::npos) {
      newSlashPos = nativeName.length();
    }

    if (newSlashPos - oldSlashPos == 1) {
      /*
       * Ignore a slash at the end
       */
      if (slashPos == std::string::npos) {
        break;
      }

      throw std::string ("empty path component");
    }
    else if (newSlashPos - oldSlashPos == 2 &&
             nativeName[oldSlashPos+1] == '.') {
      /*
       * A path component of "." - ignore
       */
      continue;
    }
    else if (newSlashPos - oldSlashPos == 3 &&
             nativeName[oldSlashPos+1] == '.' &&
             nativeName[oldSlashPos+2] == '.') {
      /*
       * If the name is empty, and if it was a relative name, then
       * back up beyond the current working directory.
       */

      if (name.empty()) {
        if (!mayBackUp) {
          throw std::string ("unprocessible '..' path component");
        }

        name = cwd ();
        mayBackUp = false;
      }

      std::string::size_type lastSlashPos = name.rfind ('/');

      if (lastSlashPos == 0) {
        throw std::string ("unprocessible '..' path component");
      }
      else if (lastSlashPos == std::string::npos) {
        name.erase ();
      }
      else {
        name.erase (lastSlashPos);
      }

      continue;
    }

    /*
     * Concatenate new path component
     */

    if (name.length() && (name.length() != 1 || name[0] != '/')) {
      name += '/';
    }

    name += nativeName.substr (oldSlashPos + 1, newSlashPos - oldSlashPos - 1);
  }
  while (slashPos != std::string::npos);

  if (name.empty()) {
    /*
     * Refers to the current directory.
     */

    return cwd ();
  }

  return name;
}

/*
 * ----------------------------------------------------------------------
 * File Name Helpers
 * ----------------------------------------------------------------------
 */

std::string
joinNames (const std::string & dir,
                                const std::string & name)
  throw (std::string)
{
  ocpiAssert (name.length() > 0);

  if (name[0] == '/') {
    return name;
  }

  ocpiAssert (dir.length() > 0);

  std::string absName = dir;

  if (dir.length() != 1 || dir[0] != '/') {
    absName += '/';
  }

  absName += name;

  return absName;
}

std::string
absoluteName (const std::string & name)
  throw (std::string)
{
  return joinNames (cwd(), name);
}

std::string
directoryName (const std::string & name)
  throw (std::string)
{
  std::string::size_type slashPos = name.rfind ('/');

  if (slashPos == std::string::npos) {
    return cwd ();
  }

  if (slashPos == 0) {
    return "/";
  }

  return name.substr (0, slashPos);
}

std::string
relativeName (const std::string & name)
  throw (std::string)
{
  std::string::size_type slashPos = name.rfind ('/');

  if (slashPos == std::string::npos) {
    return name;
  }

  if (slashPos == 0 && name.length() == 1) {
    /*
     * The relative name of "/" is "/".
     */
    return "/";
  }

  return name.substr (slashPos + 1);
}

std::string
getPathElement (std::string & path,
                                     bool ignoreInvalid,
                                     char separator)
  throw (std::string)
{
  std::string::size_type colPos;
  std::string firstElement, res;

  if (!separator) {
    separator = ':';
  }

 again:
  colPos = path.find (separator);

  if (colPos == std::string::npos) {
    firstElement = path;
    path.clear ();
  }
  else {
    firstElement = path.substr (0, colPos);
    path = path.substr (colPos + 1);
  }

  if (firstElement.empty()) {
    if (path.empty()) {
      return res;
    }

    goto again;
  }

  try {
    res = fromNativeName (firstElement);
  }
  catch (const std::string &) {
    if (!ignoreInvalid) {
      throw;
    }

    goto again;
  }

  return res;
}

/*
 * ----------------------------------------------------------------------
 * Directory Management
 * ----------------------------------------------------------------------
 */

std::string
cwd ()
  throw (std::string)
{
  char buffer[1024];

  if (!::getcwd (buffer, 1024)) {
    throw Posix::getErrorMessage (errno);
  }

  return fromNativeName (buffer);
}

void
cd (const std::string & name)
  throw (std::string)
{
  std::string nativeName = toNativeName (name);

  if (::chdir (nativeName.c_str())) {
    throw Posix::getErrorMessage (errno);
  }
}

void
mkdir (const std::string & name, bool existsOk)
  throw (std::string)
{
  std::string nativeName = toNativeName (name);

  bool isDir;
  if (exists(name, &isDir)) {
    if (existsOk) {
      if (!isDir)
	throw "when creating directory \"" + name + "\", a non-directory exists";
      return;
    }
    throw "when creating directory \"" + name + "\", it already exists";
  }
  if (::mkdir(nativeName.c_str(), 0777))
    throw Posix::getErrorMessage(errno);
}

void
rmdir (const std::string & name)
  throw (std::string)
{
  std::string nativeName = toNativeName (name);

  if (::rmdir (nativeName.c_str())) {
    throw Posix::getErrorMessage (errno);
  }
}

/*
 * ----------------------------------------------------------------------
 * Directory Listing
 * ----------------------------------------------------------------------
 */

FileIterator
list (const std::string & dir,
                           const std::string & pattern)
  throw (std::string)
{
  return FileIterator (dir, pattern);
}

/*
 * ----------------------------------------------------------------------
 * File Information
 * ----------------------------------------------------------------------
 */

bool
exists(const std::string & name, bool * isDir, uint64_t *size, std::time_t *mtime,
       FileId *id)
  throw ()
{
  std::string nativeName = toNativeName (name); // Error: Exception thrown in function declared not to throw exceptions.

  struct stat info;
  if (stat (nativeName.c_str(), &info))
    return false;
  if ((info.st_mode & S_IFDIR) == S_IFDIR) {
    if (isDir)
      *isDir = true;
  } else if ((info.st_mode & S_IFREG) == S_IFREG || (info.st_mode & S_IFIFO) == S_IFIFO) {
    if (isDir)
      *isDir = false;
    if (size)
      *size = info.st_size;
  } else
    return false;
  if (mtime)
    *mtime = info.st_mtime;
  if (id) {
    struct PosixId {
      dev_t device;
      ino_t inode;  
    } *posix_id;
    ocpiAssert ((compileTimeSizeCheck<sizeof (id->m_opaque), sizeof(*posix_id)> ()));
    posix_id = (PosixId*)id->m_opaque;
    memset(id, 0, sizeof(FileId));
    posix_id->device = info.st_dev;
    posix_id->inode = info.st_ino;
  }
  return true;
}

unsigned long long
size (const std::string & name)
  throw (std::string)
{
  std::string nativeName = toNativeName (name);

  struct stat info;
  if (stat (nativeName.c_str(), &info)) {
    throw Posix::getErrorMessage (errno);
  }

  if ((info.st_mode & S_IFREG) != S_IFREG) {
    throw std::string ("not a regular file");
  }

  return info.st_size;
}

std::time_t
lastModified (const std::string & name)
  throw (std::string)
{
  std::string nativeName = toNativeName (name);

  struct stat info;
  if (stat (nativeName.c_str(), &info)) {
    throw Posix::getErrorMessage (errno);
  }

  return info.st_mtime;
}

/*
 * ----------------------------------------------------------------------
 * File System Operations
 * ----------------------------------------------------------------------
 */

void
rename (const std::string & srcName,
                             const std::string & destName)
  throw (std::string)
{
  std::string srcNativeName = toNativeName (srcName);
  std::string destNativeName = toNativeName (destName);

  if (::rename (srcNativeName.c_str(), destNativeName.c_str())) {
    throw Posix::getErrorMessage (errno);
  }
}

void
remove (const std::string & name)
  throw (std::string)
{
  std::string nativeName = toNativeName (name);

  if (unlink (nativeName.c_str())) {
    throw Posix::getErrorMessage (errno);
  }
}
const char *slashes = "/";

Dir::Dir(const std::string &dir) throw (std::string)
  : m_name(toNativeName(dir)) {
  
  ocpiAssert ((compileTimeSizeCheck<sizeof (m_opaque), sizeof(DIR *)> ()));
  DIR *dfd = opendir(m_name.c_str());
  if (dfd == NULL)
    throw OCPI::OS::Posix::getErrorMessage (errno);
  m_opaque = (intptr_t)dfd;
  }

Dir::~Dir() throw() {
  closedir((DIR *)m_opaque);
}

bool Dir::next(std::string &s, bool &isDir) throw(std::string) {
  struct dirent entry, *result;
  DIR *dfd = (DIR *)m_opaque;
  int errnum;
  do
    if ((errnum = readdir_r(dfd, &entry, &result)))
      throw "Error reading diretory: " + OCPI::OS::Posix::getErrorMessage(errnum);
  while (result && entry.d_name[0] == '.' &&
	 (!entry.d_name[1] || (entry.d_name[1] == '.' && !entry.d_name[2])));
  if (result) {
    s = entry.d_name;
    // isDir = entry.d_type == DT_DIR; // for BSD...
    exists(m_name + "/" + s, &isDir);
    return true;
  }
  return false;
}

}
}
}
