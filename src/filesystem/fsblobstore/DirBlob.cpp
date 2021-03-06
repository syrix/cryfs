#include "DirBlob.h"
#include <cassert>

//TODO Remove and replace with exception hierarchy
#include "messmer/fspp/fuse/FuseErrnoException.h"

#include <messmer/blobstore/implementations/onblocks/utils/Math.h>
#include <messmer/cpp-utils/data/Data.h>
#include "MagicNumbers.h"
#include "../CryDevice.h"
#include "FileBlob.h"
#include "SymlinkBlob.h"

using std::vector;
using std::string;
using std::pair;
using std::make_pair;

using blobstore::Blob;
using blockstore::Key;
using cpputils::Data;
using cpputils::unique_ref;
using cpputils::make_unique_ref;
using boost::none;

namespace cryfs {
namespace fsblobstore {

DirBlob::DirBlob(unique_ref<Blob> blob, std::function<off_t (const blockstore::Key&)> getLstatSize) :
    FsBlob(std::move(blob)), _getLstatSize(getLstatSize), _entries(), _mutex(), _changed(false) {
  ASSERT(magicNumber() == MagicNumbers::DIR, "Loaded blob is not a directory");
  _readEntriesFromBlob();
}

DirBlob::~DirBlob() {
  std::unique_lock<std::mutex> lock(_mutex);
  _writeEntriesToBlob();
}

void DirBlob::flush() {
  std::unique_lock<std::mutex> lock(_mutex);
  _writeEntriesToBlob();
  baseBlob().flush();
}

unique_ref<DirBlob> DirBlob::InitializeEmptyDir(unique_ref<Blob> blob, std::function<off_t(const blockstore::Key&)> getLstatSize) {
  InitializeBlobWithMagicNumber(blob.get(), MagicNumbers::DIR);
  return make_unique_ref<DirBlob>(std::move(blob), getLstatSize);
}

void DirBlob::_writeEntriesToBlob() {
  if (_changed) {
    Data serialized = _entries.serialize();
    baseBlob().resize(1 + serialized.size());
    baseBlob().write(serialized.data(), 1, serialized.size());
    _changed = false;
  }
}

void DirBlob::_readEntriesFromBlob() {
  //No lock needed, because this is only called from the constructor.
  Data data = baseBlob().readAll();
  _entries.deserializeFrom(static_cast<uint8_t*>(data.data()) + 1, data.size() - 1);  // data+1/size-1 because the first byte is the magic number
}

void DirBlob::AddChildDir(const std::string &name, const Key &blobKey, mode_t mode, uid_t uid, gid_t gid) {
  AddChild(name, blobKey, fspp::Dir::EntryType::DIR, mode, uid, gid);
}

void DirBlob::AddChildFile(const std::string &name, const Key &blobKey, mode_t mode, uid_t uid, gid_t gid) {
  AddChild(name, blobKey, fspp::Dir::EntryType::FILE, mode, uid, gid);
}

void DirBlob::AddChildSymlink(const std::string &name, const blockstore::Key &blobKey, uid_t uid, gid_t gid) {
  AddChild(name, blobKey, fspp::Dir::EntryType::SYMLINK, S_IFLNK | S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH, uid, gid);
}

void DirBlob::AddChild(const std::string &name, const Key &blobKey,
    fspp::Dir::EntryType entryType, mode_t mode, uid_t uid, gid_t gid) {
  std::unique_lock<std::mutex> lock(_mutex);
  _entries.add(name, blobKey, entryType, mode, uid, gid);
  _changed = true;
}

const DirEntry &DirBlob::GetChild(const string &name) const {
  std::unique_lock<std::mutex> lock(_mutex);
  return _entries.get(name);
}

const DirEntry &DirBlob::GetChild(const Key &key) const {
  std::unique_lock<std::mutex> lock(_mutex);
  return _entries.get(key);
}

void DirBlob::RemoveChild(const Key &key) {
  std::unique_lock<std::mutex> lock(_mutex);
  _entries.remove(key);
  _changed = true;
}

void DirBlob::AppendChildrenTo(vector<fspp::Dir::Entry> *result) const {
  std::unique_lock<std::mutex> lock(_mutex);
  result->reserve(result->size() + _entries.size());
  for (const auto &entry : _entries) {
    result->emplace_back(entry.type, entry.name);
  }
}

off_t DirBlob::lstat_size() const {
  //TODO Why do dirs have 4096 bytes in size? Does that make sense?
  return 4096;
}

void DirBlob::statChild(const Key &key, struct ::stat *result) const {
  const auto &child = GetChild(key);
  //TODO Loading the blob for only getting the size of the file/symlink is not very performant.
  //     Furthermore, this is the only reason why DirBlob needs a pointer to _fsBlobStore, which is ugly
  result->st_mode = child.mode;
  result->st_uid = child.uid;
  result->st_gid = child.gid;
  //TODO If possible without performance loss, then for a directory, st_nlink should return number of dir entries (including "." and "..")
  result->st_nlink = 1;
  //TODO Handle file access times
  result->st_mtime = result->st_ctime = result->st_atime = 0;
  result->st_size = _getLstatSize(key);
  //TODO Move ceilDivision to general utils which can be used by cryfs as well
  result->st_blocks = blobstore::onblocks::utils::ceilDivision(result->st_size, (off_t)512);
  result->st_blksize = CryDevice::BLOCKSIZE_BYTES; //TODO FsBlobStore::BLOCKSIZE_BYTES would be cleaner
}

void DirBlob::chmodChild(const Key &key, mode_t mode) {
  std::unique_lock<std::mutex> lock(_mutex);
  _entries.setMode(key, mode);
  _changed = true;
}

void DirBlob::chownChild(const Key &key, uid_t uid, gid_t gid) {
  std::unique_lock<std::mutex> lock(_mutex);
  if(_entries.setUidGid(key, uid, gid)) {
    _changed = true;
  }
}

void DirBlob::setLstatSizeGetter(std::function<off_t(const blockstore::Key&)> getLstatSize) {
    std::unique_lock<std::mutex> lock(_mutex);
    _getLstatSize = getLstatSize;
}

cpputils::unique_ref<blobstore::Blob> DirBlob::releaseBaseBlob() {
  std::unique_lock<std::mutex> lock(_mutex);
  _writeEntriesToBlob();
  return FsBlob::releaseBaseBlob();
}

}
}
